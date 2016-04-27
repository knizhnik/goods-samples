#pragma once

class CWarehouseItemList : public set_owner
{
public:
	METACLASS_DECLARATIONS(CWarehouseItemList, set_owner);

	static ref<CWarehouseItemList> create(ref<object> const& owner)
	{
		return NEW CWarehouseItemList(self_class, owner);
	}

protected:
	CWarehouseItemList(class_descriptor& desc, ref<object> const& owner);
};
