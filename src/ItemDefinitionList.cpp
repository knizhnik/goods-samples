#include "stdafx.h"
#include "IndexHelper.h"
#include "ItemDefinition.h"
#include "ItemDefinitionList.h"


REGISTER(CItemDefinitionList, set_owner, pessimistic_scheme);

CItemDefinitionList::CItemDefinitionList(class_descriptor& desc, ref<object> const& owner)
	: set_owner(desc, owner)
	, m_IndexByCode(DbIndex::create(this))
	, m_IndexByDescription(DbIndex::create(this))
{
}

field_descriptor& CItemDefinitionList::describe_components()
{
	return 
		FIELD(m_IndexByCode),
		FIELD(m_IndexByDescription);
}

/*virtual override*/
void CItemDefinitionList::insert(ref<set_member> mbr)
{
	if (mbr.is_nil())
	{
		return;
	}
	set_owner::insert(mbr);

	const ref<CItemDefinition> item_definition = mbr->obj;
	InsertInIndexes(item_definition);
}

/*virtual override*/
void CItemDefinitionList::remove(ref<set_member> mbr)
{
	if (mbr.is_nil())
	{
		return;
	}
	set_owner::remove(mbr);

	const ref<CItemDefinition> item_definition = mbr->obj;
	RemoveFromIndexes(item_definition);
}

void CItemDefinitionList::InsertInIndexes(ref<CItemDefinition> const& item_definition)
{
	const auto code = item_definition->GetCode();
	const auto description = item_definition->GetDescription();

	Index::InsertInIndex(m_IndexByCode, item_definition, code);
	Index::InsertInIndex(m_IndexByDescription, item_definition, description);
}

void CItemDefinitionList::RemoveFromIndexes(ref<CItemDefinition> const& item_definition)
{
	const auto code = item_definition->GetCode();
	const auto description = item_definition->GetDescription();

	Index::RemoveFromIndex(m_IndexByCode, code);
	Index::RemoveFromIndex(m_IndexByDescription, description);
}

ref<set_member> CItemDefinitionList::FindByCode(std::wstring const code) const
{
	return Index::FindMember(m_IndexByCode, code);
}

ref<set_member> CItemDefinitionList::FindByDescription(std::wstring const description) const
{
	return Index::FindMember(m_IndexByDescription, description);
}
