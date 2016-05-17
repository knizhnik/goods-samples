#include "stdafx.h"
#include "Storage.h"
#include "RootDataLayer.h"


CDataBase& db::GetDatabase()
{
	static CDataBase database("el");
	return database;
}

bool db::OpenDatabase(const char* dbs_name, char const* login, char const* password)
{
#if PGSQL_ORM
	return GetDatabase().open(dbs_name, login, password);
#else
	return GetDatabase().Open(dbs_name);
#endif
}

void db::CloseDatabase()
{
	return GetDatabase().close();
}

static ref<CDatabaseRoot> EnsureDatabaseRoot(CDataBase& database)
{
	ref<CDatabaseRoot> db_root;
	database.get_root(db_root);

	if (!db_root->IsNewDatabase())
	{
		return db_root;
	}

	return db_root->create();
}

ref<CDatabaseRoot> db::GetDatabaseRoot()
{
	static ref<CDatabaseRoot> db_root = EnsureDatabaseRoot(GetDatabase());
	return db_root;
}
