#include "stdafx.h"
#include "WarehouseItem.h"

REGISTER(CWarehouseItem, object, pessimistic_scheme);

CWarehouseItem::CWarehouseItem(class_descriptor& desc)
	: object(desc)
{
}

field_descriptor& CWarehouseItem::describe_components()
{
	return
		FIELD(m_Description),
		FIELD(m_Package),
		FIELD(m_ItemDefinition),
		FIELD(m_Dimensions),
		FIELD(m_Weight),
		FIELD(m_LocationCode),
		FIELD(m_WarehouseReceipt),
		FIELD(m_WarehouseReceiptItemsMember);
}