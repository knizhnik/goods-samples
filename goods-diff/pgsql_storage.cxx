#include <set>
#include "pgsql_storage.h"

#ifndef _WIN32
#include <iconv.h>
#endif

#ifndef GOODS_DATABASE_CONVERTER
#define GOODS_DATABASE_CONVERTER 1
#endif

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

inline bool is_text_set_member(class_descriptor* desc)
{
	return desc == &set_member::self_class
		|| (desc->base_class == &set_member::self_class 
			&& !(desc->class_attr & class_descriptor::cls_binary));
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
			if (field->loc.offs >= 0) { 
				std::string name = prefix + field->name;
				int i = columns.size();
				while (--i >= 0 && columns[i] != name);
				if (i < 0) { 
					columns.push_back(name);
				} else { 
					// duplicate field
					field->loc.offs = -1; // do not store this field
				}
			}
		}
        field = (field_descriptor*)field->next;
    } while (field != first);
}


static void compare_classes(char const* class_name, cpid_t cpid, std::string const& prefix, field_descriptor* goods_first, field_descriptor* orm_first)
{
	field_descriptor* field = goods_first;
	if (field != NULL) {
		do { 
			if (field->loc.type == fld_structure) { 
				field_descriptor* first = orm_first; 
				field_descriptor* matched = NULL;
				while (first != NULL) {
					field_descriptor* f = first;
					do { 
						if (strcmp(f->name, field->name) == 0) { 
							matched = f;
							break;
						}
					} while ((f = (field_descriptor*)f->next) != first);

					if (matched || first->loc.type != fld_structure) { 
						break;
					}
					first = first->components;
				}
				compare_classes(class_name, cpid, prefix + field->name + ".", field->components, 
								matched ? matched->components : NULL);
			} else { 
				std::string name = prefix + field->name;
				if (field->loc.offs >= 0) { 
					field_descriptor* f = goods_first; 
					while (strcmp(f->name, field->name) != 0) {
						f = (field_descriptor*)f->next;
					}
					if (f != field) { 	
						printf("= %s(%x)::%s\n", class_name, cpid, name.c_str());
					} else if (field->loc.type != field->dbs.type || field->loc.n_items != field->dbs.n_items) { 
						printf("# %s(%x)::%s: loc.type=%d, loc.n_items=%d, dbs.type=%d, dbs.n_items=%d\n", class_name, cpid, name.c_str(),
							   field->loc.type, field->loc.n_items, field->dbs.type, field->dbs.n_items);
					}
				} else { 
					printf("- %s(%x)::%s\n", class_name, cpid, name.c_str());
				}
			}
			field = (field_descriptor*)field->next;
		} while (field != goods_first);
	}
	field = orm_first;
	if (field != NULL) { 
		do { 
			field_descriptor* f = goods_first; 
			if (f != NULL) { 
				while (strcmp(f->name, field->name) != 0 && (f = (field_descriptor*)f->next) != goods_first);
			}
			if (f == NULL || strcmp(f->name, field->name) != 0) { 
				std::string name = prefix + field->name;
				printf("+ %s(%x)::%s\n", class_name, cpid, name.c_str());
			} 
			field = (field_descriptor*)field->next;
		} while (field != orm_first);
	}
}

static std::string get_host(std::string const& address)
{
	return address.substr(0, address.find(':'));
}

static std::string get_port(std::string const& address)
{
	size_t sep1 = address.find(':');
	size_t sep2 = address.find(':', sep1 + 1);
	return (sep2 != std::string::npos)
		? address.substr(sep1 + 1, sep2 - sep1 - 1) : address.substr(sep1 + 1);
}

static std::string get_database(std::string const& address)
{
	size_t sep1 = address.find(':');	
	size_t sep2 =  address.find(':',sep1+1);
	return (sep2 != std::string::npos)
		? address.substr(sep2+1) : std::string();
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
				return "text"; // "_int2";
			  case 4:
				return "_int4";
			  case 8:
				return "_int8";
			  default:
				assert(false);
			}
			break;
		  case fld_signed_integer:
			switch (field->loc.size) {
			  case 1:
				return "text";
			  case 2:
				return "_int2";
			  case 4:
				return "_int4";
			  case 8:
				return "_int8";
			  default:
				assert(false);
			}
			break;
		  case fld_real:
			switch (field->loc.size) { 
			  case 4:
				return "_float4";
			  case 8:
				return "_float8";
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
			return "int2";
		case 4:
			return "int4";
		case 8:
			return "int8";
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
			return "float4";
		case 8:
			return "float8";
		default:
			assert(false);
		}
	default:
		assert(false);
	}
	return "";
}

static void get_table_columns(std::map<std::string, field_descriptor*>& columns, std::string const& prefix, field_descriptor* first, int inheritance_depth)
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
			get_table_columns(columns, field == first && inheritance_depth > 1 ? prefix : prefix + field->name + ".", field->components, 
							  field == first && inheritance_depth >  0 ? inheritance_depth-1 : 0);
		} else if (columns.find(prefix + field->name) == columns.end()) { 
			columns[prefix + field->name] = field;
		}
  	  NextField:
        field = (field_descriptor*)field->next;
    } while (field != first);
}


inline bool is_btree(class_descriptor* desc)
{
	return desc == &B_tree::self_class || desc == &SB_tree16::self_class || desc == &SB_tree32::self_class;
}
	
boolean pgsql_storage::open(char const* connection_address, const char* login, const char* password, obj_storage* storage) 
{
	os = storage;
	opid_buf_pos = OPID_BUF_SIZE;
	std::stringstream connStr;
	connStr << "host=" << get_host(connection_address)
			<< " port=" << get_port(connection_address);
	std::string dbname = get_database(connection_address);
	if (!dbname.empty()) {
		connStr << " dbname=" << dbname;
	}
	if (login != NULL && *login) { 
		connStr << " user=" << login;
	}
	if (password != NULL && *password) { 
		connStr << " password=" << password;
	}
	if (!appName.empty()) { 
		connStr << " application_name=" << appName;
	}
	connString = connStr.str();

	connection* con = open_connection();
	
	try {
		work txn(*con);	
		txn.exec("create domain objref as bigint");
		txn.exec("create domain objrefs as bigint[]");
		txn.commit();
	} catch (pqxx_exception const&) {} // ignore error if domain already exists

	work txn(*con);	

	if (!dbname.empty()) {
		txn.exec("ALTER DATABASE " + dbname + " SET search_path = \"$user\", public, external_file");
	}

	txn.exec("create extension if not exists external_file");

	txn.exec("create table if not exists dict_entry(owner objref, name text, value objref, primary key(owner,name))");
	txn.exec("create table if not exists classes(cpid integer primary key, name text, descriptor bytea)");
	txn.exec("create table if not exists set_member(opid objref primary key, next objref, prev objref, owner objref, obj objref, key bytea)");
	txn.exec("create table if not exists root_class(cpid integer)");
	txn.exec("create table if not exists version_history(opid objref, clientid bigint, modtime bigserial)");

	txn.exec("create index if not exists set_member_owner on set_member(owner)");
	txn.exec("create index if not exists set_member_key on set_member(key)");
	txn.exec("create index if not exists set_member_prev on set_member(prev)");
	txn.exec("create index if not exists version_history_pk on version_history(modtime)");

	txn.exec("create sequence if not exists oid_sequence minvalue 65537");
	txn.exec("create sequence if not exists cid_sequence minvalue 2"); // 1 is RAW_CPID
	txn.exec("create sequence if not exists client_sequence");
		
	txn.exec("create or replace function on_update() returns trigger as $$ begin perform pg_notify(TG_ARGV[0],current_user); return new; end; $$ language plpgsql");

	if (txn.exec("select numbackends from pg_stat_database where datname=current_database()")[0][0].as(int64_t()) == 1) {
		txn.exec("delete from version_history");
	}

	con->prepare("get_attrs", "select attname,typname from pg_class,pg_attribute,pg_type where pg_class.relname=lower($1) and pg_class.oid=pg_attribute.attrelid and pg_attribute.atttypid=pg_type.oid and attnum>0 order by attnum");



	for (size_t i = 0; i < DESCRIPTOR_HASH_TABLE_SIZE; i++) { 
		for (class_descriptor* cls = class_descriptor::hash_table[i]; cls != NULL; cls = cls->next) {
			if (cls == &object::self_class || cls->mop->is_transient() 
				|| (cls->class_attr & class_descriptor::cls_non_relational) 
				|| (cls->constructor == NULL && (cls->class_attr & class_descriptor::cls_hierarchy_super_root))) 
			{ 
				continue;
			}
			class_descriptor* root_class = get_root_class(cls);
			std::string table_name(root_class->name);
			std::string class_name(cls->name);
			if (root_class == &set_member::self_class) { 
				if (cls->class_attr & class_descriptor::cls_binary) { 
					((field_descriptor*)root_class->fields->prev)->flags |= fld_binary; // mark set_member::key as binary
				}
				continue;
			}

			if (table_name == class_name) {
				std::stringstream sql;

				result rs = txn.prepared("get_attrs")(class_name).exec();
				size_t nAttrs = rs.size();

				std::map<std::string, field_descriptor*> columns;
				get_table_columns(columns, "", cls->fields, get_inheritance_depth(cls));
				// for all derived classes
				if (!(cls->class_attr & class_descriptor::cls_hierarchy_super_root)) {
					for (size_t j = 0; j < DESCRIPTOR_HASH_TABLE_SIZE; j++) { 
						for (class_descriptor* derived = class_descriptor::hash_table[j]; derived != NULL; derived = derived->next) {
							if (derived != cls && !is_btree(derived) && !derived->mop->is_transient()
								&& !(derived->class_attr & (class_descriptor::cls_hierarchy_super_root | class_descriptor::cls_hierarchy_root)))
							{
								for (class_descriptor* base = derived->base_class; base != NULL; base = base->base_class) { 
									if (base == cls) { 
										get_table_columns(columns, "", derived->fields, -1);
										break;
									}
								}
							}
						}
					}
				}													
				
				if (nAttrs != 0) { // table is already present in database
					char sep = ' ';
					std::map<std::string, std::string> oldColumns;
					for (size_t i = 0; i < nAttrs; i++) { 
						oldColumns[rs[i][0].as(std::string())] = rs[i][1].as(std::string());
					}
				
					sql << "alter table " << class_name;
					for (auto newColumn = columns.begin(); newColumn != columns.end(); newColumn++) {
						auto oldColumn = oldColumns.find(newColumn->first);
						auto fd = newColumn->second;
						std::string newType = map_type(fd);
						if (oldColumn == oldColumns.end()) {
							sql << sep << "add column \"" << newColumn->first << "\" " << newType;
							sep = ',';
						} else { 
							std::string oldType = oldColumn->second;
							if (newType != oldType) { 
								printf("Old type '%s' not equal to new type '%s'\n", oldType.c_str(), newType.c_str());
								sql << sep << "alter column \"" <<  newColumn->first << "\" set data type " << newType;
								sep = ',';								
							}
						}
					}
					if (sep != ' ') { // something has to be altered						
						printf("Schema evolution: %s\n", sql.str().c_str());
						txn.exec(sql.str());
					}
				} else { 
					// no such table
					sql << "create table " << class_name << "(opid objref primary key";
					for (auto column = columns.begin(); column != columns.end(); column++) {
						sql << ",\"" << column->first << "\" " << map_type(column->second);
					}
					sql << ")";
					//printf("%s\n", sql.str().c_str());
					txn.exec(sql.str());			   
				}
				for (auto column = columns.begin(); column != columns.end(); column++) {
					if (column->second->flags & fld_indexed) { 
						txn.exec(std::string("create index if not exists \"") +  class_name + "." + column->first + "\" on " + class_name + "(\"" +  column->first + "\")");
					}
				}
			}
		}
	}

	clientID = txn.exec("select nextval('client_sequence')")[0][0].as(int64_t()); 
	lastSyncTime = txn.prepared("lastsync").exec()[0][0].as(int64_t());
	txn.commit();
	return true;
}

void pgsql_storage::close()
{
	pgsql_session* session, *next;
	for (session = sessions; session != NULL; session = next) { 
		next = session->next;
		delete session;
	}
}


pgsql_session* pgsql_storage::new_session() 
{
	critical_section on(cs);
	pgsql_session* session = sessions;
	if (session == NULL) { 
		session = new pgsql_session(open_connection());
	} else {
		sessions = session->next;
	}
	return session;
}

void pgsql_storage::release_session(pgsql_session* session)
{
	critical_section on(cs);
	session->next = sessions;
	sessions = session;
}

	

void pgsql_storage::bulk_allocate(size_t sizeBuf[], cpid_t cpidBuf[], size_t nAllocObjects, 
								  objref_t opid_buf[], size_t nReservedOids, hnd_t clusterWith[])
{
	// Bulk allocate at obj_storage level is disabled 
	assert(false);
}

objref_t pgsql_storage::allocate(cpid_t cpid, size_t size, int flags, objref_t clusterWith)
{	
	autocommit txn(this); 
	critical_section on(cs);
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
	autocommit txn(this); 
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

boolean pgsql_storage::session_lock(nat8 id, lck_t lck, int attr)
{
	autocommit txn(this); 
	char const* lock_kind = (lck == lck_shared) 
		? (attr & lckattr_nowait) ? "nowait_shared_lock" : "shared_lock"
		: (attr & lckattr_nowait) ? "nowait_exclusive_lock" : "exclusive_lock";
	result rs = txn->prepared(lock_kind)(id).exec();
	return (attr & lckattr_nowait) ? rs[0][0].as(bool()) : true;
}

boolean pgsql_storage::session_unlock(nat8 id, lck_t lck) 
{
	autocommit txn(this); 
	char const* lock_kind = (lck == lck_shared) ? "shared_unlock" : "exclusive_unlock";
	result rs = txn->prepared(lock_kind)(id).exec();
	return rs[0][0].as(bool());
}	

void pgsql_storage::commit_transaction()
{
    transaction_manager* mng = transaction_manager::get();
	pgsql_extension* extension = (pgsql_extension*)mng->extension;
	if (extension != NULL) { 
		pgsql_session* session = extension->session;
		if (session->txn != NULL) { 
			session->txn->commit();
			//printf("Commit transaction\n");
			delete session->txn;
			session->txn = NULL;
		}
	}
}
	
connection* pgsql_storage::open_connection()
{
	connection* con = new connection(connString); // database name is expected to be equal to user name
	con->prepare("new_oid", "select nextval('oid_sequence') from generate_series(1,$1)"); 
	con->prepare("new_cid", "select nextval('cid_sequence')"); 
	con->prepare("set_oid", "select setval('oid_sequence', $1)"); 
	con->prepare("set_cid", "select setval('cid_sequence', $1)"); 
	con->prepare("get_root", "select cpid from root_class"); 
	con->prepare("add_root", "insert into root_class values ($1)"); 
	con->prepare("set_root", "update root_class set cpid=$1"); 
	con->prepare("get_class", "select descriptor from classes where cpid=$1"); 
	con->prepare("put_class", "insert into classes (cpid,name,descriptor) values ($1,$2,$3)"); 
	con->prepare("change_class", "update classes set descriptor=$1 where cpid=$2"); 
	con->prepare("index_equal", "select * from set_member m where owner=$1 and key=$2 and not exists (select * from set_member p where p.opid=m.prev and p.owner=$1 and p.key=$2)");
	con->prepare("index_greater_or_equal", "select * from set_member where owner=$1 and key>=$2 order by key limit 1");
	con->prepare("index_drop", "update set_member set owner=0 where owner=$1");
	con->prepare("index_del", "update set_member set owner=0 where owner=$1 and opid=$2");

	con->prepare("hash_put", "insert into dict_entry (owner,name,value) values ($1,$2,$3) "
				 "on conflict (owner,name) do update set value=$3 where dict_entry.owner=$1 and dict_entry.name=$2");
	con->prepare("hash_get", "select value from dict_entry where owner=$1 and name=$2");
	con->prepare("hash_delall", "delete from dict_entry where owner=$1 and name=$2");
	con->prepare("hash_del", "delete from dict_entry where owner=$1 and name=$2 and value=$3");
	con->prepare("hash_drop", "delete from dict_entry where owner=$1");

	con->prepare("mkversion", "insert into version_history (opid,clientid) values ($1,$2)");
	con->prepare("deteriorated", "select opid from version_history where modtime>$1 and clientid<>$2");
	con->prepare("lastsync", "select max(modtime) from version_history");
				
	con->prepare("hash_size", "select count(*) from dict_entry where owner=$1");
	con->prepare("write_file", "select writeEfile($1, $2)");
	con->prepare("read_file", "select readEfile($1)");

    con->prepare("get_time", "SELECT extract(epoch from now())");
	con->prepare("get_clients", "select count(distinct split_part(application_name, ':', 2)||client_addr), count(distinct application_name||client_addr) from pg_stat_activity where application_name like $1");

	con->prepare("nowait_shared_lock", "select pg_try_advisory_lock_shared($1)");
	con->prepare("shared_lock", "select pg_advisory_lock_shared($1)");
	con->prepare("nowait_exclusive_lock", "select pg_try_advisory_lock($1)");
	con->prepare("exclusive_lock", "select pg_advisory_lock($1)");
	con->prepare("exclusive_unlock", "select pg_advisory_unlock($1)");
	con->prepare("shared_unlock", "select pg_advisory_unlock_shared($1)");

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
			{
				std::stringstream sql;			
				sql << "insert into " << table_name << " (opid";
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
				sql << "update " << table_name << " set ";
				for (size_t i = 0; i < columns.size(); i++) { 
					if (i != 0) sql << ',';
					sql << '\"' << columns[i] << "\"=$" << (i+2);
				}
				sql << " where opid=$1";
				con->prepare(class_name + "_update", sql.str());
			}
			if (table_name == class_name) {
				con->prepare(table_name + "_delete", std::string("delete from ") + table_name + " where opid=$1");
				con->prepare(table_name + "_loadobj", std::string("select * from ") + table_name + " where opid=$1");
				con->prepare(table_name + "_loadobj_for_update", std::string("select * from ") + table_name + " where opid=$1 for update");
				con->prepare(table_name + "_loadset", std::string("with recursive set_members(opid,obj) as (select m.opid,m.obj from set_member m where m.prev=$1 union all select m.opid,m.obj from set_member m join set_members s ON m.prev=s.opid) select m.opid as mbr_opid,m.next as mbr_next,m.prev as mbr_prev,m.owner as mbr_owner,m.obj as mbr_obj,m.key as mbr_key,t.* from set_members s, set_member m, ") + table_name + " t where m.opid=s.opid and t.opid=s.obj limit $2");

			}
		}	
	}	
	return con;
}	

pgsql_session* pgsql_storage::current_session()
{
    transaction_manager* mng = transaction_manager::get();
	pgsql_extension* extension = (pgsql_extension*)mng->extension;
	if (extension == NULL) { 
		mng->extension = extension = new pgsql_extension(this);
	}
	return extension->session;
}

work* pgsql_storage::start_transaction(bool& toplevel)
{
	pgsql_session* session = current_session();
	if (session->txn == NULL) { 				
		session->con->get_notifs();
		session->txn = new work(*session->con);
		std::vector<objref_t> deteriorated;
		int64_t sync = session->txn->prepared("lastsync").exec()[0][0].as(int64_t());
		result rs = session->txn->prepared("deteriorated")(lastSyncTime)(clientID).exec();
		deteriorated.resize(rs.size());
		size_t i = 0;
		for (result::const_iterator it = rs.begin(); it != rs.end(); ++it) {
			result::tuple t = *it;
			deteriorated[i++] = t[0].as(objref_t());
		}
		assert(i == deteriorated.size());
		// Invalidation of objects need global lock. To avoid deadlock unlock first storage mutex.
		size_t n = deteriorated.size();
		object_monitor::lock_global(); 
		for (size_t i = 0; i < n; i++) { 				
			hnd_t hnd = object_handle::find(os, deteriorated[i]);
			if (hnd != 0) { 
				if (IS_VALID_OBJECT(hnd->obj)) { 
					hnd->obj->mop->invalidate(hnd); 
				} else { 
					hnd->obj = INVALIDATED_OBJECT; 
				}
			}
		}
		if (lastSyncTime < sync) { 
			lastSyncTime = sync;
		}
		object_monitor::unlock_global(); 
		toplevel = true;
	} else {
		toplevel = false;
	}
	return session->txn;
}

void pgsql_storage::get_class(cpid_t cpid, dnm_buffer& buf)
{
	autocommit txn(this);
	assert(cpid != RAW_CPID);		
	result rs = txn->prepared("get_class")(cpid).exec();
	assert(rs.size() == 1);
	binarystring desc = binarystring(rs[0][0]);
	memcpy(buf.put(desc.size()), desc.data(), desc.size());
}

cpid_t pgsql_storage::put_class(dbs_class_descriptor* dbs_desc)
{
	autocommit txn(this);
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
	autocommit txn(this);
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
	critical_section on(cs);
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


void pack_string(dnm_buffer& buf, std::string const& val)
{
#ifdef _WIN32
	int len = val.size();
	std::vector<wchar_t> wchars(len);
	if (len != 0) {
		len = MultiByteToWideChar(CP_UTF8, 0/*MB_ERR_INVALID_CHARS*/, val.data(), val.size(), &wchars[0], wchars.size()); 
		if (len == 0) { 
      	    fprintf(stderr, "Failed to convert string '%s' to UTF-16\n", val.c_str());
		}	
	}
	char* dst = buf.append((1 + len)*2);
	dst = pack2(dst, len);
	for (int i = 0; i < len; i++) { 
		dst = pack2(dst, (nat2)wchars[i]);
	}
#else
	wstring_t wstr(val.c_str());
	char* dst = buf.append((1 + wstr.length())*2);
	dst = pack2(dst, wstr.length());
	for (int i = 0; i < wstr.length(); i++) { 
		dst = pack2(dst, (nat2)wstr[i]);
	}
#endif
}	

static size_t unpack_struct(std::string const& prefix, field_descriptor* first, 
							dnm_buffer& buf, result::tuple const& record, size_t refs_offs, int inheritance_depth, 
							bool make_zero_terminated)
{
	if (first == NULL) {
		return refs_offs;
	}

	field_descriptor* field = first;
    do { 
		if (field->loc.type == fld_structure) { 
			assert(field->loc.n_items == 1);
			refs_offs = unpack_struct(field == first && inheritance_depth > 1 ? prefix : prefix + field->name + ".", field->components, buf, record, refs_offs, field == first && inheritance_depth >  0 ? inheritance_depth-1 : 0, make_zero_terminated);
		} else {
			result::tuple::reference col(record[std::string("\"") + prefix + field->name + '"']);
			//assert(!col.is_null() || field->loc.type == fld_string || field->loc.type == fld_reference);
			if (field->loc.n_items != 1) { 
				if (field->loc.size == 1) {
					if ((field->flags & fld_binary) || field->loc.type == fld_unsigned_integer) {
						binarystring blob(col);
						if (field->loc.n_items == 0) { 
							size_t size = blob.size();
							if (make_zero_terminated) { 
								char* dst = buf.append(size+1);
								memcpy(dst, blob.data(), size);
								dst[size] = '\0';
							} else {
								char* dst = buf.append(size);
								memcpy(dst, blob.data(), size);
							}
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
					  objref_t opid = col.is_null() ? 0 : col.as(objref_t());
					  packref(dst, 0, opid);
					  break;			  
				  }
				  case fld_string:
				  {
					  if (col.is_null()) { 
						  pack2(buf.append(2), 0xFFFF);
					  } else {
						  pack_string(buf, col.as(std::string()));
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

objref_t pgsql_storage::unpack_object(std::string const& prefix, class_descriptor* desc, dnm_buffer& buf, result::tuple const& record)
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
		size_t refs_offs = unpack_struct(prefix, desc->fields, buf, record, hdr_offs + sizeof(dbs_object_header), get_inheritance_depth(desc), is_text_set_member(desc));
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
		load_query_result(rs, buf, qf_include_members);
	}
#endif
	return opid;
}

objref_t pgsql_storage::load_query_result(result& rs, dnm_buffer& buf, objref_t last_mbr, int flags)
{
	objref_t next_mbr = 0;
	for (result::const_iterator i = rs.begin(); i != rs.end(); ++i) {
		result::tuple obj_record = *i;
		objref_t obj_opid = obj_record["opid"].as(objref_t());
		class_descriptor* obj_desc = lookup_class(GET_CID(obj_opid));
		size_t mbr_buf_offs = buf.size();
		objref_t mbr_opid = unpack_object("mbr_", &set_member::self_class, buf, obj_record);
		if (mbr_opid == last_mbr) { 
			next_mbr = 0;
		} else { 
			stid_t next_sid;
			unpackref(next_sid, next_mbr, &buf + mbr_buf_offs + sizeof(dbs_object_header));
		}
		if (!(flags & qf_include_members)) { 
			buf.put(mbr_buf_offs);
			unpack_object("", obj_desc, buf, obj_record); 
		}
	}
	return next_mbr;
}

void pgsql_storage::query(objref_t owner, char const* table, char const* where, char const* order_by, nat4 limit, nat4 offset, dnm_buffer& buf)
{
	autocommit txn(this);
	std::stringstream sql;
	sql << "select t.* from set_member m," << table << " t where m.owner=" << owner << " and t.opid=m.obj";
	if (where) { 
		sql << " and " << where;
	}
	if (order_by) { 
		sql << " order by " << order_by;
	}
	if (limit != 0) { 
		sql << " limit " << limit;
	}
	if (offset != 0) { 
		sql << " offset " << offset;
	}
	result rs = txn->exec(sql.str());
	buf.put(0); // reset buffer
	for (result::const_iterator i = rs.begin(); i != rs.end(); ++i) {
		result::tuple obj_record = *i;
		objref_t obj_opid = obj_record["opid"].as(objref_t());
		class_descriptor* obj_desc = lookup_class(GET_CID(obj_opid));
		unpack_object("", obj_desc, buf, obj_record); 
	}
}
	

void pgsql_storage::query(objref_t& first_mbr, objref_t last_mbr, char const* query, nat4 buf_size, int flags, nat4 max_members, dnm_buffer& buf)
{
	autocommit txn(this);
	load(first_mbr, flags, buf);
	stid_t sid;
	objref_t obj;
	unpackref(sid, obj, &buf + sizeof(dbs_object_header) + 3*sizeof(dbs_reference_t));
	cpid_t cpid = GET_CID(obj);
	class_descriptor* desc = lookup_class(cpid);
	std::string table_name = get_table(desc);
	std::stringstream sql;
	char const* follow = (flags & qf_backward) ? "m.next" : "m.prev";
	sql << "with recursive set_members(no,opid,obj) as (select 1,m.opid,m.obj from set_member m where m.opid=" << first_mbr << " union all select s.no+1,m.opid,m.obj from set_member m join set_members s on " << follow << "=s.opid";
	if (last_mbr != 0) { 
		sql << " where " << follow << " <> " << last_mbr;
	}
	sql << ") select m.opid as mbr_opid,m.next as mbr_next,m.prev as mbr_prev,m.owner as mbr_owner,m.obj as mbr_obj,m.key as mbr_key,t.* from set_members s, set_member m, " << table_name << " t where m.opid=s.opid and t.opid=s.obj";
	if (query && *query) { 
		sql << " and " << query;
	}
	sql << " order by s.no limit " << max_members;
	result rs = txn->exec(sql.str());
	buf.put(0); // reset buffer
	first_mbr = load_query_result(rs, buf, last_mbr, flags);
}


void pgsql_storage::load(objref_t* opp, int n_objects, 
						 int flags, dnm_buffer& buf)
{
	autocommit txn(this);
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
			result rs = txn->prepared(get_table(desc) + ((flags & lof_update) ? "_loadobj_for_update" : "_loadobj"))(opid).exec();
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
	bool toplevel;
	buf.put(0);
	start_transaction(toplevel);
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

static void store_int_array(invocation& stmt, char* src_bins, int elem_size, int length, int allocated)
{
	int i;
	std::stringstream buf;
	assert(length <= allocated);
	buf << '{';
	for (i = 0; i < length; i++) { 
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
	while (i < allocated) { 
		if (i != 0) {
			buf << ',';
		}
		buf << "0";
		i += 1;
	}
	buf << '}';
	stmt(buf.str());	
}

std::string convertString(std::string const& val, char const* encoding) 
{
	if (val.empty()) { 
		return val;
	}
#ifdef _WIN32
	std::vector<wchar_t> wchars(val.size());
	int cp;
	if (strcmp(encoding, "ansi") == 0)  { 
		cp = CP_ACP;
	} else { 
		cp = CP_OEMCP;
	}
	int len = MultiByteToWideChar(cp, 0/*MB_ERR_INVALID_CHARS*/, val.data(), val.size(), &wchars[0], wchars.size()); 
	if (len == 0) { 
		fprintf(stderr, "Failed to convert string '%s' to UTF-16\n", val.c_str());
		return "";
	}	
	std::vector<char> chars(len*4);
	len = WideCharToMultiByte(CP_UTF8, 0/*WC_ERR_INVALID_CHARS*/, &wchars[0], len, &chars[0], len*2, NULL, NULL);
	if (len == 0) { 
		fprintf(stderr, "Failed to convert string '%s' to UTF-8\n", val.c_str());
		return val;
	}	
	return std::string(&chars[0], len);
#else
	iconv_t ic = iconv_open("UTF-8", encoding);
	if (ic < 0) {
		fprintf(stderr, "Usupported charset %s\n", encoding);
		return "";
	}
	std::vector<char> chars(val.size()*2);
	size_t srcSize = val.size();
	size_t dstSize = chars.size();
	char* src = (char*)val.c_str();
	char* dst = &chars[0];
	size_t rc = iconv(ic, &src, &srcSize, &dst, &dstSize);
	iconv_close(ic);
	if (rc == (size_t)-1) {
		fprintf(stderr, "Failed to convert string '%s' to UTF-8\n", val.c_str());
		return "";
	}
	return std::string(&chars[0], rc);
#endif
}

void unpack_string(invocation& stmt, char* body, size_t len)
{
	std::vector<wchar_t> buf(len+1);
	for (size_t i = 0; i < len; i++) { 
		buf[i] = unpack2(body);
		body += 2;
	}
#ifdef _WIN32	
	std::vector<char> chars(len*4 + 1);
	if (len != 0) { 
		len = WideCharToMultiByte(CP_UTF8, 0/*WC_ERR_INVALID_CHARS*/, &buf[0], len, &chars[0], len*4, NULL, NULL);
		assert(len != 0);
	}
	stmt(std::string(&chars[0], len));
#else
	buf[len] = 0;
	wstring_t wstr(&buf[0]);
	char* chars = wstr.getChars();
	assert(chars != NULL);
	stmt(chars);
	delete[] chars;
#endif
}

size_t pgsql_storage::store_struct(field_descriptor* first, invocation& stmt, char* &src_refs, char* &src_bins, size_t size, bool is_zero_terminated, char const* encoding)
{	
	if (first == NULL) {
		return size;
	}
	autocommit txn(this);
	field_descriptor* field = first;	
    do { 
		int offs = field->loc.offs;
		if (field->dbs.n_items != 1) { 
			switch (field->dbs.type) { 
			  case fld_unsigned_integer:
				if (field->dbs.size == 1) { 
					if (field->dbs.n_items == 0) {
						std::string val(src_bins, size);
						if (offs >= 0) {
							stmt(txn->esc_raw(val));
						}
						src_bins += size;
						size = 0;
					} else { 
						std::string val(src_bins, field->dbs.n_items);
						if (offs >= 0) {
							stmt(txn->esc_raw(val));
						}
						src_bins += field->dbs.n_items;
						size -= field->dbs.n_items;
					}
				} else { 
					if (field->dbs.n_items == 0) {
						if (offs >= 0) {
	                        int used = (int)(size/field->dbs.size);
							if (field->loc.type == fld_string && field->dbs.size == 2) { 
								unpack_string(stmt, src_bins, used);
							} else { 
								int allocated = used;
								if (field->loc.n_items != 0 && field->loc.n_items != used) { 
									if (field->loc.n_items < used) { 
										fprintf(stderr, "Array is truncated from %d to %d\n",
												used, field->loc.n_items);
										used = allocated = field->loc.n_items;
									} else { 
										allocated = field->loc.n_items;
									}
								}
								store_int_array(stmt, src_bins, field->dbs.size, used, allocated);							
							}
						}
						src_bins += size;
						size = 0;
					} else { 
						if (offs >= 0) {
							int used = field->dbs.n_items;
							if (field->loc.type == fld_string && field->dbs.size == 2) { 
								unpack_string(stmt, src_bins, used);
							} else { 
								int allocated = used;
								if (field->loc.n_items != 0 && field->loc.n_items != used) { 
									if (field->loc.n_items < used) { 
										fprintf(stderr, "Array is truncated from %d to %d\n",
												used, field->loc.n_items);
										used = allocated = field->loc.n_items;
									} else { 
										allocated = field->loc.n_items;
									}
								}
								store_int_array(stmt, src_bins, field->dbs.size, used, allocated);
							}
						}
						src_bins += field->dbs.n_items*field->dbs.size;
						size -= field->dbs.n_items*field->dbs.size;
					}
				}
				break;

			  case fld_signed_integer:
				if (field->dbs.size == 1) {
					if (field->dbs.n_items == 0) {
						std::string val(src_bins, is_zero_terminated && size != 0 ? strlen(src_bins) : size);
						if (offs >= 0) {
							if ((field->flags & fld_binary) || field->loc.type == fld_raw_binary) {
								stmt(txn->esc_raw(val));
							} else { 
								if (encoding) { 
									val = convertString(val, encoding);
								}
								stmt(val);
							}
						}
						src_bins += size;
						size = 0;
					} else { 
						std::string val(src_bins, field->dbs.n_items);
						if (offs >= 0) {
							if ((field->flags & fld_binary) || field->loc.type == fld_raw_binary) {
								stmt(txn->esc_raw(val));
							} else { 
								if (encoding) { 
									val = convertString(val, encoding);
								}
								stmt(val);
							}
						}
						src_bins += field->dbs.n_items;
						size -= field->dbs.n_items;
					}
				} else { 
					if (field->dbs.n_items == 0) {
						if (offs >= 0) {
							int used = (int)(size/field->dbs.size);
							int allocated = used;
							if (field->loc.n_items != 0 && field->loc.n_items != used) { 
								if (field->loc.n_items < used) { 
									fprintf(stderr, "Array is truncated from %d to %d\n",
											used, field->loc.n_items);
									used = allocated = field->loc.n_items;
								} else { 
									allocated = field->loc.n_items;
								}
							}
							store_int_array(stmt, src_bins, field->dbs.size, used, allocated);
						}
						src_bins += size;
						size = 0;
					} else { 
						if (offs >= 0) {						
							int used = field->dbs.n_items;
							int allocated = used;
							if (field->loc.n_items != 0 && field->loc.n_items != used) { 
								if (field->loc.n_items < used) { 
									fprintf(stderr, "Array is truncated from %d to %d\n",
											used, field->loc.n_items);
									used = allocated = field->loc.n_items;
								} else { 
									allocated = field->loc.n_items;
								}
							}
							store_int_array(stmt, src_bins, field->dbs.size, used, allocated);
						}
						src_bins += field->dbs.n_items*field->dbs.size;
						size -= field->dbs.n_items*field->dbs.size;
					}					
				}
				break;
			  default:
				assert(false);
			}
		} else {
			switch(field->dbs.type) { 
			  case fld_reference:
			  {
				  objref_t opid;
				  stid_t sid;
				  src_refs = unpackref(sid, opid, src_refs);
				  size -= sizeof(dbs_reference_t);
				  if (offs >= 0) { 
					  stmt(opid);
				  }
				  break;			  
			  }
			  case fld_string:
			  {
				  size_t len = unpack2(src_bins);
				  src_bins += 2;
				  size -= 2;
				  if (len != 0xFFFF) { 
					  if (offs >= 0) { 
						  unpack_string(stmt, src_bins, len);
					  }
					  src_bins += len*2;
					  size -= len*2;
				  } else if (offs >= 0) {
					  stmt(); // store NULL
				  }
				  break;
			  }
			  case fld_raw_binary:
			  {
				  size_t len = unpack4(src_bins);
				  std::string blob(src_bins+4, len);
				  if (offs >= 0) {
					  stmt(txn->esc_raw(blob));
				  }
				  src_bins += 4 + len;
				  size -= 4 + len;
				  break;
			  }
			  case fld_signed_integer:
				switch (field->dbs.size) { 
				  case 1:	
				    if (offs >= 0) {
					    stmt((int2)*src_bins);
				    }
				    src_bins += 1;
					size -= 1;
					break;
				  case 2:
				    if (offs >= 0) {
						stmt((int2)unpack2(src_bins));
					}
					src_bins += 2;
					size -= 2;
					break;
				  case 4:
				    if (offs >= 0) {
						stmt((int4)unpack4(src_bins));
					}
					src_bins += 4;
					size -= 4;
					break;
				  case 8:
				  {
					  int8 ival;
					  src_bins = unpack8((char*)&ival, src_bins);
					  if (offs >= 0) {
						  stmt(ival);
					  }
					  size -= 8;
					  break;
				  }
				  default:
					assert(false);
				}
				break;
			  case fld_unsigned_integer:
				switch (field->dbs.size) { 
				  case 1:	
					//stmt((nat2)*src_bins++);
				    if (offs >= 0) {
						stmt((int2)*src_bins);
					}
					src_bins += 1;
					size -= 1;
					break;
				  case 2:
					//stmt(unpack2(src_bins));
				    if (offs >= 0) {
						stmt((int2)unpack2(src_bins));
					}
					src_bins += 2;
					size -= 2;
					break;
				  case 4:
					//stmt(unpack4(src_bins));
				    if (offs >= 0) {
						stmt((int4)unpack4(src_bins));
					}
					src_bins += 4;
					size -= 4;
					break;
				  case 8:
				  {
					  int8 ival;
					  src_bins = unpack8((char*)&ival, src_bins);
					  if (offs >= 0) {
						  stmt(ival);
					  }
					  size -= 8;
					  break;
				  }
				  default:
					assert(false);
				}
				break;
			  case fld_real:
				switch (field->dbs.size) { 
				  case 4:
				  {					  
					  if (offs >= 0) {
						  union {
							  float f;
							  int4  i;
						  } u;
						  u.i = unpack4(src_bins);
						  stmt(u.f);
					  }
					  src_bins += 4;
					  size -= 4;
					  break;
				  }
				  case 8:
				  {
					  double f;
					  src_bins = unpack8((char*)&f, src_bins);
					  if (offs >= 0) {
						  stmt(f);
					  }
					  size -= 8;
					  break;
				  }
				  default:
					assert(false);
				}
				break;
			  case fld_structure:
 			    size = store_struct(field->components, stmt, src_refs, src_bins, size, is_zero_terminated, encoding);
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
	{
		autocommit txn(this);
		assert(n_trans_servers == 1);
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
					size_t size = hdr->get_size();
					size_t left = store_struct(desc->fields, stmt, src_refs, src_bins, size, is_text_set_member(desc), NULL);
					assert(left == 0);
				}
				result r = stmt.exec();
				assert(r.affected_rows() == 1);

				// mark object as updated
				r = txn->prepared("mkversion")(opid)(clientID).exec();
				assert(r.affected_rows() == 1);
			}
			ptr = (char*)(hdr + 1) + hdr->get_size();
		}
	}
	commit_transaction();
	return true;
}

void pgsql_storage::rollback_transaction()
{
    transaction_manager* mng = transaction_manager::get();
	pgsql_extension* extension = (pgsql_extension*)mng->extension;
	if (extension != NULL) { 
		pgsql_session* session = extension->session;
		if (session->txn != NULL) { 
			session->txn->abort();
			//printf("Abort transaction\n");
			delete session->txn;
			session->txn = NULL;
		}
	}
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
	autocommit txn(this);
	result rs = txn->exec("select pg_database_size(current_database())");	
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
	autocommit txn(this);
	return txn->prepared(name);
}

ref<set_member> pgsql_storage::index_find(database const* db, objref_t index, char const* op, std::string const& key)
{
	ref<set_member> mbr;
	objref_t opid;
	{
		autocommit txn(this); 
		result rs = txn->prepared(op)(index)(txn->esc_raw(key)).exec();
		size_t size = rs.size();
		if (size == 0) { 
			return NULL;
		}
		assert(size == 1);
		result::tuple record = rs[0];
		opid = record[0].as(objref_t());
	}
	((database*)db)->get_object(mbr, opid);
	return mbr;
}


//
// Emulation of GOODS B_tree
// 

REGISTER_EX(pgsql_index, B_tree, pessimistic_repeatable_read_scheme, class_descriptor::cls_hierarchy_super_root);

field_descriptor& pgsql_index::describe_components()
{
    return NO_FIELDS;
}

ref<set_member> pgsql_index::find(const char* str, size_t len, skey_t key) const
{
	pgsql_storage* pg = get_storage(this);
	std::string strkey(str, len != 0 && str[len-1] == '\0' ? len-1 : len);
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
	std::string strkey(str, len != 0 && str[len-1] == '\0' ? len-1 : len);
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
	if (mbr->cls.class_attr & class_descriptor::cls_binary) {
	skey_t key = mbr->get_key();
		next = findGE(key);
		if (pg == NULL) { 
			pack8(key);
			mem_index.insert(std::pair< std::string, ref<set_member> >(std::string((char*)&key, sizeof(key)), mbr));
		}			
	} else { 
		char key[MAX_KEY_SIZE];
		size_t keySize = mbr->copyKeyTo(key, MAX_KEY_SIZE);
		assert(keySize < MAX_KEY_SIZE);
		next = findGE(key, keySize, 0); // skey is not used here
		if (pg == NULL) { 
			if (keySize != 0 && key[keySize-1] == '\0') keySize -= 1;
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
	if (pg == NULL) {
		std::string strkey;
		if (mbr->cls.class_attr & class_descriptor::cls_binary) {
			skey_t key = mbr->get_key();
			pack8(key);
			strkey = std::string((char*)&key, sizeof key);
		} else { 
			char key[MAX_KEY_SIZE];
			size_t keySize = mbr->copyKeyTo(key, MAX_KEY_SIZE);
			assert(keySize < MAX_KEY_SIZE);
			if (keySize != 0 && key[keySize-1] == '\0') keySize -= 1;
			strkey = std::string(key, keySize);
		}
		auto iterpair = mem_index.equal_range(strkey);		
		for (auto it = iterpair.first; it != mem_index.end() && it != iterpair.second; ++it) {
			if (it->second == mbr) {
				mem_index.erase(it);
				break;
			}
		}
	} else {
		objref_t opid = mbr->get_handle()->opid;
		pgsql_storage::autocommit txn(pg);
		pg->statement("index_del")(hnd->opid)(opid).exec();
	}
	set_owner::remove(mbr);
}

void pgsql_index::clear()
{
	pgsql_storage* pg = get_storage(this);
	if (pg != NULL) { 
		pgsql_storage::autocommit txn(pg);
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

REGISTER_EX(pgsql_dictionary, dictionary, pessimistic_repeatable_read_scheme, class_descriptor::cls_hierarchy_super_root);

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
	pgsql_storage::autocommit txn(pg);
	pg->statement("hash_put")(hnd->opid)(name)(obj_hnd->opid).exec();	
}

anyref pgsql_dictionary::get(const char* name) const
{
	anyref ref;
	objref_t obj;
	{
		pgsql_storage* pg = get_storage(this);
		pgsql_storage::autocommit txn(pg);
		result rs = pg->statement("hash_get")(hnd->opid)(name).exec();
		if (rs.empty()) { 
			return NULL;
		}
		obj = rs[0][0].as(objref_t());
	}
	((database*)get_database())->get_object(ref, obj, 0);
	return ref;
}

boolean pgsql_dictionary::del(const char* name) 
{
	pgsql_storage* pg = get_storage(this);
	pgsql_storage::autocommit txn(pg);
	result rs = pg->statement("hash_delall")(hnd->opid)(name).exec();
	boolean found = rs.affected_rows() != 0;
	return found;
}

boolean pgsql_dictionary::del(const char* name, anyref obj) 
{
	hnd_t obj_hnd = obj->get_handle();
	if (obj_hnd == NULL) { 
		return false;
	}
	pgsql_storage* pg = get_storage(this);
	pgsql_storage::autocommit txn(pg);
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
	pgsql_storage::autocommit txn(pg);
	pg->statement("hash_drop")(hnd->opid).exec();
}

size_t pgsql_dictionary::get_number_of_elements() const
{
	pgsql_storage* pg = get_storage(this);
	pgsql_storage::autocommit txn(pg);
	result rs = pg->statement("hash_size")(hnd->opid).exec();
	assert(rs.size() == 1);
	int64_t n_elems = rs[0][0].as(int64_t());
	return n_elems;
}

time_t pgsql_storage::GetTime() 
{ 
    autocommit txn(this); 
    result rs = txn->prepared("get_time").exec();
    assert(rs.size() == 1);
    return (time_t)rs[0][0].as(double());
}

nat4 pgsql_storage::GetCurrentUsersCount(char const* appId, nat4 &nInstances)
{
    autocommit txn(this); 
	char appIdPattern[8];
	strcat(strcpy(appIdPattern, appId), "%");
    result rs = txn->prepared("get_clients")(appIdPattern).exec();
    assert(rs.size() == 1);
    nInstances = rs[0][1].as(nat4());
    return rs[0][0].as(nat4());
}

std::string pgsql_storage::GetCurrentConnectionString()
{
    return connString;
}

int pgsql_storage::execute(char const* sql)
{
    autocommit txn(this); 
	result rs = txn->exec(sql);
	return rs.affected_rows();
}
	
std::string int2string_hex(long long x) { 
	char buf[64];
	sprintf(buf, "%llx", x);
	return std::string(buf);
}

std::string int2string_dec(long long x) { 
	char buf[64];
	sprintf(buf, "%lld", x);
	return std::string(buf);
}

int pgsql_storage::get_socket()
{
	pgsql_session* session = current_session();
	return session->con->sock();
}

void pgsql_storage::process_notifications()
{
	pgsql_session* session = current_session();
	session->con->get_notifs();
}

pgsql_storage::listener::listener(connection_base &c, const PGSTD::string &channel, event& e) 
: notification_receiver(c, channel), notification(&e) {}

void pgsql_storage::listener::operator()(const PGSTD::string &payload, int backend_pid)
{
	notification->signal();
}


void pgsql_storage::listen(hnd_t hnd, event& e)
{
	std::string id = int2string_dec(hnd->opid);
	std::string channel = std::string("chan") + id;
	auto it = observers.find(channel);
	if (it == observers.end()) { 
		autocommit txn(this); 
		class_descriptor* root_class = get_root_class(&hnd->obj->cls);
		txn->exec(std::string("create trigger ") + channel + " after update on " + root_class->name + " for each row when (NEW.opid=" + id + ") execute procedure on_update('" + channel + "')");
		observers[channel] = new listener(txn->conn(), channel, e);
	} else { 
		it->second->notification = &e;
	}
}

void pgsql_storage::unlisten(hnd_t hnd, event& e)
{
	std::string id = int2string_dec(hnd->opid);
	std::string channel = std::string("chan") + id;
	auto it = observers.find(channel);
	if (it != observers.end()) { 
		autocommit txn(this); 
		delete it->second;
		txn->exec(std::string("drop trigger ") + channel);
		observers.erase(it);
	}
}

#if GOODS_DATABASE_CONVERTER

#include "mmapfile.h"
#include "osfile.h"
#include "memmgr.h"
#include "fstream"

static inline size_t get_opid_hash(opid_t opid)
{
	static const int hash_const = 15;
	return (opid >> hash_const) << hash_const;
}

inline char* unpack6(nat2& sid, opid_t& opid, char* src) { 
    return unpack4((char*)&opid, unpack2((char*)&sid, src));
}

inline objref_t makeref(cpid_t cpid, opid_t opid) 
{
	return opid == ROOT_OPID ? ROOT_OPID : MAKE_OBJREF(cpid, opid);
}

static void map_classes(dbs_class_descriptor* desc)
{
	for (size_t i = 0; i < desc->n_fields; i++) { 
		char* name = &desc->names[desc->fields[i].name];
		if (strncmp(name, "SB_tree", 7) == 0) {
			strcpy(&desc->names[desc->fields[i].name], "B_tree");
			break;
		}
	}
}


boolean pgsql_storage::convert_goods_database(char const* databasePath, char const* databaseName)
{
	pgsql_storage::autocommit txn(this);

	txn.commitOnExit = false;

	std::string idx_path = std::string(databasePath) + "/" + databaseName + ".idx";
	std::string odb_path = std::string(databasePath) + "/" + databaseName + ".odb";
	std::string blob_path = std::string(databasePath) + "/blobs/";

	dnm_buffer buf;

    mmap_file idx_file(idx_path.c_str(), 4*1024*1024);
    os_file   odb_file(odb_path.c_str());
	msg_buf   msgbuf;
	file::iop_status status = idx_file.open(file::fa_readwrite,
											file::fo_random|file::fo_create);
	if (status != file::ok) {
		idx_file.get_error_text(status, msgbuf, sizeof msgbuf);
		fprintf(stderr, "Failed to open index file %s: %s\n", idx_path.c_str(), msgbuf);
		return false;
	}
	dbs_handle* index_beg = (dbs_handle*)idx_file.get_mmap_addr();
		
	status = odb_file.open(file::fa_readwrite,
						   file::fo_largefile|file::fo_random|file::fo_directio);
	if (status != file::ok) {
		odb_file.get_error_text(status, msgbuf, sizeof msgbuf);
		fprintf(stderr, "Failed to open database file %s: %s\n", odb_path.c_str(), msgbuf);
		return false;
	}

    dnm_queue<opid_t> queue;
    dnm_array<nat4>   bitmap;
	dnm_array<nat8>   objrefs;
	dnm_array<dbs_class_descriptor*> goods_classes;
	dnm_array<class_descriptor*> orm_classes;
	opid_t max_opid = ROOT_OPID;
	cpid_t max_cpid = RAW_CPID;

	queue.put(ROOT_OPID);
	bitmap[ROOT_OPID >> 5] |= 1 << (ROOT_OPID & 31);

    do { 
        opid_t opid = queue.get();
		dbs_handle* hnd = &index_beg[opid];
		cpid_t cpid = hnd->get_cpid();
		dbs_class_descriptor* dbs_desc = goods_classes[cpid];
		class_descriptor* desc = orm_classes[cpid];
		if (desc == NULL) {
			dbs_handle* cls_hnd = &index_beg[cpid];
			size_t dbs_desc_size = cls_hnd->get_size();
			dbs_desc = (dbs_class_descriptor*)new char[dbs_desc_size]; // reserve space for pgsql_index
			status = odb_file.read(cls_hnd->get_pos(), dbs_desc, dbs_desc_size);
			if (status != file::ok) {
				odb_file.get_error_text(status, msgbuf, sizeof msgbuf);
				fprintf(stderr, "Failed to read class descriptor %d: %s\n", cpid, msgbuf);
				return false;
			}
			dbs_desc->unpack();
			map_classes(dbs_desc);

			char* class_name = dbs_desc->name();
			desc = class_descriptor::find(class_name);
			if (desc == NULL) { 
				fprintf(stderr, "Failed to locate class %s\n", class_name);
				return false;
			}
			if (opid == ROOT_OPID) { 
				txn->prepared("add_root")(cpid).exec(); // store information about root 
			}

			assert(desc != &hash_item::self_class); // should not be accessed because we traverse hash_table in special way

			class_descriptor* dst_desc = (desc == &hash_table::self_class)
				? &pgsql_dictionary::self_class : is_btree(desc) ? &pgsql_index::self_class : desc;
			desc = (desc == dst_desc && *desc->dbs_desc != *dbs_desc && strcmp(desc->name, "set_member") != 0) ? new class_descriptor(cpid, desc, dbs_desc) : dst_desc;

			goods_classes[cpid] = dbs_desc;
			orm_classes[cpid] = desc;

			std::vector<std::string> columns;
			class_descriptor* root_class = get_root_class(desc);
			std::string table_name(root_class->name);
			if (desc != dst_desc) { 
				compare_classes(desc->name, cpid, "", desc->fields, dst_desc->fields);
			}
			get_columns("", desc->fields, columns, get_inheritance_depth(desc));
			std::stringstream sql;			
			sql << "insert into " << table_name << " (opid";
			for (size_t i = 0; i < columns.size(); i++) { 
				sql << ",\"" << columns[i] << '\"';
			}
			sql << ") values ($1";
			for (size_t i = 0; i < columns.size(); i++) { 
				sql << ",$" << (i+2);
			}
			sql << ")";
			txn->conn().prepare(std::string(desc->name) + "_convert_" + int2string_hex(cpid), sql.str());

			std::string desc_data((char*)dst_desc->dbs_desc, dst_desc->dbs_desc->get_size());
			((dbs_class_descriptor*)desc_data.data())->pack();
			txn->prepared("put_class")(cpid)(desc->name)(txn->esc_raw(desc_data)).exec();						
		}
		if (opid > max_opid) { 
			max_opid = opid;
		}
		if (cpid > max_cpid) { 
			max_cpid = cpid;
		}
		objref_t ref = makeref(cpid, opid);
		if (strcmp(desc->name, "ExternalBlob") == 0) { 
			std::ifstream ifs(blob_path + int2string_hex(get_opid_hash(opid)) + std::string("\\") + int2string_hex(opid) + ".blob", 
							  std::ifstream::binary);
			if (ifs)
			{				
				ifs.seekg(0, std::ios::end);				
				std::streampos length = ifs.tellg();
				ifs.seekg(0, std::ios::beg);                   
				std::vector<char> buffer(length);
				ifs.read(&buffer[0], length);
				std::string blob(&buffer[0], length);				
				txn->prepared("write_file")(txn->esc_raw(blob))(ref).exec();
			}
			continue;
		} else if (desc->class_attr & class_descriptor::cls_non_relational)  { 
			continue; // skip non-relational classes	
		}
		size_t size = hnd->get_size();
		buf.put(size);
		status = odb_file.read(hnd->get_pos(), &buf, size);
		if (status != file::ok) {
			odb_file.get_error_text(status, msgbuf, sizeof msgbuf);
			fprintf(stderr, "Failed to read object %x: %s\n", opid, msgbuf);
			return false;
		}
		size_t n_refs = dbs_desc->get_number_of_references(size);
		size_t orig_size = size;
		size += 2*n_refs; // ORM refenreces are 8 bytes instead of 6-bytes GOODS referenced
		
		char* src_refs = &buf;			   
		char* src_bins = src_refs + n_refs*6;
		char const* encoding = getenv("GOODS_ENCODING");
		invocation stmt = txn->prepared(std::string(desc->name) + "_convert_" + int2string_hex(cpid)) ;
		stmt(ref);

		if (desc == &pgsql_dictionary::self_class) { 
			dnm_buffer item_buf;
			// store hash table
			for (size_t i = 0; i < n_refs; i++) { 
				stid_t sid;
				opid_t obj, item;
				src_refs = unpack6(sid, item, src_refs);
				while (item != 0) {
					hnd = &index_beg[item];
					size = hnd->get_size();
					item_buf.put(size);
					status = odb_file.read(hnd->get_pos(), &item_buf, size);
					if (status != file::ok) {
						odb_file.get_error_text(status, msgbuf, sizeof msgbuf);
						fprintf(stderr, "Failed to read hash_item %x: %s\n", item, msgbuf);
						return false;
					}
					char* body = &item_buf;
					body = unpack6(sid, item, body);
					body = unpack6(sid, obj, body);
					txn->prepared("hash_put")(ref)(body)(makeref(index_beg[obj].get_cpid(), obj)).exec();
					if (obj != 0 && !(bitmap[obj >> 5] & (1 << (obj & 31)))) {
						bitmap[obj >> 5] |= 1 << (obj & 31);
						queue.put(obj);
					}
				}
			}
		} else { 
			for (size_t i = 0; i < n_refs; i++) { 
				stid_t sid;
				opid_t obj;
				src_refs = unpack6(sid, obj, src_refs);
				if (obj != 0) { 
					objrefs[i] = makeref(index_beg[obj].get_cpid(), obj);
					pack8(objrefs[i]);
					if (!(bitmap[obj >> 5] & (1 << (obj & 31)))) {
						bitmap[obj >> 5] |= 1 << (obj & 31);
						queue.put(obj);
					}
				} else {
					objrefs[i] = 0;
				}
			}
			assert(src_refs == src_bins);
			if (desc->n_varying_references != 0) { 
				store_array_of_references(stmt, (char*)&objrefs, src_bins);
			} else { 
				bool is_text = is_text_set_member(desc);
				if (!is_text && desc->base_class == &set_member::self_class) {
					if (&buf + orig_size == src_bins + 8) {
						pack8(*(nat8*)src_bins);
					} else { 
						assert(&buf + orig_size == src_bins + 4);
						pack4(*(nat4*)src_bins);
					}						
				}
				src_refs = (char*)&objrefs;
				size_t left = store_struct(desc->fields, stmt, src_refs, src_bins, size, is_text, encoding);
				assert(left == 0);
			}
		}
		result r = stmt.exec();
		assert(r.affected_rows() == 1);
    } while (!queue.is_empty());

	for (cpid_t i = RAW_CPID; i <= max_cpid; i++) { 
		delete[] (char*)goods_classes[i];
	}

	txn->prepared("set_oid")(max_opid).exec();
	txn->prepared("set_cid")(max_cpid).exec();
	
	idx_file.close();
	odb_file.close();

	txn.commitOnExit = true;

	return true;
}
	
#else

boolean pgsql_storage::convert_goods_database(char const* databasePath, char const* databaseName)
{
	return false;
}

#endif
