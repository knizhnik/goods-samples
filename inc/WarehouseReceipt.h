#pragma once

class CWarehouseReceipt : public object
{
public:
	METACLASS_DECLARATIONS(CWarehouseReceipt, object);

	static ref<CWarehouseReceipt> create()
	{
		return NEW CWarehouseReceipt(self_class);
	}

	std::wstring GetNumber() const
	{
		return m_Number.getSafeWideChar();
	}

	time_t GetDate() const
	{
		return m_Date;
	}

	ref<set_owner> GetItems() const
	{
		return m_Items;
	}

	std::wstring GetShipperName() const
	{
		return m_ShipperName.getSafeWideChar();
	}

	std::wstring GetConsigneeName() const
	{
		return m_ConsigneeName.getSafeWideChar();
	}

	void SetNumber(const wchar_t* number)
	{
		m_Number = number;
	}

	void SetDate(time_t date)
	{
		m_Date = date;
	}

	void SetShipperName(const wchar_t* shipper_name)
	{
		m_ShipperName = shipper_name;
	}

	void SetConsigneeName(const wchar_t* consignee_name)
	{
		m_ConsigneeName = consignee_name;
	}

protected:
	CWarehouseReceipt(class_descriptor& desc);

private:
	wstring_t		m_Number;
	nat8			m_Date;
	ref<set_owner>	m_Items;
	wstring_t		m_ShipperName;
	wstring_t		m_ConsigneeName;
};