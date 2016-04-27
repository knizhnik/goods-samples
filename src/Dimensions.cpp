
#include "stdafx.h"
#include "Dimensions.h"


REGISTER(CDimensions, object, pessimistic_scheme);

CDimensions::CDimensions(class_descriptor& desc, double length, double width, double height)
	: object(desc)
	, m_Length(length)
	, m_Width(width)
	, m_Height(height)
{
}

field_descriptor& CDimensions::describe_components()
{
	return 
		FIELD(m_Length),
		FIELD(m_Width),
		FIELD(m_Height);
}