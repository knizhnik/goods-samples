
#include "stdafx.h"
#include "Package.h"



REGISTER(CPackage, CDbObject, pessimistic_scheme);

CPackage::CPackage(class_descriptor& desc)
	: CDbObject(desc)
{
}

field_descriptor& CPackage::describe_components()
{
	return
		FIELD(m_Code),
		FIELD(m_Description),
		FIELD(m_Dimensions);
}


/*virtual override*/
std::string CPackage::GetHashCode() const
{
	return m_Code.getChars();
}
