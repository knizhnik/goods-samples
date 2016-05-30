
#pragma once

class CDbObject;

class CGlobalIndex : public object
{
public:
	METACLASS_DECLARATIONS(CGlobalIndex, object);

	static ref<CGlobalIndex> create()
	{
		return NEW CGlobalIndex(self_class);
	}

	void InsertObject(ref<CDbObject> const& db_object);
	void RemoveObject(ref<CDbObject> const& db_object);

private:
	CGlobalIndex(class_descriptor& desc);

private:
	ref<DbHash>	m_HashTable;
};
