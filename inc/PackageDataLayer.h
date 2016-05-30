
#pragma once

#include "Package.h"

class CPackageList;

namespace db
{
	namespace package
	{

		ref<CPackage> Create(const wchar_t* code, const wchar_t* description);
		ref<CPackage> FindByCode(const wchar_t* code);
		void Save(ref<CPackage> const& package);
		size_t GetCount();
		ref<CPackageList> GetList();

	}	// namespace package
}	// namespace db
