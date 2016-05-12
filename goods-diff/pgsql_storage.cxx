#inlude "pgsql_storage.h"

#define MAX_TABLES 256
#define GET_CID(x) ((x) % MAX_TABLES)
#define GET_OID(x) ((x) / MAX_TABLES)
#define MAKE_OPID(t,o) ((o)*MAX_TABLES + (t))
#define OID_BIAS 0x10000

inline string get_table(class_descriptor* desc)
{
	while (desc->base_class != &object::self_class) { 
		desc = desc->base_class;
	}
	return string(desc->name);
}

static void get_columns(string prefix, field_descriptor* first, vector<string>& columns)
{
	field_descriptor* field = first;
	
    do { 
		if (field->field->loc.type == fld_structure) { 
			get_columns(prefix + field->name, field->components, columns);
		} else { 
			columns->push_back(prefix + field->name);
		}
        field = (field_descriptor*)field->next;
    } while (field != first);
}

static string get_host(string address)
{
	return address.substr(0, address.find(':'));
}

static string get_port(string address)
{
	return address.substr(address.find(':')+1);
}

static void define_table_columns(string prefix, field_descriptor* first, stringstream& sql)
{
    do { 
		if (field->field->loc.type == fld_structure) { 
			define_table_columns(prefix + field->name, field->components, sql);
		} else { 
			sql << ",\"" << prefix << field->name << "\" " << map_type(field->dbs.type, field->dbs.size);
		}
        field = (field_descriptor*)field->next;
    } while (field != first);
}
	

void pgsql_storage::open(char const* connection_address, const char* login, const char* password) 
{
	opid_buf_pos = 0;
	con = new connection(string("user=") + login + " password=" + password + " host=" + get_host(connection_address) + " port=" + get_port(connection_address));
	work txn(*con);	
	con->prepare("alloc", "select nextval('oid_sequence') from generate_series(1,$1"); 
	con->prepare("get_class", "select desc from classes where oid=?"); 
	con->prepare("put_class", "insert into classes (desc) values ($1)"); 
	con->prepare("change_class", "update classes set desc=$1 where oid=$2"); 
	con->prepare("index_equal", "select * from set_member m where owner=$1 and key=$2 and not exists (select * from set_member p where p.oid=m.prev and p.owner=$1 and p.key=$2)");
	con->prepare("index_greater_or_equal", "select * from set_member where owner=$1 and key>=$2 limit 1");
	con->prepare("index_equal_skey", "select * from set_member m where owner=$1 and skey=$2 and not exists (select * from set_member p where p.oid=m.prev and p.owner=$1 and p.skey=$2)");
	con->prepare("index_greater_or_equal_skey", "select * from set_member where owner=$1 and skey>=$2 limit 1");
	con->prepare("index_drop", "delete from set_member where owner=$1");
	con->prepare("index_del", "delete from set_member where oid=$1");

	con->prepare("hash_put", "insert into hash_table (owner,name,opid,sid) values ($1,$2,$3,$4)");
	con->prepare("hash_get", "select opid,sid from hash_table where owner=$1 and name=$2");
	con->prepare("hash_delall", "delete from hash_table where owner=$1 and name=$2");
	con->prepare("hash_del", "delete from hash_table where owner=$1 and name=$2 and opid=$3 and sid=$4");
	con->prepare("hash_drop", "delete from hash_table where owner=$1");
	con->prepare("hash_size", "select count(*) from hash_table where owner=$1");
				 
	for (size_t i = 0; i < DESCRIPTOR_HASH_TABLE_SIZE; i++) { 
		for (class_descriptor* cls = class_descriptor::hash_table[i]; cls != NULL; cls = cls->next) {
			vector<string> columns;
			string table_name = get_table(cls);
			string class_name(cls->name);
			get_columns("", cls->fields, columns);
			if (cls->base_class == &set_member::self_desc) { 
				columns.push_back("skey");
			}
			{
				stringstream sql;			
				sql << "insert into " << table_name << " (";
				for (size_t i = 0; i < columns.size(); i++) { 
					if (i != 0) sql << ',';
					sql << '\"' << columns[i] << '\"';
				}
				sql << ") values (";
				for (size_t i = 0; i < columns.size(); i++) { 
					if (i != 0) sql << ',';
					sql << '$' << (i+1);
				}
				sql << ")";
				con->prepare(class_name + "_insert", sql);
			}
			{
				stringstream sql;			
				sql << "update " << class_name << " set ";
				for (size_t i = 0; i < columns.size(); i++) { 
					if (i != 0) sql << ',';
					sql << '\"' << columns[i] << '\"';
				}
				sql << " where opid=$1";
				con->prepare(class_name + "_update", sql);
			}
			if (table_name == class_name) {
				stringstream sql;
				sql << "create table if not exists \"" << cls->name << "\"(opid bigint primary key";
				define_table_columns("", cls->fields, csql);
				if (cls->base_class == &set_member::self_desc) { 
					sql << ", skey bigint";
				}
				sql << ")";
				txn.exec(sql.str());			   

				con->prepare(table_name + "_delete", string("delete from ") + table_name + " where opid=$1");
				con->prepare(table_name + "_loadobj", string("select * from ") + table_name + " where opid=$1");
				con->prepare(table_name + "_loadset", string("with recursive set_members(opid,obj) as (select m.opid,m.obj from set_member m where m.prev=$1 union all select m.opid,m.obj from set_member m join set_members s ON m.prev=s.opid) select s.opid as mbr_opid,s.next as mbr_next,s.prev as mbr_prev,s.owner as mbr_owner,s.obj as mbr_obj,s.key as mbr_key,t.* from set_members s, ") + table_name + " t where t.opid=s.obj limit $2");

			}
		}	
	}	
	txn.exec("create index if not exists set_member_key on set_member(key)");
	txn.exec("create index if not exists set_member_skey on set_member(skey)");
	txn.commit();
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
		result rs = txn->prepared("alloc")(OPID_BUF_SIZE).exec();
		for (size_t i = 0; i < OPID_BUF_SIZE; i++) { 
			opid_buf[i] = rs[i][0].as(opid_t());
		}
		opid_buf_pos = 0;
	}
	return MAKE_OPID(opid_buf{opid_buf_pos++], cpid);
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
	result rs = txt->prepared("get_class")(cpid + OID_BIAS).exec();
	binarystring desc = rs[0][0].as(binarystring());
	memcpy(buf.put(desc.size()), desc.data());
}

cpid_t pgsql_storage::put_class(dbs_class_descriptor* dbs_desc)
{
	size_t dbs_desc_size = dbs_desc->get_size();	
	binarystring buf(dbs_desc, dbs_desc_size);	
	((dbs_class_descriptor*)buf.data())->pack();
	result rs = txn->prepared("put_class")(data).exec();	
	return rs.oid() - OID_BIAS;
}

void pgsql_storage::change_class(cpid_t cpid, 
                                      dbs_class_descriptor* dbs_desc)
{
	size_t dbs_desc_size = dbs_desc->get_size();
	binarystring buf(dbs_desc, dbs_desc_size);
	((dbs_class_descriptor*)buf.data())->pack();
	txt->prepared("change_class")(data,cpid + OID_BIAS).exec();
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
		assert(desc->dbs_desc != *dbs_desc);
		if (descriptor_table.size() <= cpid) { 
			descriptor_table.resize(cpid);
		}
		descriptor_table[cpid] = desc;
	}
	return desc;
}

static size_t unpack_struct(field_descriptor* first, string prefix, 
							dnm_buffer& buf, tuple const& record, size_t n_refs)
{
	field_descriptor* field = first;
    do { 
		tuple::reference col = record[prefix + field->name];
		assert(!col.is_null() || field->loc.type == fld_string);
		if (field->loc.n_items != 1) { 
			assert(field->loc.type == fld_signed_integer && field->loc.size == 1);
			string str = col.as(string());
			if (varying_length == 0) { 
				char* dst = buf.append(str.size());
				memcpy(dst, str.data(), str.size());
			} else { 
				char* dst = buf.append(field->loc.n_items);
				if (field->loc.n_items <= str.size) { 
					memcpy(dst, str.data(), field->loc.n_items);
				} else { 
					memcpy(dst, str.data(), str.size());
					memset(dst + str.size(), 0, field->loc.n_items - str.size());
				}
			}
		} else {
			switch(field->loc.type) { 
			  case fld_reference:
			  {
				  char* dst = (dbs_referece_t*)&buf + n_refs++;
				  nat8 objref = col.as(nat8());
				  opid_t opid = (opid_t)objref;
				  packref(dst, (stid_t)(objref >> 32), opid);
				  break;			  
			  }
			  case fld_string:
			  {
				  if (col.is_null()) { 
					  char* dst = buf.append(2);
					  pack2(dst, 0xFFFF);
				  } else {
					  string str = col.as(string());
					  std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
					  std::wstring wstr = conv.from_bytes(str);
					  char* dst = buf.append((1 + str.size())*2);
					  dst = pack2(dst, str.size());
					  for (size_t i = 0i i < str.size(); i++) { 
						  dst = pack2(dst, str[i]);
					  }
				  }
				  break;
			  }
			  case fld_raw_binary:
			  {
				  binarystring blob = col.as(binarystring());
				  char* dst = buf.append(4 + blob.size());
				  dst = pack4(dst, blob.size());
				  memcpy(dst, blob.data(), blob.size());
				  break;
			  }
			  case fld_signed_integer:
			  case fld_unsigned_integer:
				switch (field->loc.size) { 
				  case 1:				
					*buf.append(1) = col.as(int1());
					break;
				  case 2:
					pack2(buf.append(2), col.as(int2()));
					break;
				  case 4:
					pack4(buf.append(4), col.as(int4()));
					break;
				  case 8:
					pack8(buf.append(8), col.as(int8()));
					break;
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
					  union {
						  double f;
						  int8   i;
					  } u;
					  u.f = col.as(double());
					  pack8(buf.append(8), u.i); 
					  break;
				  }
				  default:
					assert(false);
				}
				break;
			  case fld_structure:
				n_regs = load_struct(string(field->name) + ".", field->coponents, buf, record, n_refs);
				break;
			  default:
				asset(false);
			}
		}
        field = (field_descriptor*)field->next;
    } while (field != first);

	return n_refs;
}

static opid_t load_query_result(result& rs, dnm_buffer& buf);

static void unpack_object(string prefix, class_descriptor* desc, dnm_buffer& buf, tuple const& record)
{
	dbs_object_header* hdr = (dbs_object_header*)buf.append(sizeof(dbs_object_header) + desc->n_fixed_references*sizeof(dbs_reference_t)); 
	size_t hdr_offs = buf.size();
	opid_t opid = record[prefix + "opid"].as(opid_t());
	hdr->set_opid(opid);		
	hdr->set_cpid(GET_CID(opid));
	hdr->set_sid(sid);
	hdr->set_flags(0);
                
	size_t n_refs = unpack_struct(desc->fields, prefix, buf, record, 0);
	assert(n_refs == desc->n_fixed_references);
	hdr = (dbs_object_header*)(&buf + hdr_offs);
	hdr->size = buf.size() - hdr_offs;

	if (desc == &set_member::self_class && prefix.size() == 0) { 
		opid_t mbr_obj_opid;
		stid_t mbr_obj_sid;
		// unpack referene of set_member::obj
		unpackref(mbr_obj_sid, mbr_obj_opid, &buf + hdr_offs + sizeof(dbs_object_header) + sizeof(dbs_reference_t)*3);
		cpid_t cpid = GET_CID(mbr_obj_opid);
		string table_name = get_table(lookup_class(cpid));
		result rs = txn->prepared(table_name  + "loadset")(opid)(max_preloaded_set_members).exec();
		load_query_result(rs, buf);
	}
	return opid;
}

static opid_t load_query_result(result& rs, dnm_buffer& buf)
{
	opid_t next_mbr = 0;
	for (auto i = rs.begin(); i != rs.end(); ++i) {
		tuple obj_record = *i;
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
	load(next_mbr, flags, buf);
	stid_t sid;
	opid_t opid;
	unpackref(sid, opid, &buf + sizeof(dbs_object_header) + 3*sizeof(dbs_referece_t));
	cpid_t cpid = GET_CID(opid);
	class_descriptor* desc = lookup_class(cpid);
	string table_name = get_table(desc);
	std::stringstream sql;
	sql << "with recursive set_members(opid,obj) as (select m.opid,m.obj from set_member m where m.oipd=" << opid << " union all select m.opid,m.obj from set_member m join set_members s ON m.prev=s.opid) select s.opid as mbr_opid,s.next as mbr_next,s.prev as mbr_prev,s.owner as mbr_owner,s.obj as mbr_obj,s.key as mbr_key,t.* from set_members s, " << table_name << " t where t.opid=s.obj and " << query << " limit " << max_members;
	result rs = txn->exec(sql.str());
	buf.cut(buf.size());
	next_mbr = load_query_result(rs, buf);
}


void pgsql_storage::load(opid_t* opp, int n_objects, 
						 int flags, dnm_buffer& buf)
{
	for (int i = 1; i < n_objects; i++) { 
		opid_t opid = opp[i];
		cpid_t cpid = GET_CID(opid);
		class_descriptor* desc = lookup_class(cpid);		
		result rs = txn->prepared(get_table(desc) + "_loadobj")(GET_OID(opid)).exec();
		unpack_object("", desc, buf, rs[0]);
	}
}

void pgsql_storage::forget_object(opid_t opid)
{
}


void pgsql_storage::throw_object(opid_t opid)
{
}

void pgsql_storage::begin_transaction(dnm_buffer& buf)
{
	txn = new work(*con);
}


static size_t store_struct(field_descriptor* first, invocation& stmt, char* &src_refs, char* &src_bins, size_t size)
{
	field_descriptor* field = first;
	
    do { 
		if (field->loc.n_items != 1) { 
			assert(field->loc.type == fld_signed_integer && field->loc.size == 1);
			string str = col.as(string());
			if (field->loc.n_items == 0) {
				stmt(string(src_bins, size));
				src_bins += size;
				size = 0;
			} else { 
				stmt(string(src_bins, field->loc.n_items));
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
					  vectpor<wchar_t> buf(len);
					  for (size_t i = 0; i < len; i++) { 
						  buf[i] = unpack2(src_bins);
						  src_bins += 2;
						  size -= 2;
					  }
					  std::wstring wstr(&buf, len);
					  std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
					  std::string str = conv.to_bytes(wstr);
					  stmt(str);
				  } else {
					  stmt(); // store NULL
				  }
				  break;
			  }
			  case fld_raw_binary:
			  {
				  size_t len = unpack4(src_bins);
				  binarystring blob(src_bins+4, len);
				  stmt(blob);
				  src_bins += 4 + len;
				  size -= 4 + len;
				  break;
			  }
			  case fld_signed_integer:
				switch (field->loc.size) { 
				  case 1:	
					stmt(*src_bins++);
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
					stmt(unpack8(src_bins));
					src_bins += 8;
					size -= 8;
					break;
				  default:
					assert(false);
				}
				break;
			  case fld_unsigned_integer:
				switch (field->loc.size) { 
				  case 1:	
					stmt((nat1)*src_bins++);
					size -= 1;
					break;
				  case 2:
					stmt((nat2)unpack2(src_bins));
					src_bins += 2;
					size -= 2;
					break;
				  case 4:
					stmt((nat4)unpack4(src_bins));
					src_bins += 4;
					size -= 4;
					break;
				  case 8:
					stmt((nat8)unpack8(src_bins));
					src_bins += 8;
					size -= 8;
					break;
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
					  union {
						  double f;
						  int8   i;
					  } u;
					  u.i = unpack8(src_bins);
					  stmt(u.f);
					  src_bins += 8;
					  size -= 8;
					  break;
				  }
				  default:
					assert(false);
				}
				break;
			  case fld_structure:
				size = store_struct(field->coponents, stmt, src_bins, src_refs, size);
				break;
			  default:
				asset(false);
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
	char* ptr = &buf;
	char* end = ptr + buf.size();
	while (ptr < end) { 
		dbs_object_header* hdr = (dbs_object_header*)&buf;
		int flags = hdr->get_flags();
		class_descriptor* desc = lookup_class(hdr->get_cpid());
		invocation stmt = txn->preapred(string(desc->name) + ((flags & tof_update) ? "_update" : "_insert"));
		stmt(hdr->get_opid());
		size_t left = store_struct(desc->fields, stmt, (char*)(hdr+1), hdr->get_size());
		assert(left == 0);
		if (desc->base_class == &set_member::self_desc) { 
			int64_t skey = 0; 
			switch (hdr->get_size() - sizeof(dbs_reference_t)*4) { 
			  case 4:
				skey = unpack4((char*)(hdr+1) + sizeof(dbs_reference_t)*4);
				break;
			  case 8:
				skey = unpack8((char*)(hdr+1) + sizeof(dbs_reference_t)*4);
				break;
			}
			stmt(skey);
		}
		stmt.exec();
	}
	txn->commit();
	delete txn;
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
	return rs[0][0].as(nat8_t());
}

boolean pgsql_storage::start_gc()
{
	return false;
}

void pgsql_storage::add_user(char const* login, char const* password){}

void pgsql_storage::del_user(char const* login){}

inline pgsql_storage get_storage(object* obj) 
{
	return (pgsql_storage*)obj->hnd->storage->storage;
}

invocation pgsql_storage::statement(char const* name)
{
	return txn->prepared(name);
}

ref<set_member> pgsql_storage::index_find(opid_t index, char const* op, pstring key)
{
	result rs = txn->prepared(op)(index)(key);
	if (rs.empty()) { 
		return NULL;
	}
	tuple record = rs[0];
	opid_t opid = record[0].as(opid_t());
	hnd_t hnd = object_handle::create_persistent_reference(this, opid, 0);
	if (!IS_VALID_OBJECT(hnd->obj)) {
		dnm_buffer buf;
		class_descriptor* desc = &set_member::self_desc;
		unpack_object("", desc, buf, record); 
		dbs_object_header* hdr = (object_header*)buf;
		set_member::self_desc.unpack(hdr, hnd, lof_auto);
	}
	return hnd->obj;
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
	return pg->index_find(hnd->opid, "index_equal", string(str, len));
}

ref<set_member> pgsql_index::find(const char* str, size_t len, skey_t key) const
{
	pgsql_storage* pg = get_storage(this);
	return pg->index_find(hnd->opid, "index_greater_or_equal", string(str, len));
}


ref<set_member> pgsql_index::find(const char* str) const
{
	pgsql_storage* pg = get_storage(this);
	return pg->find(hnd->opid, "index_equal", string(str));
}

ref<set_member> pgsql_index::find(skey_t key) const
{
	pgsql_storage* pg = get_storage(this);
	stringstream buf;			
	buf << key;
	return pg->find(hnd->opid, "index_equal_skey", buf);
}

ref<set_member> pgsql_index::findGE(skey_t key) const
{
	pgsql_storage* pg = get_storage(this);
	stringstream buf;			
	buf << key;
	return pg->find(hnd->opid, "index_greater_or_equal_skey", buf);
}

ref<set_member> pgsql_index::findGE(const char* str, size_t len, skey_t key) const
{
	pgsql_storage* pg = get_storage(this);
	return pg->index_find(hnd->opid, "index_greater_or_equal", string(str, len));
}
	
ref<set_member> pgsql_index::findGE(const char* str) const
{
	pgsql_storage* pg = get_storage(this);
	return pg->index_find(hnd->opid, "index_greater_or_equal", string(str));
}


void pgsql_index::insert(ref<set_member> mbr)
{
	skey_t skey = mbr->get_key();
	ref<set_member> next = (skey != mbr->set_member::get_key())
		? findGE(skey) : findGE(mbr->key);
	if (next.is_nil()) { 
		put_last(mbr); 
	} else {
		put_before(next, mbr);
	}
}

void pgsql_index::remove(ref<set_member> mbr)
{
	pgsql_storage* pg = get_storage(this);
	pg->statement("index_del")(mbr->opid).exec();	
	set_owner::remove(mbr);
}

void pgsql_index::clear()
{
	pgsql_storage* pg = get_storage(this);
	pg->statement("index_drop")(hnd->opid).exec();	
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
		object::make_persistent(obj_hnd, hnd->storage);
	}
	pg->statement("hash_put")(hnd->opid)(name)(obj_hnd->opid)(obj_hnd->storage->id).exec();	
}

anyref pgsql_dictionary::get(const char* name) const
{
	pgsql_storage* pg = get_storage(this);
	result rs = pg->statement("hash_get")(hnd->opid)(name).exec();
	object_ref ref;
	if (rs.empty()) { 
		return NULL;
	}
	get_database()->get_object(ref, rs[0][0].as(opid_t()), rs[0][1].as(stid_t()));
	return ref;
}

bool pgsql_dictionary::del(const char* name) const
{
	pgsql_storage* pg = get_storage(this);
	result rs = pg->statement("hash_delall")(hnd->opid)(name).exec();
	return rs.affected_rows() != 0;
}

bool pgsql_dictionary::del(const char* name, anyref obj) const
{
	pgsql_storage* pg = get_storage(this);
	hnd_t obj_hnd = obj->get_handle();
	if (obj_hnd == NULL) { 
		return false;
	}
	result rs = pg->statement("hash_del")(hnd->opid)(name)(obj_hnd->opid)(obj_hnd->storage->id).exec();
	return rs.affected_rows() != 0;
}

anyref pgsql_dictionary::apply(hash_item::item_function) const
{
	assert(false);
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
	return rs[0][0].as(int64_t());
}
