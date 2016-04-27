
#pragma once

#include "ItemDefinition.h"

class CItemDefinitionList;

namespace db
{
	namespace item_def
	{

		ref<CItemDefinition> Create(const wchar_t* code, const wchar_t* description);

		void SetPackage(w_ref<CItemDefinition> w_item_definition, ref<CPackage> const& package);
		void Save(ref<CItemDefinition> const& item_def);
		size_t GetCount();
		ref<CItemDefinitionList> GetList();

	}	// namespace item_def
}	// namespace db