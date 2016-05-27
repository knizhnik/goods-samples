// -- Goods Extensions
#pragma once

#include <vector>
#include <algorithm>
#include <functional>
#include "goods_container.h"

template<typename DestinationType, typename SourceType>
ref<DestinationType> goods_cast(ref<SourceType> const& obj)
{
	if(obj.is_nil())
		return nullptr;

	r_ref<object> r_obj = obj;
#if GOODS_RUNTIME_TYPE_CHECKING

	if (classof((DestinationType const*)0).is_superclass_for(obj->get_handle()))
		return static_cast< ref<DestinationType> >(obj);
	return nullptr;

#elif USE_DYNAMIC_TYPECAST

	if (NULL != dynamic_cast<const DestinationType*>(obj->get_handle()->obj))
		return static_cast< ref<DestinationType> >(obj);
	return nullptr;

#endif
}

template<typename DestinationType, typename SourceType>
ref<DestinationType> goods_cast(r_ref<SourceType> const& read_access_ref)
{
	return goods_cast<DestinationType>(ref<SourceType> { read_access_ref });
}

template<typename DestinationType, typename SourceType>
ref<DestinationType> goods_cast(w_ref<SourceType> const& write_access_ref)
{
	return goods_cast<DestinationType>(ref<SourceType> { write_access_ref });
}

template<typename DestinationType>
ref<DestinationType> goods_cast(const object* obj_ptr)
{
	return goods_cast<DestinationType>(ref<object> { obj_ptr });
}

template<typename DestinationType>
ref<DestinationType> goods_cast(const object_ref* object_ref_ptr)
{
	if (object_ref_ptr == nullptr)
	{
		return nullptr;
	}
	return goods_cast<DestinationType>(ref<object> { *object_ref_ptr });
}

template<typename T>
struct is_goods_class : std::is_base_of<object, T>
{
};

template<typename T>
struct is_goods_ref : std::integral_constant<bool, false>
{	
};

template<typename U>
struct is_goods_ref<ref<U>> : std::is_base_of<object, U>
{
};


inline BOOL is_member_used(ref<set_member> const& mbr)
{
	return !mbr.is_nil() && !mbr->owner.is_nil();
}

inline BOOL is_member_of(ref<set_member> const& mbr, ref<set_owner> const& owner)
{
	return !mbr.is_nil() && mbr->owner == owner;
}

inline BOOL remove_from_set_owner(ref<set_owner> owner, ref<set_member> mbr)
{
	auto w_owner = modify(owner);
	if (!is_member_of(mbr, owner))
		return FALSE;

	w_owner->remove(mbr);
	return TRUE;
}

ref<set_member> try_reuse_member(ref<set_member> cur_mbr, ref<set_member> const& new_mbr);

inline BOOL is_new_object(const object* obj)
{
	return obj->get_handle()->opid == 0;
}

inline BOOL is_new_object(ref<object> const& obj)
{
	return obj.get_handle()->opid == 0;
}

inline BOOL is_list_empty(ref<set_owner> const& list)
{
	return list.is_nil() || list->n_members == 0;
}

inline bool are_equal_or_nil(ref<object> const& lhs, ref<object> const& rhs)
{
	if (lhs.is_nil() || rhs.is_nil())
	{
		return true;
	}
	
	return lhs == rhs;
}

template <typename T>
ref<T> make_set_owner_copy(ref<T> const& list)
{
	ref<T> list_copy = T::create(list.is_nil() ? NULL : list->obj);

	if (is_list_empty(list))
		return list_copy;

	copy_set_owner_to(list, list_copy);

	return list_copy;
}

//remove all elements of a set_owner
void remove_all_members(ref<set_owner> list);			// -- use for peristent lists
void clear_setowner(ref<set_owner> list);				// -- use for in-memory lists but currently clears any set_owner even persistent ones
void clear_setowner_in_memory(ref<set_owner> list);		// -- force use for in-memory lists only



void copy_set_owner_to(ref<set_owner> const& source, ref<set_owner> destination);

template <typename T>
ref<set_owner> copy_set_owner_to_set_owner_if(ref<set_owner> const& source, std::function<bool(ref<T> const&)> filter)
{
	auto destination = set_owner::create(nullptr);
	auto std_destination = std_set_owner(destination);
	auto std_source = std_set_owner(source);

	std::copy_if(std_source.begin(), std_source.end(), std::back_inserter(std_destination), filter);

	return destination;
}

/** 
	Copy set_owner objects to a vector including those objects matching the condition
	@param[in] source: source set_owner	
	@param[in] filter_fnc: condition to be met

	@return Return the vector with the objects
*/
template <typename T>
std::vector<ref<T>> copy_set_owner_to_if(ref<set_owner> const& source, std::function<bool (ref<T> const&)> filter_fnc) 
{
	std::vector<ref<T>> ret_vector;
	if(source.is_nil())
		return std::move(ret_vector);

	ret_vector.reserve(source->n_members);

	auto source_cont = std_set_owner(source);
	if(filter_fnc)
		std::copy_if(begin(source_cont), end(source_cont), std::back_inserter(ret_vector), filter_fnc);
	else
		std::copy(begin(source_cont), end(source_cont), std::back_inserter(ret_vector));

	return std::move(ret_vector);
}

/** 
	Copy all set_owner objects to a vector 
	@param[in] source: source set_owner	

	@return Return the vector with the objects
*/
template <typename T>
std::vector<ref<T>> copy_set_owner_to(ref<set_owner> const& source) 
{
	return copy_set_owner_to_if<T>(source, nullptr);
}

/** 
	Copy vector objects to a set_owner including those objects matching the condition
	@param[in] source: source vector
	@param[in] filter_fnc: condition to be met

	@return Return the set_owner with the objects
*/
template <typename T>
ref<set_owner> copy_vector_to_if(std::vector<ref<T>> const& source, std::function<bool (ref<T> const&)> filter_fnc) 
{
	ref<set_owner> ret_list = set_owner::create(nullptr);
	if(source.empty())
		return ret_list;

	auto ret_cont = std_set_owner(ret_list);
	if(filter_fnc)
		std::copy_if(begin(source), end(source), std::back_inserter(ret_cont), filter_fnc);
	else
		std::copy(begin(source), end(source), std::back_inserter(ret_cont));

	return ret_list;
}

/** 
	Copy all vector objects to a set_owner
	@param[in] source: source vector

	@return Return the set_owner with the objects
*/
template <typename T>
ref<set_owner> copy_vector_to(std::vector<ref<T>> const& vector)
{
	return copy_vector_to_if<T>(vector, nullptr);
}

template <typename ListType>
ref<set_member> remove_and_advance(ref<ListType> list, ref<set_member> current_mbr)
{
	ref<set_member> next_mbr = current_mbr->next;
	modify(list)->remove(current_mbr);
	return next_mbr;
}

template<typename ListType, typename Predicate>
ref<set_member> find_first(ref<ListType> list, Predicate&& predicate)
{
	for (ref<set_member> mbr = list->first; !mbr.is_nil(); mbr = mbr->next)
	{
		if (predicate(mbr->obj))
		{
			return mbr;
		}
	}
	return nullptr;
}

template<typename ListType, typename Predicate>
inline void remove_if(ref<ListType> list, Predicate&& predicate)
{
	const auto first_match = find_first(list, predicate);
	if (first_match.is_nil())
	{
		return;
	}

	w_ref<ListType> w_list = list;
	for (ref<set_member> mbr = remove_and_advance(list, first_match); !mbr.is_nil();)
	{
		ref<set_member> current_mbr = mbr;
		mbr = mbr->next;

		if (predicate(current_mbr->obj))
		{
			w_list->remove(current_mbr);
		}
	}
}

inline ref<set_member> get_upper_bound(ref<set_member> start_mbr, std::string const& key)
{
	for (; !start_mbr.is_nil() && key == start_mbr->key; start_mbr = start_mbr->next);

	return start_mbr;
}

inline bool compare_key_with_prefix(ref<set_member> mbr, std::string const& prefix)
{
	return strncmp(prefix.c_str(), mbr->key, prefix.size()) == 0;
}

inline ref<set_member> get_prefix_upper_bound(ref<set_member> start_mbr, std::string const& prefix)
{
	for (; !start_mbr.is_nil() && compare_key_with_prefix(start_mbr, prefix); start_mbr = start_mbr->next);

	return start_mbr;
}
