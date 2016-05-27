// -- Goods Extensions

#include "stdAfx.h"
#include "goodsex.h"

ref<set_member> try_reuse_member(ref<set_member> cur_mbr, ref<set_member> const& new_mbr)
{
	if (is_member_used(cur_mbr))
		return cur_mbr;

	ref<set_member> mbr;
	if (cur_mbr.is_nil())
		mbr = new_mbr;
	else
	{
		//update key to reuse opid
		//Before it was >= (early release 8.0) but we had to change it to == because
		//when calling the find() method of BTree it uses the set_member::compare method that
		//returns the difference of key lenghts when the keys were the same and this causes
		//the find to return NULL
		if (cur_mbr->getKeyLength() == new_mbr->getKeyLength())
		{
			memset(modify(cur_mbr)->key, 0, cur_mbr->getKeyLength());
			memcpy(modify(cur_mbr)->key, new_mbr->key, new_mbr->getKeyLength());
			mbr = cur_mbr;
		}
		else
			mbr = new_mbr;
	}
	return mbr;
}

static void do_remove_all_members(ref<set_owner> list)
{
	if(is_list_empty(list))
		return;
	
	const bool clear_members = is_new_object(list);

	write_access<set_owner> wlist = modify(list);
	for(ref<set_member> mbr = wlist->first; !mbr.is_nil();)
	{
		ref<set_member> tmp_mbr = mbr;
		mbr = mbr->next;
		wlist->remove(tmp_mbr);
		if(clear_members)
			modify(tmp_mbr)->clear();
	}

	if(clear_members)
		wlist->clear();
}

void remove_all_members(ref<set_owner> list)
{
	if(is_list_empty(list))
		return;

	do_remove_all_members(list);
}

void clear_setowner(ref<set_owner> list)
{
	if (list.is_nil())
		return;

	assert(is_new_object(list));
	
	do_remove_all_members(list);
}

void clear_setowner_in_memory(ref<set_owner> list)
{
	if (list.is_nil() || !is_new_object(list))
		return;

	clear_setowner(list);
}

ref<set_member> create_member(ref<object> const& obj)
{
	const char s = 0;
	return set_member::create(obj, &s, sizeof(s));
}

void copy_set_owner_to(ref<set_owner> const& source, ref<set_owner> destination)
{
	if (is_list_empty(source))
		return;

	write_access<set_owner> w_destination = modify(destination);
	for (ref<set_member> mbr = source->first; !mbr.is_nil(); mbr = mbr->next)
	{
		ref<object> obj = mbr->obj;
		w_destination->insert(create_member(obj));
	}
}
