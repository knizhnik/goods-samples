
#pragma once

class CDbObject;

namespace db
{
	namespace global_index
	{
		void InsertObject(ref<CDbObject> const& db_object);
		void RemoveObject(ref<CDbObject> const& db_object);

	}	// namespace global_index

}	// namespace db

