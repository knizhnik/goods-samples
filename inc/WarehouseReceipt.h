#pragma once

#include "DbObject.h"
#include "ActionTimeStamp.h"

enum class WarehouseReceiptIndexMemberEnum
{
	Main,
	IndexByNumber,
	IndexByDate
};

class CWarehouseReceipt : public CDbObject
{
public:
	METACLASS_DECLARATIONS(CWarehouseReceipt, CDbObject);

	static ref<CWarehouseReceipt> create()
	{
		return NEW CWarehouseReceipt(self_class);
	}

	virtual std::string GetHashCode() const override;

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

	ref<set_member> GetIndexMember(WarehouseReceiptIndexMemberEnum member_on) const
	{
		return m_IndexMembers->getat(static_cast<nat4>(member_on));
	}

	std::string GetShipperEmail() const
	{
		return m_ShipperEmail.get_data();
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

	void SetIndexMember(WarehouseReceiptIndexMemberEnum member_on, ref<set_member> const& member)
	{
		modify(m_IndexMembers)->putat(static_cast<nat4>(member_on), member);
	}

	void SetCreationStamp(const wchar_t* name, time_t date);

	void SetShipperEmail(const char* email)
	{
		m_ShipperEmail = raw_binary_t(email, strlen(email) + 1);
	}

protected:
	CWarehouseReceipt(class_descriptor& desc);

private:
	wstring_t			m_Number;
	nat8				m_Date;
	ref<set_owner>		m_Items;
	wstring_t			m_ShipperName;
	wstring_t			m_ConsigneeName;
	ref<ArrayOfObject>	m_IndexMembers;
	CActionTimeStamp	m_CreationStamp;
	raw_binary_t		m_ShipperEmail;
};
