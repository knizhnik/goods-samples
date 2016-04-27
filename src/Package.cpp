
#include "stdafx.h"
#include "Package.h"



REGISTER(CPackage, object, pessimistic_scheme);

CPackage::CPackage(class_descriptor& desc)
	: object(desc)
{
}

field_descriptor& CPackage::describe_components()
{
	return
		FIELD(m_Code),
		FIELD(m_Description),
		FIELD(m_Dimensions);
}
