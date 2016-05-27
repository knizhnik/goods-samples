
#include "stdafx.h"
#include "CustomFieldController.h"
#include "goods_container.h"
#include "goodsex.h"
#include "DimensionDataLayer.h"
#include "HazardousItem.h"
#include "ItemDefinitionDataLayer.h"
#include "ItemDefinitionList.h"
#include "FillDatabase.h"
#include "PackageDataLayer.h"
#include "RootDataLayer.h"
#include "WarehouseItemDataLayer.h"
#include "WarehouseReceiptDataLayer.h"
#include "WarehouseReceiptList.h"

#include <algorithm>
#include <vector>

static inline void text_out(const char* mesage)
{
	console::output(mesage);
	console::output("\r\n");
}

static void InsertPackage(const wchar_t* code, const wchar_t* description)
{
	w_ref<CPackage> w_package = db::package::Create(code, description);
	w_package->SetDimensions(db::dimensions::Create(10, 10, 10));
	db::package::Save(w_package);
}

static void FillPackages()
{
	if (db::package::GetCount() > 0)
	{
		text_out("packagees already created");
		return;
	}

	InsertPackage(L"BOX", L"Box");
	InsertPackage(L"CTN", L"Carton");
	InsertPackage(L"DRM", L"Drum");
	InsertPackage(L"PLT", L"Pallet");
	InsertPackage(L"TNK", L"Tank");
}

static ref<CItemDefinition> CreateItemDefinition(const wchar_t* code, const wchar_t* description, ref<CPackage> const& package, double weight, CItemDefinitionFlags flags)
{
	w_ref<CItemDefinition> w_item_def = db::item_def::Create(code, description);

	db::item_def::SetPackage(w_item_def, package);
	w_item_def->SetWeight(weight);
	w_item_def->SetFlags(flags, true);
	w_item_def->SetImage(description, sizeof(wchar_t) * (1 + wcslen(description)));

	return w_item_def;
}

static void InsertItemDefinition(const wchar_t* code, const wchar_t* description, ref<CPackage> const& package, double weight, CItemDefinitionFlags flags = CItemDefinitionFlags::None)
{
	auto item_def = CreateItemDefinition(code, description, package, weight, flags);
	db::item_def::Save(item_def);
}

static void FillItemDefinitions()
{
	if (db::item_def::GetCount() > 0)
	{
		text_out("item definitions already created");
		return;
	}

	const auto box = db::package::FindByCode(L"BOX");
	InsertItemDefinition(L"PRN001", L"Printer", box, 10);
	InsertItemDefinition(L"MON002", L"Monitor", box, 20);
	InsertItemDefinition(L"KBD003", L"Keyboard", box, 2);
	InsertItemDefinition(L"OPH002", L"Office Phone", box, 4);
	InsertItemDefinition(L"TBM002", L"Three Bottons Mouse", box, 1);

	const auto carton = db::package::FindByCode(L"CTN");
	InsertItemDefinition(L"PSH001", L"Paper Sheets", carton, 2);
	InsertItemDefinition(L"LBL001", L"Labels", carton, 2);

	const auto tank = db::package::FindByCode(L"TNK");
	InsertItemDefinition(L"KER001", L"Kerosene", tank, 10, CItemDefinitionFlags::HazMat);
}

static size_t GetRandomIndex(size_t max)
{
	return std::rand() % max;
}

static std::wstring GetRandomLastname()
{
	static const std::vector<std::wstring> lastnames
	{
		L"Smith",
		L"Jones",
		L"Lewis",
		L"Newman",
		L"Garcia",
		L"Perez",
		L"Книжник"
	};

	auto index = GetRandomIndex(lastnames.size());
	return lastnames[index];
}

static std::wstring GetRandomFirnstname()
{
	static const std::vector<std::wstring> firstnames 
	{
		L"John",
		L"Michael",
		L"Anthony",
		L"George",
		L"Javier",
		L"Nichole",
		L"Maria",
		L"Jose",
		L"Константин"
	};

	auto index = GetRandomIndex(firstnames.size());
	return firstnames[index];
}

static std::wstring GetRandomName()
{
	auto name = GetRandomFirnstname();
	name += L" ";
	name += GetRandomLastname();
	return name;
}

static ref<CItemDefinition> GetNextItemDefinition()
{
	static ref<set_member> next_item_def_member = db::GetDatabaseRoot()->GetItemDefinitionList()->first;

	if (next_item_def_member.is_nil())
	{
		next_item_def_member = db::GetDatabaseRoot()->GetItemDefinitionList()->first;
	}

	auto item_def = next_item_def_member->obj;
	next_item_def_member = next_item_def_member->next;
	return item_def;
}

static std::wstring GetRandomLocationCode()
{
	std::wstring location_code = L"LOC";
	location_code += std::to_wstring(GetRandomIndex(100));
	return location_code;
}

static std::vector<char> GetItemBlobDaata()
{
	std::vector<char> blob { 0x40, 0x40, 0x40, 0x40 };
	return std::move(blob);
}

static void SetUpHazMatItem(ref<CWarehouseItem> wh_item)
{
	auto hazmat_item = goods_cast<CHazardousItem>(wh_item);
	if (hazmat_item.is_nil())
	{
		return;
	}
	modify(hazmat_item)->SetHazMatCode(L"HZM-12");
	modify(hazmat_item)->SetExtraData(GetItemBlobDaata());
}

static ref<CWarehouseItem> CreateWarehouseItem()
{
	w_ref<CWarehouseItem> wh_item = db::wh_item::Create(GetNextItemDefinition());
	
	SetUpHazMatItem(wh_item);

	wh_item->SetLocationCode(GetRandomLocationCode().c_str());

	return wh_item;
}

static const std::vector<std::pair<std::wstring, std::wstring>>& GetCustomFieldsValue()
{
	static const std::vector<std::pair<std::wstring, std::wstring>> fields_values
	{
		std::make_pair(L"CustomFieldOne", L"One"),
		std::make_pair(L"CustomFieldTwo", L"Two")
	};

	return fields_values;
}

static std::string CreateEmail(std::wstring const& name)
{
	std::string email { begin(name), end(name) };
	auto found_it = std::find(begin(email), end(email), 0x20);
	if (found_it != end(email))
	{
		*found_it = "."[0];
	}
	email += "@magaya.com";
	return email;
}

static ref<CWarehouseReceipt> CreateWarehouseReceipt(db::db_filler::FillDatabaseData const& db_fill_data, const wchar_t* number)
{
	w_ref<CWarehouseReceipt> wh_receipt = db::warehouse_receipt::Create(number);
	
	wh_receipt->SetConsigneeName(GetRandomName().c_str());

	const auto shipper_name = GetRandomName();
	wh_receipt->SetShipperName(shipper_name.c_str());
	wh_receipt->SetShipperEmail(CreateEmail(shipper_name).c_str());
	
	for (size_t i = 0; i < GetRandomIndex(db_fill_data.MaxItemsPerWarehouseReceipt); ++i)
	{
		db::warehouse_receipt::AddItem(wh_receipt, CreateWarehouseItem());
	}

	for (auto& field_value : GetCustomFieldsValue())
	{
		db::custom_fields::SetCustomField(anyref(wh_receipt), field_value.first.c_str(), field_value.second.c_str());
	}

	return wh_receipt;
}

static void FillWarehouseReceipts(db::db_filler::FillDatabaseData const& db_fill_data)
{
	if (db::warehouse_receipt::GetCount() > 0)
	{
		return;
	}

	std::srand(time(nullptr));

	for (size_t i = 0; i < db_fill_data.WarehouseReceiptCount; ++i)
	{
		auto wh_receipt = CreateWarehouseReceipt(db_fill_data, std::to_wstring(i).c_str());		

		db::warehouse_receipt::Save(wh_receipt);
	}
}

static void SwapEntities(write_access<CWarehouseReceipt>& w_whr)
{
	auto shipper_name = w_whr->GetShipperName();
	auto consignee_name = w_whr->GetConsigneeName();
	std::swap(shipper_name, consignee_name);

	w_whr->SetShipperName(shipper_name.c_str());
	w_whr->SetConsigneeName(consignee_name.c_str());
}

static void EditWarehouseReceipts(db::db_filler::FillDatabaseData const& db_fill_data)
{
	auto whr_list = db::warehouse_receipt::GetList();
	
	for (ref<CWarehouseReceipt> whr : whr_list)
	{
		auto w_whr = modify(whr);
		
		SwapEntities(w_whr);
		auto whr_number = w_whr->GetNumber();
		whr_number += L"_1";
		w_whr->SetNumber(whr_number.c_str());

		db::warehouse_receipt::Save(whr);
	}
}

void db::db_filler::FillDatabase(FillDatabaseData const& db_fill_data)
{
	FillPackages();
	FillItemDefinitions();
	FillWarehouseReceipts(db_fill_data);
	EditWarehouseReceipts(db_fill_data);
}

static bool TestItemDefinitionsIntegrity()
{
	const auto item_definitions_list = db::item_def::GetList();
	return std::all_of(begin(item_definitions_list), end(item_definitions_list), [&item_definitions_list] (ref<CItemDefinition> const& item_def)
	{
		const auto code = item_def->GetCode();
		const auto code_member = item_definitions_list->FindByCode(code);

		if (code_member.is_nil() || code_member->obj != item_def)
		{
			text_out("item definition is not in the index by code");
			return false;
		}

		const auto description = item_def->GetDescription();
		const auto desc_member = item_definitions_list->FindByDescription(description);

		if (desc_member.is_nil() || desc_member->obj != item_def)
		{
			text_out("item definition is not in the index by description");
			return false;
		}

		const auto image = item_def->GetImage();
		if (0 != memcmp(image->get_body(), item_def->GetDescription().c_str(), image->get_size()))
		{
			text_out("item definition image (external blob) failed");
			return false;
		}

		return true;
	});
}

static bool TestWarehouseItem(ref<CWarehouseReceipt> const& whr, ref<CWarehouseItem> const wh_item)
{
	if (wh_item->GetWarehouseReceipt() != whr)
	{
		text_out("item has a wrong warehouse receipt or is missing");
		return false;
	}

	const auto whr_item_list = whr->GetItems();
	const auto whr_item_list_member = wh_item->GetIndexMember(WarehouseItemIndexMemberEnum::WarehouseReceiptList);
	if (whr_item_list_member->owner != whr_item_list)
	{
		text_out("item has a wrong warehouse item list member or is missing");
		return false;
	}
	
	const auto item_definition = wh_item->GetItemDefinition();
	if (wh_item->GetPackage() != item_definition->GetPackage())
	{
		text_out("item package is different to its item definition package");
		return false;
	}

	return true;
}

static bool TestCustomField(ref<CDbObject> const& db_object, const wchar_t* name, const wchar_t* value)
{
	const auto custom_field_result = db::custom_fields::GetCustomField(db_object, name);
	return custom_field_result.first && custom_field_result.second == value;
}

static bool TestCustomFields(ref<CWarehouseReceipt> const& whr)
{
	const auto& field_values = GetCustomFieldsValue();
	return all_of(begin(field_values), end(field_values), [&whr] (std::pair<std::wstring, std::wstring> const& value)
	{
		return TestCustomField(whr, value.first.c_str(), value.second.c_str());
	});
}

static bool TestShipperEmail(ref<CWarehouseReceipt> const& whr)
{
	const auto email = whr->GetShipperEmail();
	const std::string domain = "@magaya.com";

	return std::equal(rbegin(email), rbegin(email) + domain.size(), rbegin(domain));
}

static bool TestWarehouseReceiptIntegrity(ref<CWarehouseReceipt> const& whr)
{
	if (!TestCustomFields(whr))
	{
		text_out("failed testing custom fields");
		return false;
	}

	if (!TestShipperEmail(whr))
	{
		text_out("failed testing warehouse receipt shipper e-mail");
		return false;
	}

	const auto items = whr->GetItems();

	return std::all_of(begin(items), end(items), [&whr] (ref<CWarehouseItem> const& wh_item)
	{
		return TestWarehouseItem(whr, wh_item);
	});
}

static bool TestWarehouseReceiptsIntegrity()
{
	const auto whr_list = db::warehouse_receipt::GetList();

	return std::all_of(begin(whr_list), end(whr_list), [&whr_list] (ref<CWarehouseReceipt> const& whr)
	{
		const auto found_mbr = whr_list->FindByNumber(whr->GetNumber());
		if (found_mbr.is_nil() || found_mbr->obj != whr)
		{
			text_out("warehouse receipt not found in list by number");
			return false;
		}

		return TestWarehouseReceiptIntegrity(whr);
	});
}

bool db::db_filler::TestIntegrity()
{
	if (!TestItemDefinitionsIntegrity())
	{
		text_out("Item definitions integrity check failed");
		return false;
	}
	text_out("Item definitions integrity check passed succesfully");

	if (!TestWarehouseReceiptsIntegrity())
	{
		text_out("warehouse receipts integrity check failed");
		return false;
	}
	text_out("warehouse receipts integrity passed succesfully");

	text_out("integrity test passed succesfully");
	return true;
}

static void DeleteAllWarehouseReceipts()
{
	auto whr_list = db::warehouse_receipt::GetList();
	while (whr_list->n_members > 0)
	{
		ref<CWarehouseReceipt> whr = whr_list->first->obj;
		db::warehouse_receipt::Delete(whr);
	}
}

void db::db_filler::DeleteAll()
{
	DeleteAllWarehouseReceipts();
}
