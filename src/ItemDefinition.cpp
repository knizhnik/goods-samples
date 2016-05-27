
#include "stdafx.h"
#include "ItemDefinition.h"

REGISTER(CItemDefinition, CDbObject, pessimistic_scheme);

CItemDefinition::CItemDefinition(class_descriptor& desc)
	: CDbObject(desc)
{
}

field_descriptor& CItemDefinition::describe_components()
{
	return 
		FIELD(m_Code),
		FIELD(m_Description),
		FIELD(m_Package),
		FIELD(m_Dimensions),
		FIELD(m_Weight),
		FIELD(m_Flags),
		FIELD(m_Image);
}

/*virtual override*/
std::string CItemDefinition::GetHashCode() const
{
	return m_Code.getChars();
}
