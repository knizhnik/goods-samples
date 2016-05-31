
#include "stdafx.h"
#include "DbObject.h"

REGISTER_ABSTRACT_EX(CDbObject, object, pessimistic_scheme, CLASS_HIERARCHY_SUPER_ROOT);

CDbObject::CDbObject(class_descriptor& desc)
	: object(desc)
{
}

field_descriptor& CDbObject::describe_components()
{
	return FIELD(m_CustomFields);
}
