
#include "stdafx.h"
#include "ItemDefinition.h"

REGISTER(CItemDefinition, object, pessimistic_scheme);

CItemDefinition::CItemDefinition(class_descriptor& desc)
	: object(desc)
{
}

field_descriptor& CItemDefinition::describe_components()
{
	return 
		FIELD(m_Code),
		FIELD(m_Description),
		FIELD(m_Package),
		FIELD(m_Dimensions),
		FIELD(m_Weight);	
}