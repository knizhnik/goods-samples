#include "stdafx.h"

#include "DatabaseRoot.h"
#include "FillDatabase.h"
#include "GoodsSampleApp.h"
#include "magaya_client_storage.h"
#include "RootDataLayer.h"
#include "Storage.h"

#include <locale.h>
#include <iostream>
#include <fstream>

CGoodsSampleApp::CGoodsSampleApp()
{
}

boolean CGoodsSampleApp::open(const char* dbs_name, char const* login, char const* password)
{
	// -- attempt to open the database without authentication
	return db::OpenDatabase(dbs_name, "", "");
}

void CGoodsSampleApp::close()
{
	db::CloseDatabase();
}

static db::db_filler::FillDatabaseData CreateFillData()
{
	db::db_filler::FillDatabaseData fill_data;

	fill_data.WarehouseReceiptCount = 1000;
	fill_data.MaxItemsPerWarehouseReceipt = 30;

	return fill_data;
}

void CGoodsSampleApp::run()
{
	transaction_manager::set_isolation_level(PER_THREAD_TRANSACTION);

	// -- ensure database structure creation
	auto db_root = db::GetDatabaseRoot();
 
	time_t start = time(NULL);
	db::db_filler::FillDatabase(CreateFillData());
	console::output("time for insertion %d sec\n", time(NULL) - start);
      
	start = time(NULL);
	db::db_filler::TestIntegrity();
	console::output("time for integrity check %d sec\n", time(NULL) - start);
 
	start = time(NULL);
	db::db_filler::DeleteAll();
	console::output("time for deletion %d sec\n", time(NULL) - start);
}

