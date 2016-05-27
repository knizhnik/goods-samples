
#include "stdafx.h"
#include "GlobalIndexController.h"
#include "goods_container.h"
#include "goodsex.h"
#include "IndexHelper.h"
#include "RootDataLayer.h"
#include "WarehouseItemDataLayer.h"
#include "WarehouseReceiptDataLayer.h"
#include "WarehouseReceiptList.h"


ref<CWarehouseReceiptList> db::warehouse_receipt::GetList()
{
	return db::GetDatabaseRoot()->GetWarehouseReceiptList();
}

ref<CWarehouseReceipt> db::warehouse_receipt::Create(const wchar_t* number)
{
	w_ref<CWarehouseReceipt> w_warehouse_receipt = CWarehouseReceipt::create();

	w_warehouse_receipt->SetNumber(number);
	w_warehouse_receipt->SetDate(time(nullptr));

	return w_warehouse_receipt;
}

void db::warehouse_receipt::AddItem(w_ref<CWarehouseReceipt> w_warehouse_receipt, ref<CWarehouseItem> const& item)
{
	w_ref<set_owner> w_items =  w_warehouse_receipt->GetItems();

	auto whr_insert_member = Index::CreateMember(item);
	w_items->insert(whr_insert_member);
}

void db::warehouse_receipt::Save(ref<CWarehouseReceipt> const& warehouse_receipt)
{
	w_ref<CWarehouseReceiptList> w_list = GetList();

	if (is_new_object(warehouse_receipt))
	{
		modify(warehouse_receipt)->SetCreationStamp(L"The Creator", time(nullptr));

		auto insert_member = Index::CreateMember(warehouse_receipt);
		w_list->insert(insert_member);
		modify(warehouse_receipt)->SetIndexMember(WarehouseReceiptIndexMemberEnum::Main, insert_member);

		auto items = warehouse_receipt->GetItems();
		for (auto mbr = items->first; mbr != nullptr; mbr = mbr->next)
		{
			ref<CWarehouseItem> wh_item = mbr->obj;

			modify(wh_item)->SetIndexMember(WarehouseItemIndexMemberEnum::WarehouseReceiptList, mbr);
			modify(wh_item)->SetWarehouseReceipt(warehouse_receipt);

			db::wh_item::Save(wh_item);
		}

		db::global_index::InsertObject(warehouse_receipt);
	}
	else
	{
		auto main_member = warehouse_receipt->GetIndexMember(WarehouseReceiptIndexMemberEnum::Main);
		w_list->remove(main_member);
		w_list->insert(main_member);
	}
}

size_t db::warehouse_receipt::GetCount()
{
	return GetList()->n_members;
}

static bool CanDelete(ref<CWarehouseReceipt> const& warehouse_receipt)
{
	return true;
}

static void DeleteInnerItems(ref<CWarehouseReceipt> const& warehouse_receipt)
{
	w_ref<set_owner> w_item_list = warehouse_receipt->GetItems();
	for (ref<CWarehouseItem> wh_item : w_item_list)
	{
		auto receipt_member = wh_item->GetIndexMember(WarehouseItemIndexMemberEnum::WarehouseReceiptList);
		w_item_list->remove(receipt_member);

		db::wh_item::Delete(wh_item);
	}

	db::global_index::RemoveObject(warehouse_receipt);
}

bool db::warehouse_receipt::Delete(ref<CWarehouseReceipt> const& warehouse_receipt)
{
	if (!CanDelete(warehouse_receipt))
	{
		return false;
	}

	auto main_member = warehouse_receipt->GetIndexMember(WarehouseReceiptIndexMemberEnum::Main);
	if (main_member.is_nil())
	{
		return false;
	}

	w_ref<CWarehouseReceiptList> w_list = GetList();

	DeleteInnerItems(warehouse_receipt);

	w_list->remove(main_member);
	return true;
}
