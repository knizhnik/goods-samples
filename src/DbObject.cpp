
#include "stdafx.h"
#include "DbObject.h"

REGISTER_ABSTRACT_EX(CDbObject, object, pessimistic_scheme, class_descriptor::cls_hierarchy_super_root);

CDbObject::CDbObject(class_descriptor& desc)
	: object(desc)
{
}

field_descriptor& CDbObject::describe_components()
{
	return FIELD(m_CustomFields);
}
