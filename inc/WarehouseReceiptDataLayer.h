
#pragma once

#include "WarehouseReceipt.h"

class CWarehouseItem;
class CWarehouseReceiptList;

namespace db
{
	namespace warehouse_receipt	
	{
		ref<CWarehouseReceipt> Create(const wchar_t* number);
		void AddItem(w_ref<CWarehouseReceipt> w_warehouse_receipt, ref<CWarehouseItem> const& item);
		void Save(ref<CWarehouseReceipt> const& warehouse_receipt);
		size_t GetCount();
		ref<CWarehouseReceiptList> GetList();

	}	// namespace warehouse_receipt

}	// namespace db