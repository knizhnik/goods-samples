#include <set>
#include "pgsql_storage.h"


#define MAX_KEY_SIZE 4096

class_descriptor* get_root_class(class_descriptor* desc)
{
	while (desc->base_class != &object::self_class 
		   && !(desc->class_attr & class_descriptor::cls_hierarchy_root) 
		   && !(desc->base_class->class_attr & class_descriptor::cls_hierarchy_super_root)) 
	{ 
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
		switch (field->loc.type) { 
		  case fld_unsigned_integer:
			switch (field->loc.size) {
			  case 1:
				return "bytea";
			  case 2:
				return "smallint[]";
			  case 4:
				return "integer[]";
			  case 8:
				return "bigint[]";
			  default:
				assert(false);
			}
			break;
		  case fld_signed_integer:
			switch (field->loc.size) {
			  case 1:
				return "text";
			  case 2:
				return "smallint[]";
			  case 4:
				return "integer[]";
			  case 8:
				return "bigint[]";
			  default:
				assert(false);
			}
			break;
		  case fld_real:
			switch (field->loc.size) { 
			  case 4:
				return "real[]";
			  case 8:
				return "double precision[]";
			  default:
				assert(false);
			}
			break;
		  case fld_reference:
			return "objrefs";
		  default:
			assert(false);
		}
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
		return "objref";
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

static void define_table_columns(std::set<std::string>& columns, std::string const& prefix, field_descriptor* first, std::stringstream& sql, int inheritance_depth)
{
	if (first == NULL) {
		return;
	}

	field_descriptor* field = first;
	if (inheritance_depth < 0) { 
		goto NextField;
	}
    do { 
		if (field->loc.type == fld_structure) { 
			define_table_columns(columns, field == first && inheritance_depth > 1 ? prefix : prefix + field->name + ".", field->components, sql, field == first && inheritance_depth >  0 ? inheritance_depth-1 : 0);
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
	
	try {
		work txn(*con);	
		txn.exec("create domain objref as bigint");
		txn.exec("create domain objrefs as bigint[]");
		txn.commit();
	} catch (pqxx_exception const&x) {} // ignore error if domain alreqady exists

	work txn(*con);	

	txn.exec("create extension if not exists external_file");

	txn.exec("create table if not exists dict_entry(owner objref, name text, value objref, primary key(owner,name))");
	txn.exec("create table if not exists classes(cpid integer primary key, name text, descriptor bytea)");
	txn.exec("create table if not exists set_member(opid objref primary key, next objref, prev objref, owner objref, obj objref, key bytea)");
	txn.exec("create table if not exists root_class(cpid integer)");

	txn.exec("create index if not exists set_member_owner on set_member(owner)");
	txn.exec("create index if not exists set_member_key on set_member(key)");
	txn.exec("create index if not exists set_member_prev on set_member(prev)");

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
	con->prepare("index_greater_or_equal", "select * from set_member where owner=$1 and key>=$2 order by key limit 1");
	con->prepare("index_drop", "update set_member set owner=0 where owner=$1");
	con->prepare("index_del", "update set_member set owner=0 where owner=$1 and opid=$2");

	con->prepare("hash_put", "insert into dict_entry (owner,name,value) values ($1,$2,$3) "
				 "on conflict (owner,name) do update set value=$3 where dict_entry.owner=$1 and dict_entry.name=$2");
	con->prepare("hash_get", "select value from dict_entry where owner=$1 and name=$2");
	con->prepare("hash_delall", "delete from dict_entry where owner=$1 and name=$2");
	con->prepare("hash_del", "delete from dict_entry where owner=$1 and name=$2 and value=$3");
	con->prepare("hash_drop", "delete from dict_entry where owner=$1");
	con->prepare("hash_size", "select count(*) from dict_entry where owner=$1");
				
	con->prepare("write_file", "select writeEfile($1, $2)");
	con->prepare("read_file", "select readEfile($1)");

	for (size_t i = 0; i < DESCRIPTOR_HASH_TABLE_SIZE; i++) { 
		for (class_descriptor* cls = class_descriptor::hash_table[i]; cls != NULL; cls = cls->next) {
			if (cls == &object::self_class || cls->mop->is_transient() || (cls->class_attr & class_descriptor::cls_non_relational) || (cls->constructor == NULL && (cls->class_attr & class_descriptor::cls_hierarchy_super_root))) { 
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
			if (columns.size() != 0) {
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
				sql << "create table if not exists \"" << class_name << "\"(opid objref primary key";
				std::set<std::string> columns;
				define_table_columns(columns, "", cls->fields, sql, get_inheritance_depth(cls));

				// for all derived classes
				if (!(cls->class_attr & class_descriptor::cls_hierarchy_super_root)) {
					for (size_t j = 0; j < DESCRIPTOR_HASH_TABLE_SIZE; j++) { 
						for (class_descriptor* derived = class_descriptor::hash_table[j]; derived != NULL; derived = derived->next) {
							if (derived != cls && !is_btree(derived->name) && !derived->mop->is_transient()
								&& !(derived->class_attr & (class_descriptor::cls_hierarchy_super_root | class_descriptor::cls_hierarchy_root)))
							{
								for (class_descriptor* base = derived->base_class; base != NULL; base = base->base_class) { 
									if (base == cls) { 
										define_table_columns(columns, "", derived->fields, sql, -1);
										break;
									}
								}
							}
						}
					}
				}													
				sql << ")";
				//printf("%s\n", sql.str().c_str());
				txn.exec(sql.str());			   

				con->prepare(table_name + "_delete", std::string("delete from \"") + table_name + "\" where opid=$1");
				con->prepare(table_name + "_loadobj", std::string("select * from \"") + table_name + "\" where opid=$1");
				con->prepare(table_name + "_loadset", std::string("with recursive set_members(opid,obj) as (select m.opid,m.obj from set_member m where m.prev=$1 union all select m.opid,m.obj from set_member m join set_members s ON m.prev=s.opid) select m.opid as mbr_opid,m.next as mbr_next,m.prev as mbr_prev,m.owner as mbr_owner,m.obj as mbr_obj,m.key as mbr_key,t.* from set_members s, set_member m, \"") + table_name + "\" t where m.opid=s.opid and t.opid=s.obj limit $2");

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
								  objref_t opid_buf[], size_t nReservedOids, hnd_t clusterWith[])
{
	// Bulk allocate at obj_storage level is disabled 
	assert(false);
}

objref_t pgsql_storage::allocate(cpid_t cpid, size_t size, int flags, objref_t clusterWith)
{
	if (opid_buf_pos == OPID_BUF_SIZE) { 
		result rs = txn->prepared("new_oid")(OPID_BUF_SIZE).exec();
		for (size_t i = 0; i < OPID_BUF_SIZE; i++) { 
			opid_buf[i] = rs[i][0].as(objref_t());
		}
		opid_buf_pos = 0;
	}
	return MAKE_OBJREF(cpid, opid_buf[opid_buf_pos++]);
}

void pgsql_storage::deallocate(objref_t opid)
{ 
	class_descriptor* desc = lookup_class(GET_CID(opid));
	txn->prepared(get_table(desc) + "_delete")(opid).exec();
}

boolean pgsql_storage::lock(objref_t opid, lck_t lck, int attr)
{
	return true;
}

void pgsql_storage::unlock(objref_t opid, lck_t lck)
{
}

void pgsql_storage::start_transaction()
{
	if (txn == NULL && con != NULL) { 
		txn = new work(*con);
	}
}

void pgsql_storage::get_class(cpid_t cpid, dnm_buffer& buf)
{
	assert(cpid != RAW_CPID);		
	start_transaction();
	result rs = txn->prepared("get_class")(cpid).exec();
	assert(rs.size() == 1);
	binarystring desc = binarystring(rs[0][0]);
	memcpy(buf.put(desc.size()), desc.data(), desc.size());
}

cpid_t pgsql_storage::put_class(dbs_class_descriptor* dbs_desc)
{
	start_transaction();
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
	
																				 
void pgsql_storage::load(objref_t opid, int flags, dnm_buffer& buf)
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

static void unpack_array_of_reference(dnm_buffer& buf, result::tuple const& record) 
{
	result::tuple::reference col(record["array"]);
	std::string str = col.as(std::string());
	char const* src = &str[0];
	int n_items = 0;
	assert(*src == '{');
	src += 1;
	if (*src != '}') { 
		do { 
			objref_t opid;
			int n;
			int rc = sscanf(src, "%llu%n", &opid, &n);
			assert(rc == 1);
			packref(buf.append(sizeof(dbs_reference_t)), 0, opid);
			src += n;
			n_items += 1;
		} while (*src++ == ',');
		assert(*--src == '}');
	}
	pack4(buf.append(4), n_items);
}

static void unpack_int_array(dnm_buffer& buf, std::string const& str, int elem_size, int arr_len)
{
	
	char const* src = &str[0];
	int n_items = 0;
	assert(*src == '{');
	src += 1;
	if (*src != '}') { 
		do { 
			int8 val;
			int n;
			int rc = sscanf(src, "%llu%n", &val, &n);
			assert(rc == 1);
			switch (elem_size) { 
			  case 2:
				pack2(buf.append(2), (int2)val);
				break;
			  case 4:
				pack4(buf.append(4), (int4)val);
				break;
			  case 8:
				pack8(buf.append(8), (char*)&val);
				break;
			  default:
				assert(false);
			}
			src += n;
			n_items += 1;
		} while (*src++ == ',');

		assert(*--src == '}');
	}
	assert(arr_len == 0 || arr_len == n_items);
}

static size_t unpack_struct(std::string const& prefix, field_descriptor* first, 
							dnm_buffer& buf, result::tuple const& record, size_t refs_offs, int inheritance_depth)
{
	if (first == NULL) {
		return refs_offs;
	}

	field_descriptor* field = first;
    do { 
		if (field->loc.type == fld_structure) { 
			assert(field->loc.n_items == 1);
			refs_offs = unpack_struct(field == first && inheritance_depth > 1 ? prefix : prefix + field->name + ".", field->components, buf, record, refs_offs, field == first && inheritance_depth >  0 ? inheritance_depth-1 : 0);
		} else {
			result::tuple::reference col(record[std::string("\"") + prefix + field->name + '"']);
			assert(!col.is_null() || field->loc.type == fld_string);
			if (field->loc.n_items != 1) { 
				if (field->loc.size == 1) {
					if ((field->flags & fld_binary) || field->loc.type == fld_unsigned_integer) {
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
					unpack_int_array(buf, col.as(std::string()), field->loc.size, field->loc.n_items);
				}
			} else {
				switch(field->loc.type) { 
				  case fld_reference:
				  {
					  char* dst = &buf + refs_offs;
					  refs_offs += sizeof(dbs_reference_t);
					  objref_t opid = col.as(objref_t());
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
				  default:
					assert(false);
				}
			}
		}
        field = (field_descriptor*)field->next;
    } while (field != first);

	return refs_offs;
}

void pgsql_storage::unpack_object(std::string const& prefix, class_descriptor* desc, dnm_buffer& buf, result::tuple const& record)
{
	size_t hdr_offs = buf.size();
	dbs_object_header* hdr = (dbs_object_header*)buf.append(sizeof(dbs_object_header) + desc->n_fixed_references*sizeof(dbs_reference_t)); 
	objref_t opid = record[prefix + "opid"].as(objref_t());
	hdr->set_ref(opid);		
	hdr->set_cpid(GET_CID(opid));
	hdr->set_flags(0);
                
	if (desc->n_varying_references != 0) { 
		unpack_array_of_reference(buf, record);
	} else { 
		size_t refs_offs = unpack_struct(prefix, desc->fields, buf, record, hdr_offs + sizeof(dbs_object_header), get_inheritance_depth(desc));
		assert(refs_offs == hdr_offs + sizeof(dbs_object_header) + sizeof(dbs_reference_t)*desc->n_fixed_references);
	}
	hdr = (dbs_object_header*)(&buf + hdr_offs);
	hdr->set_size(buf.size() - hdr_offs - sizeof(dbs_object_header));
#if 0
	if (desc == &set_member::self_class && prefix.size() == 0) { 
		objref_t mbr_obj_opid;
		stid_t mbr_obj_sid;
		// unpack referene of set_member::obj
		unpackref(mbr_obj_sid, mbr_obj_opid, &buf + hdr_offs + sizeof(dbs_object_header) + sizeof(dbs_reference_t)*3);
		cpid_t cpid = GET_CID(mbr_obj_opid);
		std::string table_name = get_table(lookup_class(cpid));
		result rs = txn->prepared(table_name  + "_loadset")(opid)(max_preloaded_set_members).exec();
		load_query_result(rs, buf);
	}
#endif
}

objref_t pgsql_storage::load_query_result(result& rs, dnm_buffer& buf)
{
	objref_t next_mbr = 0;
	for (result::const_iterator i = rs.begin(); i != rs.end(); ++i) {
		result::tuple obj_record = *i;
		objref_t obj_opid = obj_record["opid"].as(objref_t());
		class_descriptor* obj_desc = lookup_class(GET_CID(obj_opid));
		size_t mbr_buf_offs = buf.size();
		unpack_object("mbr_", &set_member::self_class, buf, obj_record);
		stid_t next_sid;
		unpackref(next_sid, next_mbr, &buf + mbr_buf_offs + sizeof(dbs_object_header));
		unpack_object("", obj_desc, buf, obj_record); 
	}
	return next_mbr;
}



void pgsql_storage::query(objref_t& next_mbr, char const* query, nat4 buf_size, int flags, nat4 max_members, dnm_buffer& buf)
{
	start_transaction();
	load(next_mbr, flags, buf);
	stid_t sid;
	objref_t opid;
	unpackref(sid, opid, &buf + sizeof(dbs_object_header) + 3*sizeof(dbs_reference_t));
	cpid_t cpid = GET_CID(opid);
	class_descriptor* desc = lookup_class(cpid);
	std::string table_name = get_table(desc);
	std::stringstream sql;
	sql << "with recursive set_members(opid,obj) as (select m.opid,m.obj from set_member m where m.oipd=" << opid << " union all select m.opid,m.obj from set_member m join set_members s ON m.prev=s.opid) select m.opid as mbr_opid,m.next as mbr_next,m.prev as mbr_prev,m.owner as mbr_owner,m.obj as mbr_obj,m.key as mbr_key,t.* from set_members s, set_member m, " << table_name << " t where m.opid=s.opid and t.opid=s.obj and " << query << " limit " << max_members;
	result rs = txn->exec(sql.str());
	buf.put(0); // reset buffer
	next_mbr = load_query_result(rs, buf);
}

void pgsql_storage::load(objref_t* opp, int n_objects, 
						 int flags, dnm_buffer& buf)
{
	start_transaction();
	buf.put(0);
	for (int i = 0; i < n_objects; i++) { 
		objref_t opid = opp[i];
		cpid_t cpid = GET_CID(opid);
		if (GET_OID(opid) == ROOT_OPID) { 
			result rs = txn->prepared("get_root").exec();
			if (rs.empty()) { 
				dbs_object_header* hdr = (dbs_object_header*)buf.append(sizeof(dbs_object_header));
				hdr->set_ref(ROOT_OPID);		
				hdr->set_cpid(RAW_CPID);
				hdr->set_flags(0);
				hdr->set_size(0);
				continue;
			}
			assert(rs.size() == 1);
			cpid = rs[0][0].as(cpid_t());
		}
		class_descriptor* desc = lookup_class(cpid);
		if (desc == &ExternalBlob::self_class) { 
			result rs = txn->prepared("read_file")(opid).exec();
			assert(rs.size() == 1);
			binarystring blob(rs[0][0]);
			dbs_object_header* hdr = (dbs_object_header*)buf.append(sizeof(dbs_object_header) + blob.size()); 
			hdr->set_ref(opid);		
			hdr->set_cpid(cpid);
			hdr->set_flags(0);
			hdr->set_size(blob.size()); 
			memcpy(hdr+1, blob.data(), blob.size());
		} else { 
			result rs = txn->prepared(get_table(desc) + "_loadobj")(opid).exec();
			assert(rs.size() == 1);
			size_t hdr_offs = buf.size();
			unpack_object("", desc, buf, rs[0]);
			if (GET_OID(opid) == ROOT_OPID) {
				dbs_object_header* hdr = (dbs_object_header*)(&buf + hdr_offs);
				hdr->set_cpid(cpid);
			}
		}
	}
}

void pgsql_storage::forget_object(objref_t opid)
{
}


void pgsql_storage::throw_object(objref_t opid)
{
}

void pgsql_storage::begin_transaction(dnm_buffer& buf)
{
	buf.put(0);
	start_transaction();
}

static void store_array_of_references(invocation& stmt, char* src_refs, char* src_bins)
{
	int n_refs = unpack4(src_bins);
	stmt(n_refs);
	std::stringstream buf;
	buf << '{';
	for (int i = 0; i < n_refs; i++) { 
		objref_t opid;
		stid_t sid;
		src_refs = unpackref(sid, opid, src_refs);
		if (i != 0) {
			buf << ',';
		}
		buf << opid;
	}
	buf << '}';
	stmt(buf.str());
}

static void store_int_array(invocation& stmt, char* src_bins, int elem_size, int length)
{
	std::stringstream buf;
	buf << '{';
	for (int i = 0; i < length; i++) { 
		if (i != 0) {
			buf << ',';
		}
		switch (elem_size) { 
		  case 2:
			buf << (int2)unpack2(src_bins);
			src_bins += 2;
			break;
		  case 4:
			buf << (int4)unpack4(src_bins);
			src_bins += 4;
			break;
		  case 8:
		  {
			  int8 ival;
			  src_bins = unpack8((char*)&ival, src_bins);
			  buf << ival;
			  break;
		  }
  		  default:
			assert(false);
		}
	}
	buf << '}';
	stmt(buf.str());	
}

size_t pgsql_storage::store_struct(field_descriptor* first, invocation& stmt, char* &src_refs, char* &src_bins, size_t size)
{
	if (first == NULL) {
		return size;
	}

	field_descriptor* field = first;	
    do { 
		if (field->loc.n_items != 1) { 
			switch (field->loc.type) { 
			  case fld_unsigned_integer:
				if (field->loc.size == 1) { 
					if (field->loc.n_items == 0) {
						std::string val(src_bins, size);
						stmt(txn->esc_raw(val));
						src_bins += size;
						size = 0;
					} else { 
						std::string val(src_bins, field->loc.n_items);
						stmt(txn->esc_raw(val));
						src_bins += field->loc.n_items;
						size -= field->loc.n_items;
					}
				} else { 
					if (field->loc.n_items == 0) {
						store_int_array(stmt, src_bins, field->loc.size, size/field->loc.size);
						src_bins += size;
						size = 0;
					} else { 
						store_int_array(stmt, src_bins, field->loc.size, field->loc.n_items);
						src_bins += field->loc.n_items*field->loc.size;
						size -= field->loc.n_items*field->loc.size;
					}
				}
				break;

			  case fld_signed_integer:
				if (field->loc.size == 1) {
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
					if (field->loc.n_items == 0) {
						store_int_array(stmt, src_bins, field->loc.size, size/field->loc.size);
						src_bins += size;
						size = 0;
					} else { 
						store_int_array(stmt, src_bins, field->loc.size, field->loc.n_items);
						src_bins += field->loc.n_items*field->loc.size;
						size -= field->loc.n_items*field->loc.size;
					}					
				}
				break;
			  default:
				assert(false);
			}
		} else {
			switch(field->loc.type) { 
			  case fld_reference:
			  {
				  objref_t opid;
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
					//stmt((nat2)*src_bins++);
					stmt((int2)*src_bins++);
					size -= 1;
					break;
				  case 2:
					//stmt(unpack2(src_bins));
					stmt((int2)unpack2(src_bins));
					src_bins += 2;
					size -= 2;
					break;
				  case 4:
					//stmt(unpack4(src_bins));
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
		objref_t opid = hdr->get_ref();
		cpid_t cpid = hdr->get_cpid();		
		int flags = hdr->get_flags();
		assert(cpid != RAW_CPID);
		if (GET_OID(opid) == ROOT_OPID) { 
			result rs = txn->prepared("get_root").exec();
			if (rs.empty()) { 
				txn->prepared("add_root")(cpid).exec();
				flags |= tof_new;
			} else { 
				txn->prepared("set_root")(cpid).exec();
			}
			opid = ROOT_OPID;
		}			
		class_descriptor* desc = lookup_class(cpid);
		assert(opid != 0);
		char* src_refs = (char*)(hdr+1);
		char* src_bins = src_refs + desc->n_fixed_references*sizeof(dbs_reference_t);
		if (desc == &ExternalBlob::self_class) { 
			std::string blob(src_bins, hdr->get_size());
			txn->prepared("write_file")(txn->esc_raw(blob))(opid).exec();
		} else if ((flags & tof_new) != 0 || hdr->get_size() != 0) { 	
			invocation stmt = txn->prepared(std::string(desc->name) + ((flags & tof_new) ? "_insert" : "_update"));
			stmt(opid);
			//printf("%s object %d\n", (flags & tof_new) ? "Insert" : "Update", opid);
			if (desc->n_varying_references != 0) { 
				src_bins += (hdr->get_size() - desc->packed_fixed_size)/desc->packed_varying_size*desc->n_varying_references*sizeof(dbs_reference_t);
				store_array_of_references(stmt, src_refs, src_bins);
			} else { 
				size_t left = store_struct(desc->fields, stmt, src_refs, src_bins, hdr->get_size());
				assert(left == 0);
			}
			result r = stmt.exec();
			assert(r.affected_rows() == 1);
		}
		ptr = (char*)(hdr + 1) + hdr->get_size();
	}
	txn->commit();
	//printf("Commit transaction\n");
	delete txn;
	txn = NULL;
	// cleanup cache after transaction commit to avoid deteriorated object instances because we do not have invalidation mechanism for PsotgreSQL
	cache_manager::instance.invalidate_cache(); 
	return true;
}

void pgsql_storage::rollback_transaction()
{
	if (txn != NULL) { 
		txn->abort();
		//printf("Abort transaction\n");
		delete txn;
		txn = NULL;
	}
	cache_manager::instance.invalidate_cache(); 
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
	obj_storage* os = obj->get_handle()->storage;
	return os ? (pgsql_storage*)os->storage : NULL;
}

invocation pgsql_storage::statement(char const* name)
{
	start_transaction();
	return txn->prepared(name);
}

ref<set_member> pgsql_storage::index_find(database const* db, objref_t index, char const* op, std::string const& key)
{
	start_transaction();
	result rs = txn->prepared(op)(index)(txn->esc_raw(key)).exec();
	size_t size = rs.size();
	if (size == 0) { 
		return NULL;
	}
	assert(size == 1);
	result::tuple record = rs[0];
	objref_t opid = record[0].as(objref_t());
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
	std::string strkey(str, len);
	if (pg == NULL) { 
		inmem_index_impl::const_iterator it = mem_index.find(strkey);
		if (it == mem_index.end()) { 
			return NULL;
		}
		return it->second;
	}
	return pg->index_find(get_database(), hnd->opid, "index_equal", strkey);
}

ref<set_member> pgsql_index::find(const char* str) const
{
	return find(str, strlen(str));
}

ref<set_member> pgsql_index::find(skey_t key) const
{
	pack8(key);
	return find((char*)&key, sizeof(key));
}

ref<set_member> pgsql_index::findGE(const char* str, size_t len, skey_t key) const
{
	pgsql_storage* pg = get_storage(this);
	std::string strkey(str, len);
	if (pg == NULL) { 
		inmem_index_impl::const_iterator it = mem_index.lower_bound(strkey);
		if (it == mem_index.end()) { 
			return NULL;
		}
		return it->second;
	}
	return pg->index_find(get_database(), hnd->opid, "index_greater_or_equal", strkey);
}
	
ref<set_member> pgsql_index::findGE(skey_t key) const
{
	pack8(key);
	return findGE((char*)&key, sizeof(key));
}

ref<set_member> pgsql_index::findGE(const char* str) const
{
	return findGE(str, strlen(str));
}


void pgsql_index::insert(ref<set_member> mbr)
{
	ref<set_member> next;
	pgsql_storage* pg = get_storage(this);
	if (&mbr->cls != &set_member::self_class) {
		skey_t key = mbr->get_key();
		next = findGE(key);
		if (pg == NULL) { 
			mem_index.insert(std::pair< std::string, ref<set_member> >(std::string((char*)&key, sizeof(key)), mbr));
		}			
	} else { 
		char key[MAX_KEY_SIZE];
		size_t keySize = mbr->copyKeyTo(key, MAX_KEY_SIZE);
		assert(keySize < MAX_KEY_SIZE);
		next = findGE(key, keySize, 0); // skey is not used here
		if (pg == NULL) { 
			mem_index.insert(std::pair< std::string, ref<set_member> >(std::string(key, keySize), mbr));
		}			
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
	assert (pg != NULL);
		//printf("Delete %d from set_member\n", mbr->get_handle()->opid);
	pg->statement("index_del")(hnd->opid)(mbr->get_handle()->opid).exec();	
	set_owner::remove(mbr);
}

void pgsql_index::clear()
{
	pgsql_storage* pg = get_storage(this);
	if (pg != NULL) { 
		pg->statement("index_drop")(hnd->opid).exec();	
	} else { 
		mem_index.clear();
	}
	//printf("Drop index %d\n", hnd->opid);
    last = NULL;
    first = NULL;
	n_members = 0;
	obj = NULL;
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
		object_monitor::lock_global();
		obj_hnd->obj->mop->make_persistent(obj_hnd, hnd);
		object_monitor::unlock_global();
	}
	pg->statement("hash_put")(hnd->opid)(name)(obj_hnd->opid).exec();	
}

anyref pgsql_dictionary::get(const char* name) const
{
	pgsql_storage* pg = get_storage(this);
	result rs = pg->statement("hash_get")(hnd->opid)(name).exec();
	anyref ref;
	if (rs.empty()) { 
		return NULL;
	}
	((database*)get_database())->get_object(ref, rs[0][0].as(objref_t()), 0);
	return ref;
}

boolean pgsql_dictionary::del(const char* name) 
{
	pgsql_storage* pg = get_storage(this);
	result rs = pg->statement("hash_delall")(hnd->opid)(name).exec();
	boolean found = rs.affected_rows() != 0;
	return found;
}

boolean pgsql_dictionary::del(const char* name, anyref obj) 
{
	pgsql_storage* pg = get_storage(this);
	hnd_t obj_hnd = obj->get_handle();
	if (obj_hnd == NULL) { 
		return false;
	}
	result rs = pg->statement("hash_del")(hnd->opid)(name)(obj_hnd->opid).exec();
	bool found = rs.affected_rows() != 0;
	return found;
}

anyref pgsql_dictionary::apply(hash_item::item_function) const
{
	assert(false);
	return NULL;
}

void pgsql_dictionary::reset()
{
	pgsql_storage* pg = get_storage(this);
	pg->statement("hash_drop")(hnd->opid).exec();
}

size_t pgsql_dictionary::get_number_of_elements() const
{
	pgsql_storage* pg = get_storage(this);
	result rs = pg->statement("hash_size")(hnd->opid).exec();
	assert(rs.size() == 1);
	int64_t n_elems = rs[0][0].as(int64_t());
	return n_elems;
}
