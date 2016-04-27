#include "stdafx.h"
#include "IndexHelper.h"
#include "PackageList.h"
#include "Package.h"

REGISTER(CPackageList, set_owner, pessimistic_scheme);

CPackageList::CPackageList(class_descriptor& desc, ref<object> const& owner)
	: set_owner(desc, owner)
	, m_IndexByCode(B_tree::create(this))
{
}

field_descriptor& CPackageList::describe_components()
{
	return FIELD(m_IndexByCode);
}

/*virtual override*/
void CPackageList::insert(ref<set_member> mbr)
{
	if (mbr.is_nil())
	{
		return;
	}	
	set_owner::insert(mbr);
	
	const ref<CPackage> package = mbr->obj;
	InsertInIndexes(package);
}

/*virtual override*/
void CPackageList::remove(ref<set_member> mbr)
{
	if (mbr.is_nil())
	{
		return;
	}	
	set_owner::remove(mbr);

	const ref<CPackage> package = mbr->obj;
	RemoveFromIndexes(package);
}

void CPackageList::InsertInIndexes(ref<CPackage> const& package)
{
	const auto code = package->GetCode();
	Index::InsertInIndex(m_IndexByCode, package, code);
}

void CPackageList::RemoveFromIndexes(ref<CPackage> const& package)
{
	const auto code = package->GetCode();
	Index::RemoveFromIndex(m_IndexByCode, code);
}

ref<CPackage> CPackageList::FindByCode(std::wstring const& code) const
{
	auto found_member = Index::FindMember(m_IndexByCode, code);
	if (found_member.is_nil())
	{
		return nullptr;
	}
	return found_member->obj;
}
