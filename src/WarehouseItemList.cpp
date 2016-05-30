#include "stdafx.h"
#include "IndexHelper.h"
#include "WarehouseReceipt.h"
#include "WarehouseItem.h"
#include "WarehouseItemList.h"


REGISTER(CWarehouseItemList, set_owner, pessimistic_scheme);

CWarehouseItemList::CWarehouseItemList(class_descriptor& desc, ref<object> const& owner)
	: set_owner(desc, owner)
	, m_IndexByDate(CTimeBtree::create(this))
{
}

field_descriptor& CWarehouseItemList::describe_components()
{
	return FIELD(m_IndexByDate);
}


/*virtual override*/
void CWarehouseItemList::insert(ref<set_member> mbr)
{
	if (mbr.is_nil())
	{
		return;
	}
	set_owner::insert(mbr);

	const ref<CWarehouseItem> wh_item = mbr->obj;
	InsertInIndexes(wh_item);
}

/*virtual override*/
void CWarehouseItemList::remove(ref<set_member> mbr)
{
	if (mbr.is_nil())
	{
		return;
	}
	set_owner::remove(mbr);

	const ref<CWarehouseItem> wh_item = mbr->obj;
	RemoveFromIndexes(wh_item);
}

void CWarehouseItemList::InsertInIndexes(ref<CWarehouseItem> const& wh_item)
{	
	const auto warehose_receipt = wh_item->GetWarehouseReceipt();
	const auto date = warehose_receipt->GetDate();

	auto date_member = Index::InsertInIndex(m_IndexByDate, wh_item, date);
	
	modify(wh_item)->SetIndexMember(WarehouseItemIndexMemberEnum::IndexByDate, date_member);
}

void CWarehouseItemList::RemoveFromIndexes(ref<CWarehouseItem> const& wh_item)
{
	const auto warehose_receipt = wh_item->GetWarehouseReceipt();
	const auto date = warehose_receipt->GetDate();

	Index::RemoveFromIndex(m_IndexByDate, date);

	modify(wh_item)->SetIndexMember(WarehouseItemIndexMemberEnum::IndexByDate, nullptr);
}
