#pragma once

class CDatabaseRoot : public object
{
public:
	METACLASS_DECLARATIONS(CDatabaseRoot, object);

	CDatabaseRoot();

	ref<CDatabaseRoot> create() const;

	bool IsNewDatabase() const
	{
		return is_abstract_root();
	}

	ref<set_owner> GetPackageList() const
	{
		return m_Packages;
	}

	ref<set_owner> GetItemDefinitionList() const
	{
		return m_ItemDefinitions;
	}

	ref<set_owner> GetWarehouseItemList() const
	{
		return m_WarehouseItems;
	}

	ref<set_owner> GetWarehouseReceiptList() const
	{
		return m_WarehouseReceipts;
	}

private:
	void SetupNewDatabase();

private:
	ref<set_owner>	m_Packages;
	ref<set_owner>	m_ItemDefinitions;
	ref<set_owner>	m_WarehouseItems;
	ref<set_owner>	m_WarehouseReceipts;
};

