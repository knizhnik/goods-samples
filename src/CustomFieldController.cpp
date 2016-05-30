
#include "stdafx.h"
#include "CustomField.h"
#include "CustomFieldController.h"
#include "DbObject.h"
#include "goods_container.h"
#include "IndexHelper.h"

#include <algorithm>


static ref<set_owner> GetCustomFields(ref<CDbObject> const& db_object)
{
	auto custom_fields = db_object->GetCustomFields();
	if (custom_fields.is_nil())
	{
		custom_fields = set_owner::create(db_object);
		modify(db_object)->SetCustomFields(custom_fields);
	}
	return custom_fields;
}


static ref<CCustomField> FindCustomField(ref<set_owner> const& custom_fields, const wchar_t* name)
{
	const auto found_it = std::find_if(begin(custom_fields), end(custom_fields), [name] (ref<CCustomField> const& custom_field)
	{
		return custom_field->GetName() == name;
	});

	if (found_it == end(custom_fields))
	{
		return nullptr;
	}

	return *found_it;
}

void db::custom_fields::SetCustomField(w_ref<CDbObject> db_object, const wchar_t* name, const wchar_t* value)
{
	auto custom_fields = GetCustomFields(db_object);

	auto custom_field = FindCustomField(custom_fields, name);
	if (custom_field.is_nil())
	{
		custom_field = CCustomField::create(name, value);
		auto insert_member = Index::CreateMember(custom_field);
		modify(custom_fields)->insert(insert_member);
	}
	else
	{
		modify(custom_field)->SetValue(value);
	}
}

std::pair<bool, std::wstring> db::custom_fields::GetCustomField(ref<CDbObject> const& db_object, const wchar_t* name)
{
	auto custom_fields = db_object->GetCustomFields();
	if (custom_fields.is_nil())
	{
		return std::make_pair(false, L"");
	}

	const auto custom_field = FindCustomField(custom_fields, name);
	if (custom_field.is_nil())
	{
		return std::make_pair(false, L"");
	}

	return std::make_pair(true, custom_field->GetValue());
}
