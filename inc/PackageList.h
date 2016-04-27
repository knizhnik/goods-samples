#pragma once

class CPackage;

class CPackageList : public set_owner
{
public:
	METACLASS_DECLARATIONS(CPackageList, set_owner);

	static ref<CPackageList> create(ref<object> const& owner)
	{
		return NEW CPackageList(self_class, owner);
	}

	ref<CPackage> FindByCode(std::wstring const& code) const;

	virtual void insert(ref<set_member> mbr) override;
	virtual void remove(ref<set_member> mbr) override;

protected:
	CPackageList(class_descriptor& desc, ref<object> const& owner);

private:
	void InsertInIndexes(ref<CPackage> const& package);
	void RemoveFromIndexes(ref<CPackage> const& package);

private:
	ref<B_tree>	m_IndexByCode;
};