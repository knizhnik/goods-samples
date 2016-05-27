
#include "stdafx.h"
#include "DbObject.h"

REGISTER_ABSTRACT(CDbObject, object, pessimistic_scheme);

CDbObject::CDbObject(class_descriptor& desc)
	: object(desc)
{
}

field_descriptor& CDbObject::describe_components()
{
	return FIELD(m_CustomFields);
}
