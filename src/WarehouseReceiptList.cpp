
#include "stdafx.h"

#include "goodsex.h"
#include "IndexHelper.h"
#include "WarehouseReceipt.h"
#include "WarehouseReceiptList.h"

REGISTER(CWarehouseReceiptList, set_owner, pessimistic_scheme);

CWarehouseReceiptList::CWarehouseReceiptList(class_descriptor& desc, ref<object> const& obj)
	: set_owner(desc, obj)
	, m_IndexByNumber(DbIndex16::create(this))
	, m_IndexByDate(DbIndex::create(this))
{
}

field_descriptor& CWarehouseReceiptList::describe_components()
{
	return
		FIELD(m_IndexByNumber),
		FIELD(m_IndexByDate);
}


/*virtual override*/
void CWarehouseReceiptList::insert(ref<set_member> mbr)
{
	if (mbr.is_nil())
	{
		return;
	}
	set_owner::insert(mbr);

	const ref<CWarehouseReceipt> warehouse_receipt = mbr->obj;
	InsertInIndexes(warehouse_receipt);
}

/*virtual override*/
void CWarehouseReceiptList::remove(ref<set_member> mbr)
{
	if (mbr.is_nil())
	{
		return;
	}
	set_owner::remove(mbr);

	const ref<CWarehouseReceipt> warehouse_receipt = mbr->obj;
	RemoveFromIndexes(warehouse_receipt);
}

void CWarehouseReceiptList::InsertInIndexes(ref<CWarehouseReceipt> const& warehouse_receipt)
{
	const auto number = warehouse_receipt->GetNumber();
	const auto date = warehouse_receipt->GetDate();

	auto number_index = Index::InsertInIndex(m_IndexByNumber, warehouse_receipt, number);
	modify(warehouse_receipt)->SetIndexMember(WarehouseReceiptIndexMemberEnum::IndexByNumber, number_index);

	auto date_index = Index::InsertInIndex(m_IndexByDate, warehouse_receipt, date);
	modify(warehouse_receipt)->SetIndexMember(WarehouseReceiptIndexMemberEnum::IndexByDate, date_index);
}

void CWarehouseReceiptList::RemoveFromIndexes(ref<CWarehouseReceipt> const& warehouse_receipt)
{
	const auto number = warehouse_receipt->GetNumber();
	const auto date = warehouse_receipt->GetDate();

	auto number_index = warehouse_receipt->GetIndexMember(WarehouseReceiptIndexMemberEnum::IndexByNumber);
	remove_from_set_owner(m_IndexByNumber, number_index);

	auto date_index = warehouse_receipt->GetIndexMember(WarehouseReceiptIndexMemberEnum::IndexByNumber);
	remove_from_set_owner(m_IndexByDate, date_index);
}

ref<set_member> CWarehouseReceiptList::FindByNumber(std::wstring const& number) const
{
	return Index::FindMember(m_IndexByNumber, number);
}

