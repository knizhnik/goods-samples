#include <set>
#include "pgsql_storage.h"


// GOODS object identifier consists of 32-bit OPID and 16-bit STID.
// GOODS ORM for PostgreSQL only support work with one storage, so we will not store STID.
// But in  PostgreSQL we want to include in identifier object type information (CPID) to
// make it possible to detect object type without prior loading it.
// Right now we include CPID into 32-bit OPID for compatibility reasons.
// In future we are going to extend opid_t to 64-bits. There will be no more limitation 
// for maximal number of classes
#define MAX_CLASSES 256
#define GET_CID(x) ((x) % MAX_CLASSES)
#define GET_OID(x) ((x) / MAX_CLASSES)
#define MAKE_OPID(cid,oid) ((oid)*MAX_CLASSES + (cid))

#define MAX_KEY_SIZE 4096

class_descriptor* get_root_class(class_descriptor* desc)
{
	while (desc->base_class != &object::self_class) { 
		desc = desc->base_class;
	}
	return desc;
}

//
// We store all inherited classes in one table,
// so we need to find table for top class
//
inline std::string get_table(class_descriptor* desc)
{
	return std::string(get_root_class(desc)->name);
}


inline int get_inheritance_depth(class_descriptor* cls) {
	int depth;
	for (depth = 0; (cls = cls->base_class) != NULL; depth++);
	return depth;
}

static void get_columns(std::string const& prefix, field_descriptor* first, std::vector<std::string>& columns, int inheritance_depth)
{
	if (first == NULL) {
		return;
	}

	field_descriptor* field = first;
    do { 
		if (field->loc.type == fld_structure) { 
			get_columns(field == first && inheritance_depth > 1 ? prefix : prefix + field->name + ".", field->components, columns, field == first && inheritance_depth >  0 ? inheritance_depth-1 : 0);
		} else { 
			columns.push_back(prefix + field->name);
		}
        field = (field_descriptor*)field->next;
    } while (field != first);
}

static std::string get_host(std::string const& address)
{
	return address.substr(0, address.find(':'));
}

static std::string get_port(std::string const& address)
{
	return address.substr(address.find(':')+1);
}

static char const* map_type(field_descriptor* field)
{
	if (field->loc.n_items != 1) { // right now we support only char[] for varying part
		return "text";
	}
	switch (field->loc.type) { 
	case fld_unsigned_integer:
	case fld_signed_integer:
		switch (field->loc.size) { 
		case 1:
		case 2:
			return "smallint";
		case 4:
			return "integer";
		case 8:
			return "bigint";
		default:
			assert(false);
		}
	case fld_string:
		return "text";
	case fld_reference:
		return "bigint";
	case fld_raw_binary:
		return "bytea";
	case fld_real:
		switch (field->loc.size) { 
		case 4:
			return "real";
		case 8:
			return "double precision";
		default:
			assert(false);
		}
	default:
		assert(false);
	}
}

static void define_table_columns(std::set<std::string>& columns, std::string const& prefix, field_descriptor* first, std::stringstream& sql, bool ignore_inherited)
{
	if (first == NULL) {
		return;
	}

	field_descriptor* field = first;
	if (ignore_inherited) { 
		goto NextField;
	}
    do { 
		if (field->loc.type == fld_structure) { 
			define_table_columns(columns, prefix + field->name, field->components, sql, false);
		} else if (columns.find(field->name) == columns.end()) { 
			columns.insert(field->name);
			sql << ",\"" << prefix << field->name << "\" " << map_type(field);
		}
  	  NextField:
        field = (field_descriptor*)field->next;
    } while (field != first);
}

inline bool is_btree(std::string name)
{
	return name == "B_tree" || name == "SB_tree";
}
	
boolean pgsql_storage::open(char const* connection_address, const char* login, const char* password) 
{
	opid_buf_pos = OPID_BUF_SIZE;
	std::stringstream connStr;
	connStr << "host=" << get_host(connection_address)
			<< " port=" << get_port(connection_address);
	if (login != NULL && *login) { 
		connStr << " user=" << login;
	}
	if (password != NULL && *password) { 
		connStr << " password=" << password;
	}
	con = new connection(connStr.str()); // database name is expected to be equal to user name
	work txn(*con);	

	txn.exec("create extension if not exists external_file");

	txn.exec("create table if not exists dict_entry(owner bigint, key text, value bigint)");
	txn.exec("create table if not exists classes(cpid integer primary key, name text, descriptor bytea)");
	txn.exec("create table if not exists set_member(opid bigint primary key, next bigint, prev bigint, owner bigint, obj bigint, key bytea, skey bigint)");
	txn.exec("create table if not exists root_class(cpid integer)");

	txn.exec("create index if not exists dict_key_index on dict_entry(key)");
	txn.exec("create index if not exists dict_owner_index on dict_entry(owner)");
	txn.exec("create index if not exists set_member_owner on set_member(owner)");
	txn.exec("create index if not exists set_member_key on set_member(key)");
	txn.exec("create index if not exists set_member_skey on set_member(skey)");

	txn.exec("create sequence if not exists oid_sequence");
	txn.exec("create sequence if not exists cid_sequence minvalue 2"); // 1 is RAW_CPID

		
	con->prepare("new_oid", "select nextval('oid_sequence') from generate_series(1,$1)"); 
	con->prepare("new_cid", "select nextval('cid_sequence')"); 
	con->prepare("get_root", "select cpid from root_class"); 
	con->prepare("add_root", "insert into root_class values ($1)"); 
	con->prepare("set_root", "update root_class set cpid=$1"); 
	con->prepare("get_class", "select descriptor from classes where cpid=$1"); 
	con->prepare("put_class", "insert into classes (cpid,name,descriptor) values ($1,$2,$3)"); 
	con->prepare("change_class", "update classes set descriptor=$1 where cpid=$2"); 
	con->prepare("index_equal", "select * from set_member m where owner=$1 and key=$2 and not exists (select * from set_member p where p.oid=m.prev and p.owner=$1 and p.key=$2)");
	con->prepare("index_greater_or_equal", "select * from set_member where owner=$1 and key>=$2 limit 1");
	con->prepare("index_equal_skey", "select * from set_member m where owner=$1 and skey=$2 and not exists (select * from set_member p where p.oid=m.prev and p.owner=$1 and p.skey=$2)");
	con->prepare("index_greater_or_equal_skey", "select * from set_member where owner=$1 and skey>=$2 limit 1");
	con->prepare("index_drop", "delete from set_member where owner=$1");
	con->prepare("index_del", "delete from set_member where owner=$1 and opid=$2");

	con->prepare("hash_put", "insert into dict_entry (owner,name,value) values ($1,$2,$3)");
	con->prepare("hash_get", "select value from dict_entry where owner=$1 and name=$2");
	con->prepare("hash_delall", "delete from dict_entry where owner=$1 and name=$2");
	con->prepare("hash_del", "delete from dict_entry where owner=$1 and name=$2 and value=$3");
	con->prepare("hash_drop", "delete from dict_entry where owner=$1");
	con->prepare("hash_size", "select count(*) from dict_entry where owner=$1");
				
	con->prepare("write_file", "select writeEfile($1, $2)");
	con->prepare("read_file", "select readEfile($1)");

	for (size_t i = 0; i < DESCRIPTOR_HASH_TABLE_SIZE; i++) { 
		for (class_descriptor* cls = class_descriptor::hash_table[i]; cls != NULL; cls = cls->next) {
			if (cls == &object::self_class) { 
				continue;
			}
			std::vector<std::string> columns;
			class_descriptor* root_class = get_root_class(cls);
			std::string table_name(root_class->name);
			std::string class_name(cls->name);
			get_columns("", cls->fields, columns, get_inheritance_depth(cls));
			if (root_class == &set_member::self_class) { 
				((field_descriptor*)root_class->fields->prev)->flags |= fld_binary; // mark set_member::key as binary
			}
			if (cls->base_class == &set_member::self_class) { 
				columns.push_back("skey");
			}
			{
				std::stringstream sql;			
				sql << "insert into \"" << table_name << "\" (opid";
				for (size_t i = 0; i < columns.size(); i++) { 
					sql << ",\"" << columns[i] << '\"';
				}
				sql << ") values ($1";
				for (size_t i = 0; i < columns.size(); i++) { 
					sql << ",$" << (i+2);
				}
				sql << ")";
				con->prepare(class_name + "_insert", sql.str());
			}
			{
				std::stringstream sql;			
				sql << "update \"" << table_name << "\" set ";
				for (size_t i = 0; i < columns.size(); i++) { 
					if (i != 0) sql << ',';
					sql << '\"' << columns[i] << "\"=$" << (i+2);
				}
				sql << " where opid=$1";
				con->prepare(class_name + "_update", sql.str());
			}
			if (table_name == class_name) {
				std::stringstream sql;
				sql << "create table if not exists \"" << class_name << "\"(opid bigint primary key";
				std::set<std::string> columns;
				define_table_columns(columns, "", cls->fields, sql, false);

				// for all derived classes
				for (size_t j = 0; j < DESCRIPTOR_HASH_TABLE_SIZE; j++) { 
					for (class_descriptor* derived = class_descriptor::hash_table[j]; derived != NULL; derived = derived->next) {
						if (derived != cls && !is_btree(derived->name)) {
							for (class_descriptor* base = derived->base_class; base != NULL; base = base->base_class) { 
								if (base == cls) { 
									define_table_columns(columns, "", derived->fields, sql, true);
									break;
								}
							}
						}
					}
				}													
				sql << ")";
				printf("%s\n", sql.str().c_str());
				txn.exec(sql.str());			   

				con->prepare(table_name + "_delete", std::string("delete from \"") + table_name + "\" where opid=$1");
				con->prepare(table_name + "_loadobj", std::string("select * from \"") + table_name + "\" where opid=$1");
				con->prepare(table_name + "_loadset", std::string("with recursive set_members(opid,obj) as (select m.opid,m.obj from set_member m where m.prev=$1 union all select m.opid,m.obj from set_member m join set_members s ON m.prev=s.opid) select s.opid as mbr_opid,s.next as mbr_next,s.prev as mbr_prev,s.owner as mbr_owner,s.obj as mbr_obj,s.key as mbr_key,s.skey as mbr_skey,t.* from set_members s, \"") + table_name + "\" t where t.opid=s.obj limit $2");

			}
		}	
	}	
	txn.commit();
	return true;
}

void pgsql_storage::close()
{
	delete con;
}



void pgsql_storage::bulk_allocate(size_t sizeBuf[], cpid_t cpidBuf[], size_t nAllocObjects, 
								  opid_t opid_buf[], size_t nReservedOids, hnd_t clusterWith[])
{
	// Bulk allocate at obj_storage level is disabled 
	assert(false);
}

opid_t pgsql_storage::allocate(cpid_t cpid, size_t size, int flags, opid_t clusterWith)
{
	if (opid_buf_pos == OPID_BUF_SIZE) { 
		result rs = txn->prepared("new_oid")(OPID_BUF_SIZE).exec();
		for (size_t i = 0; i < OPID_BUF_SIZE; i++) { 
			opid_buf[i] = rs[i][0].as(opid_t());
		}
		opid_buf_pos = 0;
	}
	return MAKE_OPID(cpid, opid_buf[opid_buf_pos++]);
}

void pgsql_storage::deallocate(opid_t opid)
{ 
	class_descriptor* desc = lookup_class(GET_CID(opid));
	txn->prepared(get_table(desc) + "_delete")(opid).exec();
}

boolean pgsql_storage::lock(opid_t opid, lck_t lck, int attr)
{
	return true;
}

void pgsql_storage::unlock(opid_t opid, lck_t lck)
{
}

void pgsql_storage::get_class(cpid_t cpid, dnm_buffer& buf)
{
	assert(cpid != RAW_CPID);		
	result rs = txn->prepared("get_class")(cpid).exec();
	assert(rs.size() == 1);
	binarystring desc = binarystring(rs[0][0]);
	memcpy(buf.put(desc.size()), desc.data(), desc.size());
}

cpid_t pgsql_storage::put_class(dbs_class_descriptor* dbs_desc)
{
	size_t dbs_desc_size = dbs_desc->get_size();	
	std::string name(dbs_desc->name());
	std::string buf((char*)dbs_desc, dbs_desc_size);	
	cpid_t cpid;
	{
		result rs = txn->prepared("new_cid").exec();
		assert(rs.size() == 1);
		cpid = rs[0][0].as(cpid_t());
	}
	((dbs_class_descriptor*)buf.data())->pack();
	txn->prepared("put_class")(cpid)(name)(txn->esc_raw(buf)).exec();	
	return cpid;
}

void pgsql_storage::change_class(cpid_t cpid, 
								 dbs_class_descriptor* dbs_desc)
{
	size_t dbs_desc_size = dbs_desc->get_size();
	std::string buf((char*)dbs_desc, dbs_desc_size);
	((dbs_class_descriptor*)buf.data())->pack();
	txn->prepared("change_class")(txn->esc_raw(buf))(cpid).exec();
}
	
																				 
void pgsql_storage::load(opid_t opid, int flags, dnm_buffer& buf)
{
	load(&opid, 1, flags, buf);
}

class_descriptor* pgsql_storage::lookup_class(cpid_t cpid)
{
	class_descriptor* desc = cpid < descriptor_table.size() ? descriptor_table[cpid] : NULL;
	if (desc == NULL) { 
		dnm_buffer cls_buf;
		get_class(cpid, cls_buf);
		dbs_class_descriptor* dbs_desc = (dbs_class_descriptor*)&cls_buf;
		dbs_desc->unpack();
		desc = class_descriptor::find(dbs_desc->name());
		assert(desc != NULL);
		assert(*desc->dbs_desc == *dbs_desc);
		if (descriptor_table.size() <= cpid) { 
			descriptor_table.resize(cpid+1);
		}
		descriptor_table[cpid] = desc;
	}
	return desc;
}

static size_t unpack_struct(std::string const& prefix, field_descriptor* first, 
							dnm_buffer& buf, result::tuple const& record, size_t n_refs)
{
	if (first == NULL) {
		return n_refs;
	}

	field_descriptor* field = first;
    do { 
		result::tuple::reference col = record[prefix + field->name];
		assert(!col.is_null() || field->loc.type == fld_string);
		if (field->loc.n_items != 1) { 
			assert(field->loc.type == fld_signed_integer && field->loc.size == 1);
			if (field->flags & fld_binary) {
				binarystring blob(col);
				if (field->loc.n_items == 0) { 
					char* dst = buf.append(blob.size());
					memcpy(dst, blob.data(), blob.size());
				} else { 
					char* dst = buf.append(field->loc.n_items);
					if ((size_t)field->loc.n_items <= blob.size()) { 
						memcpy(dst, blob.data(), field->loc.n_items);
					} else { 
						memcpy(dst, blob.data(), blob.size());
						memset(dst + blob.size(), 0, field->loc.n_items - blob.size());
					}
				}
			} else { 
				std::string str = col.as(std::string());
				if (field->loc.n_items == 0) { 
					char* dst = buf.append(str.size());
					memcpy(dst, str.data(), str.size());
				} else { 
					char* dst = buf.append(field->loc.n_items);
					if ((size_t)field->loc.n_items <= str.size()) { 
						memcpy(dst, str.data(), field->loc.n_items);
					} else { 
						memcpy(dst, str.data(), str.size());
						memset(dst + str.size(), 0, field->loc.n_items - str.size());
					}
				}
			}
		} else {
			switch(field->loc.type) { 
			  case fld_reference:
			  {
				  char* dst = (char*)((dbs_reference_t*)&buf + n_refs++);
				  opid_t opid = col.as(opid_t());
				  packref(dst, 0, opid);
				  break;			  
			  }
			  case fld_string:
			  {
				  if (col.is_null()) { 
					  char* dst = buf.append(2);
					  pack2(dst, 0xFFFF);
				  } else {
					  std::string str = col.as(std::string());
					  wstring_t wstr(str.c_str());
					  char* dst = buf.append((1 + wstr.length())*2);
					  dst = pack2(dst, wstr.length());
					  for (int i = 0; i < wstr.length(); i++) { 
						  dst = pack2(dst, (nat2)wstr[i]);
					  }
				  }
				  break;
			  }
			  case fld_raw_binary:
			  {
				  binarystring blob(col);
				  char* dst = buf.append(4 + blob.size());
				  dst = pack4(dst, blob.size());
				  memcpy(dst, blob.data(), blob.size());
				  break;
			  }
			  case fld_signed_integer:
			  case fld_unsigned_integer:
				switch (field->loc.size) { 
				  case 1:				
					*buf.append(1) = (char)col.as(int());
					break;
				  case 2:
					pack2(buf.append(2), col.as(int2()));
					break;
				  case 4:
					pack4(buf.append(4), col.as(int4()));
					break;
				  case 8:
				  {
					  int8 ival = col.as(int8());					  
					  pack8(buf.append(8), (char*)&ival);
					  break;
				  }
				  default:
					assert(false);
				}
				break;
			  case fld_real:
				switch (field->loc.size) { 
				  case 4:
				  {
					  union {
						  float f;
						  int4  i;
					  } u;
					  u.f = col.as(float());
					  pack4(buf.append(4), u.i); 
					  break;
				  }
				  case 8:
				  {
					  double f = col.as(double());
					  pack8(buf.append(8), (char*)&f); 
					  break;
				  }
				  default:
					assert(false);
				}
				break;
			  case fld_structure:
				n_refs = unpack_struct(prefix + field->name + ".", field->components, buf, record, n_refs);
				break;
			  default:
				assert(false);
			}
		}
        field = (field_descriptor*)field->next;
    } while (field != first);

	return n_refs;
}

void pgsql_storage::unpack_object(std::string const& prefix, class_descriptor* desc, dnm_buffer& buf, result::tuple const& record)
{
	size_t hdr_offs = buf.size();
	dbs_object_header* hdr = (dbs_object_header*)buf.append(sizeof(dbs_object_header) + desc->n_fixed_references*sizeof(dbs_reference_t)); 
	opid_t opid = record[prefix + "opid"].as(opid_t());
	hdr->set_opid(opid);		
	hdr->set_cpid(GET_CID(opid));
	hdr->set_sid(0);
	hdr->set_flags(0);
                
	if (desc == &ExternalBlob::self_class) { 
		result rs = txn->prepared("read_file");
		assert(rs.size() == 1);
		binarystring blob(rs[0][0]);
		memcpy(buf.append(blob.size()), blob.data(), blob.size());
	} else { 		
		size_t n_refs = unpack_struct(prefix, desc->fields, buf, record, 0);
		assert(n_refs == desc->n_fixed_references);
	}
	hdr = (dbs_object_header*)(&buf + hdr_offs);
	hdr->set_size(buf.size() - hdr_offs - sizeof(dbs_object_header));

	if (desc == &set_member::self_class && prefix.size() == 0) { 
		opid_t mbr_obj_opid;
		stid_t mbr_obj_sid;
		// unpack referene of set_member::obj
		unpackref(mbr_obj_sid, mbr_obj_opid, &buf + hdr_offs + sizeof(dbs_object_header) + sizeof(dbs_reference_t)*3);
		cpid_t cpid = GET_CID(mbr_obj_opid);
		std::string table_name = get_table(lookup_class(cpid));
		result rs = txn->prepared(table_name  + "loadset")(opid)(max_preloaded_set_members).exec();
		load_query_result(rs, buf);
	}
}

opid_t pgsql_storage::load_query_result(result& rs, dnm_buffer& buf)
{
	opid_t next_mbr = 0;
	for (result::const_iterator i = rs.begin(); i != rs.end(); ++i) {
		result::tuple obj_record = *i;
		opid_t obj_opid = obj_record["opid"].as(opid_t());
		class_descriptor* obj_desc = lookup_class(GET_CID(obj_opid));
		size_t mbr_buf_offs = buf.size();
		unpack_object("mbr_", &set_member::self_class, buf, obj_record);
		stid_t next_sid;
		unpackref(next_sid, next_mbr, &buf + mbr_buf_offs + sizeof(dbs_object_header));
		unpack_object("", obj_desc, buf, obj_record); 
	}
	return next_mbr;
}



void pgsql_storage::query(opid_t& next_mbr, char const* query, nat4 buf_size, int flags, nat4 max_members, dnm_buffer& buf)
{
	pgsql_session session(this);
	load(next_mbr, flags, buf);
	stid_t sid;
	opid_t opid;
	unpackref(sid, opid, &buf + sizeof(dbs_object_header) + 3*sizeof(dbs_reference_t));
	cpid_t cpid = GET_CID(opid);
	class_descriptor* desc = lookup_class(cpid);
	std::string table_name = get_table(desc);
	std::stringstream sql;
	sql << "with recursive set_members(opid,obj) as (select m.opid,m.obj from set_member m where m.oipd=" << opid << " union all select m.opid,m.obj from set_member m join set_members s ON m.prev=s.opid) select s.opid as mbr_opid,s.next as mbr_next,s.prev as mbr_prev,s.owner as mbr_owner,s.obj as mbr_obj,s.key as mbr_key,s.skey as mbr_skey,t.* from set_members s, " << table_name << " t where t.opid=s.obj and " << query << " limit " << max_members;
	result rs = txn->exec(sql.str());
	buf.put(0); // reset buffer
	next_mbr = load_query_result(rs, buf);
	session.commit();
}

void pgsql_storage::load(opid_t* opp, int n_objects, 
						 int flags, dnm_buffer& buf)
{
	pgsql_session session(this);
	for (int i = 0; i < n_objects; i++) { 
		opid_t opid = opp[i];
		cpid_t cpid = GET_CID(opid);
		if (opid == ROOT_OPID) { 
			result rs = txn->prepared("get_root").exec();
			if (rs.empty()) { 
				dbs_object_header* hdr = (dbs_object_header*)buf.append(sizeof(dbs_object_header));
				hdr->set_opid(ROOT_OPID);		
				hdr->set_cpid(RAW_CPID);
				hdr->set_sid(id);
				hdr->set_flags(0);
				hdr->set_size(0);
				continue;
			}
			assert(rs.size() == 1);
			cpid = (cpid_t)rs[0][0].as(int());
		}
		class_descriptor* desc = lookup_class(cpid);		
		result rs = txn->prepared(get_table(desc) + "_loadobj")(opid).exec();
		assert(rs.size() == 1);
		unpack_object("", desc, buf, rs[0]);
	}
	session.commit();
}

void pgsql_storage::forget_object(opid_t opid)
{
}


void pgsql_storage::throw_object(opid_t opid)
{
}

void pgsql_storage::begin_transaction(dnm_buffer& buf)
{
	buf.put(0);
	assert(txn == NULL && con != NULL);
	txn = new work(*con);
}

size_t pgsql_storage::store_struct(field_descriptor* first, invocation& stmt, char* &src_refs, char* &src_bins, size_t size)
{
	if (first == NULL) {
		return size;
	}

	field_descriptor* field = first;	
    do { 
		if (field->loc.n_items != 1) { 
			assert(field->loc.type == fld_signed_integer && field->loc.size == 1);
			if (field->loc.n_items == 0) {
				std::string val(src_bins, size);
				assert((field->flags & fld_binary) != 0 || val[0] >= 0);
				stmt((field->flags & fld_binary) ? txn->esc_raw(val) : val);
				src_bins += size;
				size = 0;
			} else { 
				std::string val(src_bins, field->loc.n_items);
				stmt((field->flags & fld_binary) ? txn->esc_raw(val) : val);
				src_bins += field->loc.n_items;
				size -= field->loc.n_items;
			}
		} else {
			switch(field->loc.type) { 
			  case fld_reference:
			  {
				  opid_t opid;
				  stid_t sid;
				  src_refs = unpackref(sid, opid, src_refs);
				  size -= sizeof(dbs_reference_t);
				  stmt(opid);
				  break;			  
			  }
			  case fld_string:
			  {
				  size_t len = unpack2(src_bins);
				  src_bins += 2;
				  size -= 2;
				  if (len != 0xFFFF) { 
					  std::vector<wchar_t> buf(len+1);
					  for (size_t i = 0; i < len; i++) { 
						  buf[i] = unpack2(src_bins);
						  src_bins += 2;
						  size -= 2;
					  }
					  buf[len] = 0;
					  wstring_t wstr(&buf[0]);
					  char* chars = wstr.getChars();
					  assert(chars != NULL);
					  stmt(chars);
					  delete[] chars;
				  } else {
					  stmt(); // store NULL
				  }
				  break;
			  }
			  case fld_raw_binary:
			  {
				  size_t len = unpack4(src_bins);
				  std::string blob(src_bins+4, len);
				  stmt(txn->esc_raw(blob));
				  src_bins += 4 + len;
				  size -= 4 + len;
				  break;
			  }
			  case fld_signed_integer:
				switch (field->loc.size) { 
				  case 1:	
					stmt((int2)*src_bins++);
					size -= 1;
					break;
				  case 2:
					stmt((int2)unpack2(src_bins));
					src_bins += 2;
					size -= 2;
					break;
				  case 4:
					stmt((int4)unpack4(src_bins));
					src_bins += 4;
					size -= 4;
					break;
				  case 8:
				  {
					  int8 ival;
					  src_bins = unpack8((char*)&ival, src_bins);
					  stmt(ival);
					  size -= 8;
					  break;
				  }
				  default:
					assert(false);
				}
				break;
			  case fld_unsigned_integer:
				switch (field->loc.size) { 
				  case 1:	
					stmt((nat2)*src_bins++);
					size -= 1;
					break;
				  case 2:
					stmt(unpack2(src_bins));
					src_bins += 2;
					size -= 2;
					break;
				  case 4:
					stmt(unpack4(src_bins));
					src_bins += 4;
					size -= 4;
					break;
				  case 8:
				  {
					  nat8 ival;
					  src_bins = unpack8((char*)&ival, src_bins);
					  stmt(ival);
					  size -= 8;
					  break;
				  }
				  default:
					assert(false);
				}
				break;
			  case fld_real:
				switch (field->loc.size) { 
				  case 4:
				  {					  
					  union {
						  float f;
						  int4  i;
					  } u;
					  u.i = unpack4(src_bins);
					  stmt(u.f);
					  src_bins += 4;
					  size -= 4;
					  break;
				  }
				  case 8:
				  {
					  double f;
					  src_bins = unpack8((char*)&f, src_bins);
					  stmt(f);
					  size -= 8;
					  break;
				  }
				  default:
					assert(false);
				}
				break;
			  case fld_structure:
				size = store_struct(field->components, stmt, src_refs, src_bins, size);
				break;
			  default:
				assert(false);
			}
		}
        field = (field_descriptor*)field->next;
    } while (field != first);
	
	return size;
}	

boolean pgsql_storage::commit_coordinator_transaction(int n_trans_servers,
													  stid_t*trans_servers,
													  dnm_buffer& buf, 
													  trid_t& tid)
{
	assert(n_trans_servers == 1);
	assert(txn != NULL);
	char* ptr = &buf;
	char* end = ptr + buf.size();
	while (ptr < end) { 
		dbs_object_header* hdr = (dbs_object_header*)ptr;
		opid_t opid = hdr->get_opid();
		cpid_t cpid = hdr->get_cpid();		
		assert(cpid != RAW_CPID);
		if (opid == ROOT_OPID) { 
			result rs = txn->prepared("get_root").exec();
			if (rs.empty()) { 
				txn->prepared("add_root")(cpid).exec();
			} else { 
				txn->prepared("set_root")(cpid).exec();
			}
		}			
		int flags = hdr->get_flags();
		class_descriptor* desc = lookup_class(cpid);
		assert(opid != 0);
		char* src_refs = (char*)(hdr+1);
		char* src_bins = src_refs + desc->n_fixed_references*sizeof(dbs_reference_t);
		if (desc == &ExternalBlob::self_class) { 
			std::string blob(src_bins, hdr->get_size());
			txn->prepared("store_file")(opid)(txn->esc_raw(blob));
		} else { 	
			invocation stmt = txn->prepared(std::string(desc->name) + ((flags & tof_new) ? "_insert" : "_update"));
			stmt(opid);
			
			size_t left = store_struct(desc->fields, stmt, src_refs, src_bins, hdr->get_size());
			assert(left == 0);
			if (desc->base_class == &set_member::self_class) { 
				int64_t skey = 0; 
				switch (hdr->get_size() - sizeof(dbs_reference_t)*4) { 
				  case 4:
					skey = unpack4((char*)(hdr+1) + sizeof(dbs_reference_t)*4);
					break;
				  case 8:
					unpack8((char*)&skey, (char*)(hdr+1) + sizeof(dbs_reference_t)*4);
					break;
				}
				stmt(skey);
			}
		}
		stmt.exec();
		ptr = (char*)(hdr + 1) + hdr->get_size();
	}
	txn->commit();
	delete txn;
	txn = NULL;
	return true;
}
	
void pgsql_storage::commit_transaction(stid_t      coordinator, 
									   int         n_trans_servers,
									   stid_t*     trans_servers,
									   dnm_buffer& buf, 
									   trid_t      tid)
{
	assert(false);
}

boolean pgsql_storage::wait_global_transaction_completion()
{
	return false;
}

void pgsql_storage::send_messages() {}
void pgsql_storage::send_message(int message) {}
void pgsql_storage::push_message(int message) {}
	
nat8 pgsql_storage::get_used_size()
{
	work txn(*con);
	result rs = txn.exec("select pg_database_size(current_database())");	
	return rs[0][0].as(nat8());
}

boolean pgsql_storage::start_gc()
{
	return false;
}

void pgsql_storage::add_user(char const* login, char const* password){}

void pgsql_storage::del_user(char const* login){}

inline pgsql_storage* get_storage(object const* obj) 
{
	return (pgsql_storage*)obj->get_handle()->storage->storage;
}

invocation pgsql_storage::statement(char const* name)
{
	return txn->prepared(name);
}

ref<set_member> pgsql_storage::index_find(database const* db, opid_t index, char const* op, std::string const& key)
{
	pgsql_session session(this);
	result rs = txn->prepared(op)(index)(txn->esc_raw(key)).exec();
	size_t size = rs.size();
	if (size == 0) { 
		return NULL;
	}
	assert(size == 1);
	result::tuple record = rs[0];
	opid_t opid = record[0].as(opid_t());
	ref<set_member> mbr;
	((database*)db)->get_object(mbr, opid);
	return mbr;
}


//
// Emulation of GOODS B_tree
// 

REGISTER(pgsql_index, B_tree, pessimistic_repeatable_read_scheme);

field_descriptor& pgsql_index::describe_components()
{
    return NO_FIELDS;
}

ref<set_member> pgsql_index::find(const char* str, size_t len, skey_t key) const
{
	pgsql_storage* pg = get_storage(this);
	return pg->index_find(get_database(), hnd->opid, "index_equal", std::string(str, len));
}

ref<set_member> pgsql_index::find(const char* str) const
{
	pgsql_storage* pg = get_storage(this);
	return pg->index_find(get_database(), hnd->opid, "index_equal", std::string(str));
}

ref<set_member> pgsql_index::find(skey_t key) const
{
	pgsql_storage* pg = get_storage(this);
	std::stringstream buf;			
	buf << key;
	return pg->index_find(get_database(), hnd->opid, "index_equal_skey", buf.str());
}

ref<set_member> pgsql_index::findGE(skey_t key) const
{
	pgsql_storage* pg = get_storage(this);
	std::stringstream buf;			
	buf << key;
	return pg->index_find(get_database(), hnd->opid, "index_greater_or_equal_skey", buf.str());
}

ref<set_member> pgsql_index::findGE(const char* str, size_t len, skey_t key) const
{
	pgsql_storage* pg = get_storage(this);
	return pg->index_find(get_database(), hnd->opid, "index_greater_or_equal", std::string(str, len));
}
	
ref<set_member> pgsql_index::findGE(const char* str) const
{
	pgsql_storage* pg = get_storage(this);
	return pg->index_find(get_database(), hnd->opid, "index_greater_or_equal", std::string(str));
}


void pgsql_index::insert(ref<set_member> mbr)
{
	ref<set_member> next;
	if (&mbr->cls != &set_member::self_class) {
		next = findGE(mbr->get_key());
	} else { 
		char key[MAX_KEY_SIZE];
		size_t keySize = mbr->copyKeyTo(key, MAX_KEY_SIZE);
		assert(keySize < MAX_KEY_SIZE);
		next = findGE(key, keySize, 0); // skey is not used here
	}
	if (next.is_nil()) { 
		put_last(mbr); 
	} else {
		put_before(next, mbr);
	}
}

void pgsql_index::remove(ref<set_member> mbr)
{
	pgsql_storage* pg = get_storage(this);
	pgsql_storage::pgsql_session session(pg);
	pg->statement("index_del")(hnd->opid)(mbr->get_handle()->opid).exec();	
	set_owner::remove(mbr);
	session.commit();
}

void pgsql_index::clear()
{
	pgsql_storage* pg = get_storage(this);
	pgsql_storage::pgsql_session session(pg);
	pg->statement("index_drop")(hnd->opid).exec();	
    last = NULL;
    first = NULL;
	n_members = 0;
	obj = NULL;
	session.commit();
}

//
// Emulation of GOODS hash_table 
//

REGISTER(pgsql_dictionary, dictionary, pessimistic_repeatable_read_scheme);

field_descriptor& pgsql_dictionary::describe_components()
{
    return NO_FIELDS;
}

void pgsql_dictionary::put(const char* name, anyref obj)
{
	pgsql_storage* pg = get_storage(this);
	hnd_t obj_hnd = obj->get_handle();
	if (obj_hnd->opid == 0) { 
		object_handle::make_persistent(obj_hnd, hnd->storage);
	}
	pgsql_storage::pgsql_session session(pg);
	pg->statement("hash_put")(hnd->opid)(name)(obj_hnd->opid).exec();	
	session.commit();
}

anyref pgsql_dictionary::get(const char* name) const
{
	pgsql_storage* pg = get_storage(this);
	pgsql_storage::pgsql_session session(pg);
	result rs = pg->statement("hash_get")(hnd->opid)(name).exec();
	anyref ref;
	if (rs.empty()) { 
		return NULL;
	}
	((database*)get_database())->get_object(ref, rs[0][0].as(opid_t()), 0);
	session.commit();
	return ref;
}

boolean pgsql_dictionary::del(const char* name) 
{
	pgsql_storage* pg = get_storage(this);
	pgsql_storage::pgsql_session session(pg);
	result rs = pg->statement("hash_delall")(hnd->opid)(name).exec();
	boolean found = rs.affected_rows() != 0;
	session.commit();
	return found;
}

boolean pgsql_dictionary::del(const char* name, anyref obj) 
{
	pgsql_storage* pg = get_storage(this);
	hnd_t obj_hnd = obj->get_handle();
	if (obj_hnd == NULL) { 
		return false;
	}
	pgsql_storage::pgsql_session session(pg);
	result rs = pg->statement("hash_del")(hnd->opid)(name)(obj_hnd->opid).exec();
	bool found = rs.affected_rows() != 0;
	session.commit();
	return found;
}

anyref pgsql_dictionary::apply(hash_item::item_function) const
{
	assert(false);
}

void pgsql_dictionary::reset()
{
	pgsql_storage* pg = get_storage(this);
	pgsql_storage::pgsql_session session(pg);
	pg->statement("hash_drop")(hnd->opid).exec();
	session.commit();
}

size_t pgsql_dictionary::get_number_of_elements() const
{
	pgsql_storage* pg = get_storage(this);
	pgsql_storage::pgsql_session session(pg);
	result rs = pg->statement("hash_size")(hnd->opid).exec();
	assert(rs.size() == 1);
	int64_t n_elems = rs[0][0].as(int64_t());
	session.commit();
	return n_elems;
}
