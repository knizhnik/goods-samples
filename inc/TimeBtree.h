////////////////////////////////////////////////////////////////////////////////
// Header file TimeBtree.h

#ifndef TIMEBTREE_H
#define TIMEBTREE_H

class CTimeSetMember : public set_member
{
public:
	METACLASS_DECLARATIONS(CTimeSetMember, set_member);
	CTimeSetMember(class_descriptor& desc, ref<object> const& obj, time_t t)
		: set_member(desc, obj, (const char*)&t, sizeof(time_t)) 
	{
	}
	~CTimeSetMember();

	static ref<set_member> create(ref<object> obj, time_t t);

	BOOL NeedUpdatePosition(time_t tkey) const;
	BOOL UpdatePosition(time_t tkey);
	void UpdateKeyVal(time_t new_key);

    virtual int     compare(ref<set_member> mbr) const;
    virtual int     compare(const char* key, size_t size) const; 
    virtual int     compare(const char* key) const; 
    virtual int     compareIgnoreCase(const char* key) const; 

//Virtuals    
	virtual skey_t  get_key() const;
};

class CTimeBtree : public B_tree
{
public:
	METACLASS_DECLARATIONS(CTimeBtree, B_tree);
	CTimeBtree(class_descriptor& desc, ref<object> const& obj)
		   : B_tree(desc, obj) { }
	static ref<CTimeBtree> create(ref<object> const& obj){
		return NEW CTimeBtree(self_class, obj);
	}

	ref<set_member> FindFirst(time_t t) const;
	static void GetDateRange(ref<CTimeBtree> btree, 
							 ref<set_member> &Start, 
							 ref<set_member> &End,
							 time_t StartTime = 0,
							 time_t EndTime = 0);

//Virtuals
    virtual void insert(ref<set_member> mbr);
    virtual void remove(ref<set_member> mbr);
	virtual void InsertPlain(ref<set_member> mbr);
	virtual void RemovePlain(ref<set_member> mbr);
};

#endif //TIMEBTREE_H
