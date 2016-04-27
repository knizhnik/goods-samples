
#include "stdafx.h"
#include "goods_container.h"
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

	auto insert_member = Index::CreateMember(warehouse_receipt);
	w_list->insert(insert_member);

	auto items = warehouse_receipt->GetItems();
	for (auto mbr = items->first; mbr != nullptr; mbr = mbr->next)
	{
		ref<CWarehouseItem> wh_item = mbr->obj;

		modify(wh_item)->SetWarehouseReceiptMember(mbr);
		modify(wh_item)->SetWarehouseReceipt(warehouse_receipt);

		db::wh_item::Save(wh_item);
	}
}

size_t db::warehouse_receipt::GetCount()
{
	return GetList()->n_members;
}
