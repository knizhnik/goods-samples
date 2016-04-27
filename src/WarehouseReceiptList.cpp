#include "stdafx.h"
#include "IndexHelper.h"
#include "WarehouseReceipt.h"
#include "WarehouseReceiptList.h"

REGISTER(CWarehouseReceiptList, set_owner, pessimistic_scheme);

CWarehouseReceiptList::CWarehouseReceiptList(class_descriptor& desc, ref<object> const& obj)
	: set_owner(desc, obj)
	, m_IndexByNumber(B_tree::create(this))
	, m_IndexByDate(B_tree::create(this))
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

	index::InsertInIndex(m_IndexByNumber, warehouse_receipt, number);
	index::InsertInIndex(m_IndexByDate, warehouse_receipt, date);
}

void CWarehouseReceiptList::RemoveFromIndexes(ref<CWarehouseReceipt> const& warehouse_receipt)
{
	const auto number = warehouse_receipt->GetNumber();
	const auto date = warehouse_receipt->GetDate();

	index::RemoveFromIndex(m_IndexByNumber, number);
	index::RemoveFromIndex(m_IndexByDate, date);
}

ref<set_member> CWarehouseReceiptList::FindByNumber(std::wstring const& number) const
{
	return index::FindMember(m_IndexByNumber, number);
}