#pragma once

class CTimeBtree;
class CWarehouseItem;

class CWarehouseItemList : public set_owner
{
public:
	METACLASS_DECLARATIONS(CWarehouseItemList, set_owner);

	static ref<CWarehouseItemList> create(ref<object> const& owner)
	{
		return NEW CWarehouseItemList(self_class, owner);
	}

	virtual void insert(ref<set_member> mbr) override;
	virtual void remove(ref<set_member> mbr) override;

protected:
	CWarehouseItemList(class_descriptor& desc, ref<object> const& owner);

private:
	void InsertInIndexes(ref<CWarehouseItem> const& wh_item);
	void RemoveFromIndexes(ref<CWarehouseItem> const& wh_item);

private:
	ref<CTimeBtree>	m_IndexByDate;
};

