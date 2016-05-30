#pragma once

class CTimeBtree;
class CWarehouseReceipt;

class CWarehouseReceiptList : public set_owner
{
public:
	METACLASS_DECLARATIONS(CWarehouseReceiptList, set_owner);

	static ref<CWarehouseReceiptList> create(ref<object> const& obj)
	{
		return NEW CWarehouseReceiptList(self_class, obj);
	}

	virtual void insert(ref<set_member> mbr) override;
	virtual void remove(ref<set_member> mbr) override;

	ref<set_member> FindByNumber(std::wstring const& number) const;
	
	ref<B_tree> GetByDateList() const
	{
		return m_IndexByDate;
	}

protected:
	CWarehouseReceiptList(class_descriptor& desc, ref<object> const& obj);

private:
	void InsertInIndexes(ref<CWarehouseReceipt> const& warehouse_receipt);
	void RemoveFromIndexes(ref<CWarehouseReceipt> const& warehouse_receipt);

private:
	ref<DbIndex16>	m_IndexByNumber;
	ref<DbIndex>	m_IndexByDate;
};
