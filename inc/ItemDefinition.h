
#pragma once

#include "DbObject.h"

class CDimensions;
class CPackage;

enum class CItemDefinitionFlags
{
	None = 0,
	HazMat = 1
};

class CItemDefinition : public CDbObject
{
public:
	METACLASS_DECLARATIONS(CItemDefinition, CDbObject);

	static ref<CItemDefinition> create()
	{
		return NEW CItemDefinition(self_class);
	}

	virtual std::string GetHashCode() const override;

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

	bool IsMarkedAs(CItemDefinitionFlags flag) const
	{
		return (m_Flags & static_cast<nat8>(flag)) != 0;
	}

	ref<ExternalBlob> GetImage() const
	{
		return m_Image;
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

	void SetFlags(CItemDefinitionFlags flag, bool value)
	{
		m_Flags = (value) ? (m_Flags | static_cast<nat8>(flag)) : (m_Flags & ~static_cast<nat8>(flag));
	}

	void SetImage(const void* body, size_t size) 
	{
		m_Image = ExternalBlob::create(body, size);
	}

protected:
	CItemDefinition(class_descriptor& desc);

private:
	wstring_t			m_Code;
	wstring_t			m_Description;
	ref<CPackage>		m_Package;
	ref<CDimensions>	m_Dimensions;
	double				m_Weight;
	nat8				m_Flags;
	ref<ExternalBlob>	m_Image;
};
