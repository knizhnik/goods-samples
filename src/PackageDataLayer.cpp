
#include "stdafx.h"
#include "GlobalIndexController.h"
#include "IndexHelper.h"
#include "PackageDataLayer.h"
#include "PackageList.h"
#include "RootDataLayer.h"

ref<CPackage> db::package::Create(const wchar_t* code, const wchar_t* description)
{
	w_ref<CPackage> package = CPackage::create();
	package->SetCode(code);
	package->SetDescription(description);

	return package;
}

ref<CPackageList> db::package::GetList()
{
	return db::GetDatabaseRoot()->GetPackageList();
}

ref<CPackage> db::package::FindByCode(const wchar_t* code)
{
	return GetList()->FindByCode(code);
}

void db::package::Save(ref<CPackage> const& package)
{
	w_ref<CPackageList> package_list = GetList();

	auto insert_member = Index::CreateMember(package);
	package_list->insert(insert_member);

	db::global_index::InsertObject(package);
}

size_t db::package::GetCount()
{
	return GetList()->n_members;
}
