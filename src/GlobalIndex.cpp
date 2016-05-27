
#include "stdafx.h"
#include "DbObject.h"
#include "GlobalIndex.h"

REGISTER(CGlobalIndex, object, pessimistic_scheme);

CGlobalIndex::CGlobalIndex(class_descriptor& desc)
	: object(desc)
	, m_HashTable(hash_table::create(100))
{
}

field_descriptor& CGlobalIndex::describe_components()
{
	return
		FIELD(m_HashTable);
}

void CGlobalIndex::InsertObject(ref<CDbObject> const& db_object)
{
	const std::string hash_code = db_object->GetHashCode();
	modify(m_HashTable)->put(hash_code.c_str(), db_object);
}

void CGlobalIndex::RemoveObject(ref<CDbObject> const& db_object)
{
	const std::string hash_code = db_object->GetHashCode();
	modify(m_HashTable)->del(hash_code.c_str(), db_object);
}
