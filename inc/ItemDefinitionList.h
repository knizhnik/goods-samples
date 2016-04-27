#pragma once

class CItemDefinition;

class CItemDefinitionList : public set_owner
{
public:
	METACLASS_DECLARATIONS(CItemDefinitionList, set_owner);

	static ref<CItemDefinitionList> create(ref<object> const& owner)
	{
		return NEW CItemDefinitionList(self_class, owner);
	}

	virtual void insert(ref<set_member> mbr) override;
	virtual void remove(ref<set_member> mbr) override;

	ref<set_member> FindByCode(std::wstring const code) const;
	ref<set_member> FindByDescription(std::wstring const description) const;

protected:
	CItemDefinitionList(class_descriptor& desc, ref<object> const& owner);

private:
	void InsertInIndexes(ref<CItemDefinition> const& warehouse_receipt);
	void RemoveFromIndexes(ref<CItemDefinition> const& warehouse_receipt);

private:
	ref<B_tree>	m_IndexByCode;
	ref<B_tree>	m_IndexByDescription;
};