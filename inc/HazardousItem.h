
#pragma once

#include "WarehouseItem.h"

class CHazardousItem : public CWarehouseItem
{
public:
	METACLASS_DECLARATIONS(CHazardousItem, CWarehouseItem);

	static ref<CHazardousItem> create()
	{
		return NEW CHazardousItem(self_class);
	}

	std::wstring GetHazMatCode() const
	{
		return m_HazMatCode.getSafeWideChar();
	}

	void SetHazMatCode(const wchar_t* hazmat_code)
	{
		m_HazMatCode = hazmat_code;
	}

protected:
	CHazardousItem(class_descriptor& desc);

private:
	wstring_t	m_HazMatCode;
};
