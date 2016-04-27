#pragma once

class CDimensions;
class CItemDefinition;
class CPackage;
class CWarehouseReceipt;

class CWarehouseItem : public object
{
public:
	METACLASS_DECLARATIONS(CWarehouseItem, object);

	static ref<CWarehouseItem> create()
	{
		return NEW CWarehouseItem(self_class);
	}

	std::wstring GetDescription() const
	{
		return m_Description.getSafeWideChar();
	}

	ref<CPackage> GetPackage() const
	{
		return m_Package;
	}

	ref<CItemDefinition> GetItemDefinition() const
	{
		return m_ItemDefinition;
	}

	ref<CDimensions> GetDimensions() const
	{
		return m_Dimensions;
	}

	double GetWeight() const
	{
		return m_Weight;
	}

	std::wstring GetLocationCode() const
	{
		return m_LocationCode.getSafeWideChar();
	}

	ref<CWarehouseReceipt> GetWarehouseReceipt() const
	{
		return m_WarehouseReceipt;
	}

	ref<set_member> GetWarehouseReceiptMember() const
	{
		return m_WarehouseReceiptItemsMember;
	}


	void SetDescription(const wchar_t* description) 
	{
		m_Description = description;
	}

	void SetPackage(ref<CPackage> const& package)
	{
		m_Package = package;
	}

	void SetItemDefinition(ref<CItemDefinition> const& item_definition)
	{
		m_ItemDefinition = item_definition;
	}

	void SetDimensions(ref<CDimensions> const& dimensions)
	{
		m_Dimensions = dimensions;
	}

	void SetWeight(double weight)
	{
		m_Weight = weight;
	}

	void SetLocationCode(const wchar_t* location_code)
	{
		m_LocationCode = location_code;
	}

	void SetWarehouseReceipt(ref<CWarehouseReceipt> const& warehouse_receipt)
	{
		m_WarehouseReceipt = warehouse_receipt;
	}

	void SetWarehouseReceiptMember(ref<set_member> const& member)
	{
		m_WarehouseReceiptItemsMember = member;
	}

protected:
	CWarehouseItem(class_descriptor& desc);

private:
	wstring_t				m_Description;
	ref<CPackage>			m_Package;
	ref<CItemDefinition>	m_ItemDefinition;
	ref<CDimensions>		m_Dimensions;
	double					m_Weight;
	wstring_t				m_LocationCode;
	ref<CWarehouseReceipt>	m_WarehouseReceipt;
	ref<set_member>			m_WarehouseReceiptItemsMember;
};