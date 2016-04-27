
#pragma once

#include <algorithm>

namespace index
{
	template<typename KeyType>
	struct IndexKeyTraits;

	template<>
	struct IndexKeyTraits<std::wstring>
	{
		static ref<set_member> CreateMember(ref<object> const& obj, std::wstring const& key)
		{
			std::string char_key(begin(key), end(key));
			return set_member::create(obj, CreatePersistentKey(key).c_str());
		}

		static ref<set_member> FindMember(ref<B_tree> const& index, std::wstring const& key)
		{
			const auto pesistent_key = CreatePersistentKey(key);
			const ref<set_member> found_member = index->findGE(pesistent_key.c_str());
			if (found_member->compare(pesistent_key.c_str()) != 0)
			{
				return nullptr;
			}
			return found_member;
		}

		static ref<set_member> InsertInIndex(w_ref<B_tree> w_index, ref<object> const& obj, std::wstring const& key)
		{
			auto insert_member = CreateMember(obj, key);
			w_index->insert(insert_member);
			return insert_member;
		}

		static void RemoveFromIndex(w_ref<B_tree> w_index, std::wstring const& key)
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
	struct IndexKeyTraits<time_t>
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

	template<typename KeyType>
	ref<set_member> CreateMember(ref<object> const& obj, KeyType const& key)
	{
		return IndexKeyTraits<KeyType>::CreateMember(obj, key);
	}

	inline ref<set_member> CreateMember(ref<object> const& obj)
	{
		return CreateMember(obj, std::wstring(L""));
	}

	template<typename KeyType>
	ref<set_member> FindMember(ref<B_tree> const& index, KeyType const& key)
	{
		return IndexKeyTraits<KeyType>::FindMember(index, key);
	}

	template<typename KeyType>
	ref<set_member> InsertInIndex(w_ref<B_tree> w_index, ref<object> const& obj, KeyType const& key)
	{
		return IndexKeyTraits<KeyType>::InsertInIndex(w_index, obj, key);
	}

	template<typename KeyType>
	void RemoveFromIndex(w_ref<B_tree> w_index, KeyType const& key)
	{
		IndexKeyTraits<KeyType>::RemoveFromIndex(w_index, key);
	}
}