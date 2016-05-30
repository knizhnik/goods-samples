
#pragma once


// -- base abstract class 
class CDbObject : public object
{
public:
	METACLASS_DECLARATIONS(CDbObject, object);

	ref<set_owner> GetCustomFields() const
	{
		return m_CustomFields;
	}

	void SetCustomFields(ref<set_owner> const& custom_fields)
	{ 
		m_CustomFields = custom_fields;
	}

	virtual std::string GetHashCode() const = 0;

protected:
	CDbObject(class_descriptor& desc);

private:
	ref<set_owner>	m_CustomFields;
};


