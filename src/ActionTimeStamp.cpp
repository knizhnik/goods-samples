
#include "StdAfx.h"
#include "ActionTimeStamp.h"

field_descriptor& CActionTimeStamp::describe_components()
{
	return 
		FIELD(m_ActionBy),
		FIELD(m_ActionOn);
}
