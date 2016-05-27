#include "stdafx.h"
#include "WarehouseReceipt.h"

REGISTER(CWarehouseReceipt, CDbObject, pessimistic_scheme);

CWarehouseReceipt::CWarehouseReceipt(class_descriptor& desc)
	: CDbObject(desc)
	, m_Items(set_owner::create(this))
	, m_IndexMembers(ArrayOfObject::create(3, 3))
{
}

field_descriptor& CWarehouseReceipt::describe_components()
{
	return 
		FIELD(m_Number),
		FIELD(m_Date),
		FIELD(m_Items),
		FIELD(m_ShipperName),
		FIELD(m_ConsigneeName),
		FIELD(m_IndexMembers),
		FIELD(m_CreationStamp),
		FIELD(m_ShipperEmail);
}

/*virtual override*/
std::string CWarehouseReceipt::GetHashCode() const
{
	return m_Number.getChars();
}


void CWarehouseReceipt::SetCreationStamp(const wchar_t* name, time_t date)
{
	m_CreationStamp.SetActionBy(name);
	m_CreationStamp.SetActionOn(date);
}