using namespace pqxx;

#define MAX_TABLES 16
#define GET_TID(x) ((x) % MAX_TABLES)
#define GET_OID(x) ((x) / MAX_TABLES)
#define CONS_OPID(t,o) ((o)*MAX_TABLES + (t))
#define OID_BIAS 0x10000

void pgsql_storage::pgsql_storage(char const* connStr) : con(connStr), tableMap(class_descriptor::last_ctid)
{
	con.prepare("alloc", "select nextval('oid_sequence') from generate_series(1,$1"); 
	con.prepare("get_class", "select desc from classes where oid=?"); 
	con.prepare("put_class", "insert into classes (desc) values ($1)"); 
	con.prepare("change_class", "update classes set desc=$1 where oid=$2"); 

	for (size_t i = 0; i < DESCRIPTOR_HASH_TABLE_SIZE; i++) { 
		for (class_descriptor* cls = class_descriptor::hash_table[i]; cls != NULL; cls = cls->next) { 
			if (cls->base_class == &object::self_class) { 
				tableMap[cls->ctid] = tables.size();
				tables.push_back(cls->name);
				con.prepare(tables.back() + "_delete", string("delete from ") + tables.back() + " where oid=$1"); 
			} else { 
				class_descriptor* base;
				for (base = cls->base_class; base->base_class == &object::self_class; base = base->base_class);
				tableMap[cls->ctid] = tableMap[base->ctid];
			}
		}
	}
}

void pgsql_storage::bulk_allocate(size_t sizeBuf[], cpid_t cpidBuf[], size_t nAllocObjects, 
                                       opid_t opidBuf[], size_t nReservedOids, hnd_t clusterWith[])
{
	result rs = txn->prepared("alloc")(nReservedOids).exec();
	for (size_t i = 0; i < nReservedOids; i++) { 
		opidBuf[i] = rs[i][0].as(opid_t());
	}
}

opid_t pgsql_storage::allocate(cpid_t cpid, size_t size, int flags, opid_t clusterWith)
{
	opid_t opid;
	bulk_allocate(NULL, NULL, 0, &opid, 1, NULL);
	return opid;
}

void dbs_client_storage::deallocate(opid_t opid)
{ 
	txn->prepared(tables[tablesMap[GET_TID(opid)]] + "_delete")(GET_OID(opid)).exec();
}

boolean dbs_client_storage::lock(opid_t opid, lck_t lck, int attr)
{
	return true;
}

void dbs_client_storage::unlock(opid_t opid, lck_t lck)
{
}

void dbs_client_storage::get_class(cpid_t cpid, dnm_buffer& buf)
{
	result rs = txt->prepared("get_class")(cpid + OID_BIAS).exec();
	binarystring desc = rs[0][0].as(binarystring());
	memcpy(buf.put(desc.size()), desc.data());
}

cpid_t dbs_client_storage::put_class(dbs_class_descriptor* dbs_desc)
{
	size_t dbs_desc_size = dbs_desc->get_size();
	binarystring buf(dbs_desc, dbs_desc_size);
	((dbs_class_descriptor*)buf.data())->pack();
	result rs = txt->prepared("put_class")(data).exec();
	return rs.oid() - OID_BIAS;
}

void dbs_client_storage::change_class(cpid_t cpid, 
                                      dbs_class_descriptor* dbs_desc)
{
	size_t dbs_desc_size = dbs_desc->get_size();
	binarystring buf(dbs_desc, dbs_desc_size);
	((dbs_class_descriptor*)buf.data())->pack();
	txt->prepared("change_class")(data,cpid + OID_BIAS).exec();
}
	

void dbs_client_storage::query(opid_t& next_mbr, char const* query, nat4 buf_size, int flags, nat4 max_members, dnm_buffer& buf)
{
}
