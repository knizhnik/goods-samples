
#pragma once

#include "WarehouseItem.h"

class CWarehouseItemList;

namespace db
{
	namespace wh_item
	{
		ref<CWarehouseItem> Create(ref<CItemDefinition> const& item_definition);
		void Save(ref<CWarehouseItem> const& warehouse_item);
		void SetItemDefinition(w_ref<CWarehouseItem> w_item, ref<CItemDefinition> const& item_definition);
		size_t GetCount();
		ref<CWarehouseItemList> GetList();

	}	// namespace wh_item

}	// namespace db