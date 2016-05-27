#pragma once

class CCustomField : public object
{
public:
	METACLASS_DECLARATIONS(CCustomField, object); 

	static ref<CCustomField> create(const wchar_t* name, const wchar_t* value)
	{
		return NEW CCustomField(self_class, name, value);
	}

	std::wstring GetName() const
	{
		return m_Name.getSafeWideChar();
	}

	std::wstring GetValue() const
	{
		return m_Value.getSafeWideChar();
	}

	void SetValue(const wchar_t* value)
	{
		m_Value = value;
	}

protected:
	CCustomField(class_descriptor& desc, const wchar_t* name, const wchar_t* value);

private:
	wstring_t	m_Name;
	wstring_t	m_Value;
};



