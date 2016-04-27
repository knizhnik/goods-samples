#include "stdafx.h"
#include "IndexHelper.h"
#include "WarehouseItem.h"
#include "WarehouseItemList.h"


REGISTER(CWarehouseItemList, set_owner, pessimistic_scheme);

CWarehouseItemList::CWarehouseItemList(class_descriptor& desc, ref<object> const& owner)
	: set_owner(desc, owner)
{
}

field_descriptor& CWarehouseItemList::describe_components()
{
	return NO_FIELDS;
}
