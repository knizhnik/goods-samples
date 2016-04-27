#include "stdafx.h"
#include "WarehouseReceipt.h"

REGISTER(CWarehouseReceipt, object, pessimistic_scheme);

CWarehouseReceipt::CWarehouseReceipt(class_descriptor& desc)
	: object(desc)
	, m_Items(set_owner::create(this))
{
}

field_descriptor& CWarehouseReceipt::describe_components()
{
	return 
		FIELD(m_Number),
		FIELD(m_Date),
		FIELD(m_Items),
		FIELD(m_ShipperName),
		FIELD(m_ConsigneeName);
}