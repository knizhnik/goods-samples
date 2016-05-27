//////////////////////////////////////////////////////////////////////////////
// Implementation file TimeBtree.cpp

#include "stdafx.h"

#include "TimeBtree.h"
//#include "DBMetaobject.h"
//#include "MagayaDBFields.h"


/////////////////////////////////////////////////////////////////////////////
// Implementation for class CTimeSetMember

REGISTER(CTimeSetMember, set_member, pessimistic_scheme);

CTimeSetMember::~CTimeSetMember()
{
}

ref<set_member> CTimeSetMember::create(ref<object> obj, time_t t)
{
	t = t ? t : time(NULL);
	return new(self_class, sizeof(time_t)) CTimeSetMember(self_class, obj, t);
}


field_descriptor& CTimeSetMember::describe_components()
{
	return
		NO_FIELDS;
}

skey_t CTimeSetMember::get_key() const
{
	skey_t skey = 0;

	if(getKeyLength() == 4)
	{
		DWORD t;
		memcpy(&t, key, sizeof(DWORD));
		skey = (skey_t)t;
	}
	else
	{
		time_t t;
		memcpy(&t, key, sizeof(time_t));
		skey = t;
	}

	return skey;
}

void CTimeSetMember::UpdateKeyVal(time_t new_key)
{
	if(getKeyLength() == 4)
	{
		DWORD t = (DWORD)new_key;
		memcpy(key, &t, sizeof(DWORD));
	}
	else
	{
		memcpy(key, &new_key, sizeof(time_t));
	}
}

int CTimeSetMember::compare(const char* thatKey) const
{
	//Keys are not strings
	assert(0);
    return 0;
}

int CTimeSetMember::compare(ref<set_member> mbr) const
{
    return -mbr->compare(key, size - this_offsetof(set_member, key));
}

int CTimeSetMember::compare(const char* thatKey, size_t thatSize) const
{
	time_t this_key = get_key();
	time_t that_key = 0;

	// copy 4 bytes or 8 bytes depending on the sizeof(time_t)
	memcpy(&that_key, thatKey, thatSize);

	return (int)(this_key - that_key);
}

int CTimeSetMember::compareIgnoreCase(const char* thatKey) const
{
    //Keys are not strings
	assert(0);
	return 0;
}

BOOL CTimeSetMember::NeedUpdatePosition(time_t tkey) const
{
	time_t curkey = (time_t)get_key();
	return curkey != tkey;
}

BOOL CTimeSetMember::UpdatePosition(time_t tkey)
{
	BOOL rt = FALSE; 
	if(NeedUpdatePosition(tkey))
	{
		ref<CTimeBtree> timetree = owner;
		write_access<CTimeBtree> wtimetree = modify(timetree);

		//remove from the owner
		wtimetree->RemovePlain(this);

		//update the key val
		UpdateKeyVal(tkey);

		//insert in the owner
		wtimetree->InsertPlain(this);

		rt = TRUE;
	}
	return rt;
}


/////////////////////////////////////////////////////////////////////////////
// Implementation for class CTimeBtree

REGISTER(CTimeBtree, B_tree, pessimistic_scheme);

field_descriptor& CTimeBtree::describe_components()
{
	return
		NO_FIELDS;
}

void CTimeBtree::insert(ref<set_member> mbr)
{
	InsertPlain(mbr);
}

void CTimeBtree::remove(ref<set_member> mbr)
{
	RemovePlain(mbr);
}

void CTimeBtree::InsertPlain(ref<set_member> mbr)
{
	B_tree::insert(mbr);
}

void CTimeBtree::RemovePlain(ref<set_member> mbr)
{
	B_tree::remove(mbr);
}

ref<set_member> CTimeBtree::FindFirst(time_t t) const
{
	return findGE((skey_t)t);
}

void CTimeBtree::GetDateRange(ref<CTimeBtree> btree, 
							  ref<set_member> &Start, 
							  ref<set_member> &End,
							  time_t StartTime /*= 0*/,
							  time_t EndTime /*= 0*/)
{
	Start = btree->first;
	End = btree->last;	

	if(StartTime && EndTime && StartTime > EndTime)
		return;
	
	if(StartTime)
	{
		Start = btree->findGE(StartTime);
		if(!Start.is_nil() && EndTime && Start->get_key() > (skey_t)EndTime)
			Start = NULL;
	}
	
	if(EndTime)
	{
		ref<set_member> mbr = btree->findGE(EndTime);
		//skip the equal elements
		while(!mbr.is_nil() && mbr->get_key() == EndTime)
			mbr = mbr->next;
		End = mbr.is_nil() ? End : mbr->prev;
		End = End.is_nil() ? Start : End;
	}

	if(Start.is_nil())
		End = NULL;
}
