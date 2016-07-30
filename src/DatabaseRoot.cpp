#include "stdafx.h"
#include "DatabaseRoot.h"

#include "PackageList.h"
#include "GlobalIndex.h"
#include "ItemDefinitionList.h"
#include "WarehouseItemList.h"
#include "WarehouseReceiptList.h"

REGISTER(CDatabaseRoot, object, pessimistic_scheme);

CDatabaseRoot::CDatabaseRoot()
	: object(self_class)
{
}

field_descriptor& CDatabaseRoot::describe_components()
{
	return
		FIELD(m_Packages),
		FIELD(m_ItemDefinitions),
		FIELD(m_WarehouseItems),
		FIELD(m_WarehouseReceipts),
		FIELD(m_GlobalIndex);
}

ref<CDatabaseRoot> CDatabaseRoot::create() const
{
	ref<CDatabaseRoot> root = this;
	modify(root)->become(NEW CDatabaseRoot(root));
	modify(root)->SetupNewDatabase();
	return root;
}

void CDatabaseRoot::SetupNewDatabase()
{
	m_Packages = CPackageList::create(this);
	m_ItemDefinitions = CItemDefinitionList::create(this);
	m_WarehouseItems = CWarehouseItemList::create(this);
	m_WarehouseReceipts = CWarehouseReceiptList::create(this);
	m_GlobalIndex = CGlobalIndex::create(this);
}
