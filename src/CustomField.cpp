#include "stdafx.h"
#include "CustomField.h"

REGISTER(CCustomField, object, pessimistic_scheme);

CCustomField::CCustomField(class_descriptor& desc, const wchar_t* name, const wchar_t* value)
	: object(desc)
	, m_Name(name)
	, m_Value(value)
{
}

field_descriptor& CCustomField::describe_components()
{
	return 
		FIELD(m_Name),
		FIELD(m_Value);
}
