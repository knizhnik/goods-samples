
#pragma once

class CDimensions;

class CPackage : public object
{
public:
	METACLASS_DECLARATIONS(CPackage, object);

	static ref<CPackage> create()
	{
		return NEW CPackage(self_class);
	}

	std::wstring GetCode() const
	{
		return m_Code.getSafeWideChar();
	}

	std::wstring GetDescription() const
	{
		return m_Description.getSafeWideChar();
	}

	ref<CDimensions> GetDimensions() const
	{
		return m_Dimensions;
	}

	void SetCode(const wchar_t* code)
	{
		m_Code = code;
	}

	void SetDescription(const wchar_t* description)
	{
		m_Description = description;
	}

	void SetDimensions(ref<CDimensions> const& dimensions) 
	{
		m_Dimensions = dimensions;
	}

protected:
	CPackage(class_descriptor& desc);

private:
	wstring_t			m_Code;
	wstring_t			m_Description;	
	ref<CDimensions>	m_Dimensions;
};
