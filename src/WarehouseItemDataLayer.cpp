
#include "stdafx.h"
#include "DimensionDataLayer.h"
#include "IndexHelper.h"
#include "ItemDefinition.h"
#include "Package.h"
#include "RootDataLayer.h"
#include "WarehouseItemDataLayer.h"
#include "WarehouseItemlist.h"

ref<CWarehouseItemList> db::wh_item::GetList()
{
	return db::GetDatabaseRoot()->GetWarehouseItemList();
}

ref<CWarehouseItem> db::wh_item::Create(ref<CItemDefinition> const& item_definition)
{
	w_ref<CWarehouseItem> w_item = CWarehouseItem::create();

	SetItemDefinition(w_item, item_definition);
	return w_item;
}

void db::wh_item::SetItemDefinition(w_ref<CWarehouseItem> w_item, ref<CItemDefinition> const& item_definition)
{
	auto package = item_definition->GetPackage();
	
	auto source_dims = package->GetDimensions();	
	ref<CDimensions> item_dims = nullptr;
	if (!source_dims.is_nil())
	{
		item_dims = db::dimensions::Clone(source_dims);
	}
	
	auto description = item_definition->GetDescription();
	auto weight = item_definition->GetWeight();

	w_item->SetItemDefinition(item_definition);
	w_item->SetPackage(package);
	w_item->SetDimensions(item_dims);
	w_item->SetDescription(description.c_str());
	w_item->SetWeight(weight);
}

void db::wh_item::Save(ref<CWarehouseItem> const& warehouse_item)
{
	w_ref<CWarehouseItemList> list = GetList();

	auto insert_member = index::CreateMember(warehouse_item);
	list->insert(insert_member);
}

size_t db::wh_item::GetCount()
{
	return GetList()->n_members;
}
