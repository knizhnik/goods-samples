
#pragma once

class CDbObject;

class CGlobalIndex : public object
{
public:
	METACLASS_DECLARATIONS(CGlobalIndex, object);

	static ref<CGlobalIndex> create(ref<object> root)
	{
		return NEW CGlobalIndex(root->get_handle()->storage);
	}

	void InsertObject(ref<CDbObject> const& db_object);
	void RemoveObject(ref<CDbObject> const& db_object);

private:
	CGlobalIndex(obj_storage* storage);

private:
	ref<DbHash>	m_HashTable;
};
