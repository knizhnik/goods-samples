#pragma once

#include "DatabaseRoot.h"

class CDataBase;

namespace db
{

	CDataBase& GetDatabase();
	ref<CDatabaseRoot> GetDatabaseRoot();

	bool OpenDatabase(const char* dbs_name, char const* login, char const* password);
	void CloseDatabase();

}	// namespace db

