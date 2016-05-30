
#include "stdafx.h"
#include "DimensionDataLayer.h"
#include "GlobalIndexController.h"
#include "HazardousItem.h"
#include "IndexHelper.h"
#include "ItemDefinition.h"
#include "Package.h"
#include "RootDataLayer.h"
#include "WarehouseItemDataLayer.h"
#include "WarehouseItemList.h"

ref<CWarehouseItemList> db::wh_item::GetList()
{
	return db::GetDatabaseRoot()->GetWarehouseItemList();
}

ref<CWarehouseItem> db::wh_item::Create(ref<CItemDefinition> const& item_definition)
{
	w_ref<CWarehouseItem> w_item = [&item_definition] () -> ref<CWarehouseItem>
	{
		if (item_definition->IsMarkedAs(CItemDefinitionFlags::HazMat))
		{
			return CHazardousItem::create();
		}
		else
		{
			return CWarehouseItem::create();			
		}
	}();

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

	auto insert_member = Index::CreateMember(warehouse_item);
	list->insert(insert_member);
	modify(warehouse_item)->SetIndexMember(WarehouseItemIndexMemberEnum::Main, insert_member);

	db::global_index::InsertObject(warehouse_item);
}

size_t db::wh_item::GetCount()
{
	return GetList()->n_members;
}

bool db::wh_item::Delete(ref<CWarehouseItem> const& wh_item)
{
	auto main_member = wh_item->GetIndexMember(WarehouseItemIndexMemberEnum::Main);
	if (main_member.is_nil())
	{
		return false;
	}

	auto list = GetList();
	modify(list)->remove(main_member);

	db::global_index::RemoveObject(wh_item);
	return true;
}
