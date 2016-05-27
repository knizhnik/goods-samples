
#include "stdafx.h"
#include "GlobalIndex.h"
#include "GlobalIndexController.h"
#include "RootDataLayer.h"


void db::global_index::InsertObject(ref<CDbObject> const& db_object)
{
	auto global_index = db::GetDatabaseRoot()->GetGlobalIndex();
	modify(global_index)->InsertObject(db_object);
}

void db::global_index::RemoveObject(ref<CDbObject> const& db_object)
{
	auto global_index = db::GetDatabaseRoot()->GetGlobalIndex();
	modify(global_index)->RemoveObject(db_object);
}
