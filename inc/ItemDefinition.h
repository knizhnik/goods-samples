
#pragma once

class CDimensions;
class CPackage;

class CItemDefinition : public object
{
public:
	METACLASS_DECLARATIONS(CItemDefinition, object);

	static ref<CItemDefinition> create()
	{
		return NEW CItemDefinition(self_class);
	}

	std::wstring GetCode() const
	{
		return m_Code.getSafeWideChar();
	}

	std::wstring GetDescription() const
	{
		return m_Description.getSafeWideChar();
	}

	ref<CPackage> GetPackage() const
	{
		return m_Package;
	}

	ref<CDimensions> GetDimensions() const
	{
		return m_Dimensions;
	}

	double GetWeight() const
	{
		return m_Weight;
	}

	void SetCode(const wchar_t* code) 
	{
		m_Code = code;
	}

	void SetDescription(const wchar_t* description) 
	{
		m_Description = description;
	}

	void SetPackage(ref<CPackage> const& package) 
	{
		m_Package = package;
	}

	void SetDimensions(ref<CDimensions> const& dimensions) 
	{
		m_Dimensions = dimensions;
	}

	void SetWeight(double weight) 
	{
		m_Weight = weight;
	}

protected:
	CItemDefinition(class_descriptor& desc);

private:
	wstring_t			m_Code;
	wstring_t			m_Description;
	ref<CPackage>		m_Package;
	ref<CDimensions>	m_Dimensions;
	double				m_Weight;
};
