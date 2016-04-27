
#include "stdafx.h"
#include "goods_container.h"
#include "DimensionDataLayer.h"
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
		return;
	}

	InsertPackage(L"BOX", L"Box");
	InsertPackage(L"CTN", L"Carton");
	InsertPackage(L"DRM", L"Drum");
	InsertPackage(L"PLT", L"Pallet");
	InsertPackage(L"TNK", L"Tank");
}

static ref<CItemDefinition> CreateItemDefinition(const wchar_t* code, const wchar_t* description, ref<CPackage> const& package, double weight)
{
	w_ref<CItemDefinition> w_item_def = db::item_def::Create(code, description);

	db::item_def::SetPackage(w_item_def, package);
	w_item_def->SetWeight(weight);

	return w_item_def;
}

static void InsertItemDefinition(const wchar_t* code, const wchar_t* description, ref<CPackage> const& package, double weight)
{
	auto item_def = CreateItemDefinition(code, description, package, weight);
	db::item_def::Save(item_def);
}

static void FillItemDefinitions()
{
	if (db::item_def::GetCount() > 0)
	{
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
		L"Nochole",
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

static ref<CWarehouseItem> CreateWarehouseItem()
{
	w_ref<CWarehouseItem> wh_item = db::wh_item::Create(GetNextItemDefinition());

	wh_item->SetLocationCode(GetRandomLocationCode().c_str());

	return wh_item;
}

static ref<CWarehouseReceipt> CreateWarehouseReceipt(db::db_filler::FillDatabaseData const& db_fill_data, const wchar_t* number)
{
	w_ref<CWarehouseReceipt> wh_receipt = db::warehouse_receipt::Create(number);
	
	wh_receipt->SetConsigneeName(GetRandomName().c_str());
	wh_receipt->SetShipperName(GetRandomName().c_str());
	
	for (size_t i = 0; i < GetRandomIndex(db_fill_data.MaxItemsPerWarehouseReceipt); ++i)
	{
		db::warehouse_receipt::AddItem(wh_receipt, CreateWarehouseItem());
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

void db::db_filler::FillDatabase(FillDatabaseData const& db_fill_data)
{
	FillPackages();
	FillItemDefinitions();
	FillWarehouseReceipts(db_fill_data);
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
			return false;
		}

		const auto description = item_def->GetDescription();
		const auto desc_member = item_definitions_list->FindByDescription(description);

		if (desc_member.is_nil() || desc_member->obj != item_def)
		{
			return false;
		}

		return true;
	});
}

static bool TestWarehouseItem(ref<CWarehouseReceipt> const& whr, ref<CWarehouseItem> const wh_item)
{
	if (wh_item->GetWarehouseReceipt() != whr)
	{
		return false;
	}

	const auto whr_item_list = whr->GetItems();
	const auto whr_item_list_member = wh_item->GetWarehouseReceiptMember();
	if (whr_item_list_member->owner != whr_item_list)
	{
		return false;
	}
	
	const auto item_definition = wh_item->GetItemDefinition();
	if (wh_item->GetPackage() != item_definition->GetPackage())
	{
		return false;
	}

	return true;
}

static bool TestWarehouseReceiptIntegrity(ref<CWarehouseReceipt> const& whr)
{
	const auto items = whr->GetItems();
	return std::all_of(begin(items), end(items), [&whr] (ref<CWarehouseItem> const& wh_item)
	{
		return TestWarehouseItem(whr, wh_item);
	});
}

static bool TestWarehouseReceiptsIntegrity()
{
	const auto whr_list = db::warehouse_receipt::GetList();
	const auto whr_list_by_date = whr_list->GetByDateList();

	return std::all_of(begin(whr_list_by_date), end(whr_list_by_date), [&whr_list] (ref<CWarehouseReceipt> const& whr)
	{
		const auto found_mbr = whr_list->FindByNumber(whr->GetNumber());
		if (found_mbr.is_nil() || found_mbr->obj != whr)
		{
			return false;
		}

		return TestWarehouseReceiptIntegrity(whr);
	});
}

bool db::db_filler::TestIntegrity()
{
	if (!TestItemDefinitionsIntegrity())
	{
		return false;
	}

	if (!TestWarehouseReceiptsIntegrity())
	{
		return false;
	}

	return true;
}