
#include "stdafx.h"
#include "DimensionDataLayer.h"
#include "ItemDefinitionDataLayer.h"
#include "ItemDefinitionList.h"
#include "IndexHelper.h"
#include "Package.h"
#include "RootDataLayer.h"


ref<CItemDefinition> db::item_def::Create(const wchar_t* code, const wchar_t* description)
{
	w_ref<CItemDefinition> w_item_def = CItemDefinition::create();

	w_item_def->SetCode(code);
	w_item_def->SetDescription(description);

	return w_item_def;
}

ref<CItemDefinitionList> db::item_def::GetList()
{
	return db::GetDatabaseRoot()->GetItemDefinitionList();
}

void db::item_def::SetPackage(w_ref<CItemDefinition> w_item_definition, ref<CPackage> const& package)
{
	w_item_definition->SetPackage(package);
	auto package_dimensions = package->GetDimensions();
	if (!package_dimensions.is_nil())
	{
		auto item_def_dimensions = db::dimensions::Clone(package_dimensions);
		w_item_definition->SetDimensions(item_def_dimensions);
	}
}

void db::item_def::Save(ref<CItemDefinition> const& item_def)
{
	w_ref<CItemDefinitionList> list = GetList();
	
	auto insert_member = Index::CreateMember(item_def);
	list->insert(insert_member);
}

size_t db::item_def::GetCount()
{
	return GetList()->n_members;
}
