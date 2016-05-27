
#pragma once

#include "TimeBtree.h"

#include <algorithm>

namespace Index
{
	template<typename TreeType, typename KeyType>
	struct IndexKeyTraits;

	template<typename TreeType>
	struct IndexKeyTraits<TreeType, std::wstring>
	{
		static ref<set_member> CreateMember(ref<object> const& obj, std::wstring const& key)
		{
			std::string char_key(begin(key), end(key));
			return set_member::create(obj, CreatePersistentKey(key).c_str());
		}

		static ref<set_member> FindMember(ref<TreeType> const& index, std::wstring const& key)
		{
			const auto pesistent_key = CreatePersistentKey(key);
			const ref<set_member> found_member = index->findGE(pesistent_key.c_str());
			if (found_member.is_nil() || found_member->compare(pesistent_key.c_str()) != 0)
			{
				return nullptr;
			}
			return found_member;
		}

		static ref<set_member> InsertInIndex(w_ref<TreeType> w_index, ref<object> const& obj, std::wstring const& key)
		{
			auto insert_member = CreateMember(obj, key);
			w_index->insert(insert_member);
			return insert_member;
		}

		static void RemoveFromIndex(w_ref<TreeType> w_index, std::wstring const& key)
		{
			auto remove_member = FindMember(w_index, key);
			if (!remove_member.is_nil())
			{
				w_index->remove(remove_member);
			}
		}

	private:
		static std::string CreatePersistentKey(std::wstring const& key)
		{
			std::string persistent_key;
			std::transform(begin(key), end(key), std::back_inserter(persistent_key), toupper);
			return std::move(persistent_key);
		}
	};

	template<>
	struct IndexKeyTraits<B_tree, time_t>
	{
		static ref<set_member> CreateMember(ref<object> const& obj, time_t key)
		{
			return set_member::create(obj, reinterpret_cast<const char*>(&key), sizeof(time_t));
		}

		static ref<set_member> FindMember(ref<B_tree> const& index, time_t key)
		{			
			const ref<set_member> found_member = index->findGE(skey_t(key));
			if (skey_t(found_member->key) != skey_t(key))
			{
				return nullptr;
			}
			return found_member;
		}

		static ref<set_member> InsertInIndex(w_ref<B_tree> w_index, ref<object> const& obj, time_t key)
		{
			auto insert_member = CreateMember(obj, key);
			w_index->insert(insert_member);
			return insert_member;
		}

		static void RemoveFromIndex(w_ref<B_tree> w_index, time_t key)
		{
			auto remove_member = FindMember(w_index, key);
			if (!remove_member.is_nil())
			{
				w_index->remove(remove_member);
			}
		}	
	};

	template<>
	struct IndexKeyTraits<CTimeBtree, time_t>
	{
		static ref<set_member> CreateMember(ref<object> const& obj, time_t key)
		{
			return CTimeSetMember::create(obj, key);
		}

		static ref<set_member> FindMember(ref<CTimeBtree> const& index, time_t key)
		{
			const ref<set_member> found_member = index->FindFirst(key);
			if (skey_t(found_member->key) != skey_t(key))
			{
				return nullptr;
			}
			return found_member;
		}

		static ref<set_member> InsertInIndex(ref<CTimeBtree> index, ref<object> const& obj, time_t key)
		{
			auto insert_member = CreateMember(obj, key);
			modify(index)->insert(insert_member);
			return insert_member;
		}

		static void RemoveFromIndex(ref<CTimeBtree> index, time_t key)
		{
			auto u_index = update(index);
			auto remove_member = FindMember(u_index, key);
			if (!remove_member.is_nil())
			{
				modify(index)->remove(remove_member);
			}
		}
	};

	template<typename TreeType, typename KeyType>
	ref<set_member> CreateMember(ref<object> const& obj, KeyType const& key)
	{
		return IndexKeyTraits<TreeType, KeyType>::CreateMember(obj, key);
	}

	inline ref<set_member> CreateMember(ref<object> const& obj)
	{
		return CreateMember<B_tree>(obj, std::wstring(L""));
	}

	template<typename TreeType, typename KeyType>
	ref<set_member> FindMember(ref<TreeType> const& index, KeyType const& key)
	{
		return IndexKeyTraits<TreeType, KeyType>::FindMember(index, key);
	}

	template<typename TreeType, typename KeyType>
	ref<set_member> InsertInIndex(ref<TreeType> index, ref<object> const& obj, KeyType const& key)
	{
		return IndexKeyTraits<TreeType, KeyType>::InsertInIndex(index, obj, key);
	}

	template<typename TreeType, typename KeyType>
	void RemoveFromIndex(ref<TreeType> index, KeyType const& key)
	{
		IndexKeyTraits<TreeType, KeyType>::RemoveFromIndex(index, key);
	}
}
