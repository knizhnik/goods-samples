
#pragma once

class CDbObject;

namespace db
{
	namespace custom_fields
	{
		void SetCustomField(w_ref<CDbObject> db_object, const wchar_t* name, const wchar_t* value);
		std::pair<bool, std::wstring> GetCustomField(ref<CDbObject> const& db_object, const wchar_t* name);

	}	// namespace custom_fields

}	// namespace db