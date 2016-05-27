
#pragma once

class CDatabaseRoot;

namespace db
{
	namespace db_filler
	{
		struct FillDatabaseData
		{
			size_t	WarehouseReceiptCount;
			size_t	MaxItemsPerWarehouseReceipt;
		};

		void FillDatabase(FillDatabaseData const& db_fill_data);
		bool TestIntegrity();		
		void DeleteAll();

	}	// namespace db_filler

}	// namespace db