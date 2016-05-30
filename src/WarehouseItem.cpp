#include "stdafx.h"
#include "HazardousItem.h"
#include "ItemDefinition.h"
#include "WarehouseItem.h"



REGISTER(CWarehouseItem, CDbObject, pessimistic_scheme);
REGISTER(CHazardousItem, CWarehouseItem, pessimistic_scheme);

CWarehouseItem::CWarehouseItem(class_descriptor& desc)
	: CDbObject(desc)
	, m_IndexMembers(ArrayOfObject::create(2, 2))
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
		FIELD(m_IndexMembers),
		FIELD(m_ExtraData);
}

/*virtual override*/
std::string CWarehouseItem::GetHashCode() const
{
	return m_ItemDefinition->GetHashCode();
}

void CWarehouseItem::SetExtraData(std::vector<char> const& extra_data)
{	
	const auto size = extra_data.size();
	m_ExtraData = new (Blob::self_class, size) Blob((void*)(extra_data.data()), size);
}