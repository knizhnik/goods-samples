
#include "stdafx.h"
#include "HazardousItem.h"


//REGISTER(CHazardousItem, CWarehouseItem, pessimistic_scheme);

CHazardousItem::CHazardousItem(class_descriptor& desc)
	: CWarehouseItem(desc)
{
}

field_descriptor& CHazardousItem::describe_components()
{
	return
		FIELD(m_HazMatCode);
}
