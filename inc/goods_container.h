// --  goods_container.h
//
#pragma once

#include <algorithm>
#include <iterator>
#include <iostream>
#include <vector>

template<class Owner>
class goods_container_const_iterator 
{
public:
	typedef typename Owner::member_type			member_type;

	typedef typename Owner::value_type			value_type;
	typedef typename Owner::iterator_category	iterator_category;
	typedef typename Owner::difference_type		difference_type;
	typedef typename Owner::const_pointer		pointer;
	typedef typename Owner::const_reference		reference;

public:
	explicit goods_container_const_iterator(member_type const& mbr) : member(mbr) 
	{ }

	reference operator * () const 
	{
		return member->obj;
	}
	pointer operator -> () const 
	{
		return &**this;
	}

	goods_container_const_iterator operator ++ () 
	{		
		member = member->next;
		return *this;
	}
	goods_container_const_iterator operator ++ (int);

	goods_container_const_iterator operator -- () 
	{		
		member = member->prev;
		return *this;
	}
	goods_container_const_iterator operator--(int); 

	operator member_type() const
	{
		return member;
	}

	template<class Owner1>
	friend bool operator == (goods_container_const_iterator<Owner1> const& lhs, goods_container_const_iterator<Owner1> const& rhs);
	
	template<class Owner1>
	friend bool operator != (goods_container_const_iterator<Owner1> const& lhs, goods_container_const_iterator<Owner1> const& rhs);

private:
	member_type member;
};

template<class Owner1>
bool operator == (goods_container_const_iterator<Owner1> const& lhs, goods_container_const_iterator<Owner1> const& rhs)
{
	return lhs.member == rhs.member;
}

template<class Owner1>
bool operator != (goods_container_const_iterator<Owner1> const& lhs, goods_container_const_iterator<Owner1> const& rhs)
{
	return lhs.member != rhs.member;
}


template<class Owner>
class goods_container_iterator 
{
public:
	typedef typename Owner::member_type			member_type;

	typedef typename Owner::value_type			value_type;
	typedef typename Owner::iterator_category	iterator_category;
	typedef typename Owner::difference_type		difference_type;
	typedef typename Owner::pointer				pointer;
	typedef typename Owner::reference			reference;

public:
	explicit goods_container_iterator(member_type const& mbr) : member(mbr) 
	{ }

	reference operator * () const 
	{
		return member->obj;
	}
	pointer operator -> () const 
	{
		return &**this;
	}

	goods_container_iterator operator ++ () 
	{
		member = member->next;
		return *this;
	}
	goods_container_iterator operator ++ (int)
	{
		goods_container_iterator tmp(*this);
		--*this;
		return tmp;
	}

	goods_container_iterator operator -- () 
	{		
		member = member->prev;
		return *this;
	}
	goods_container_iterator operator -- (int) 
	{
		goods_container_iterator tmp(*this);
		--*this;
		return tmp;
	}

	operator member_type() const
	{
		return member;
	}

	template<class Owner1>
	friend bool operator == (goods_container_iterator<Owner1> const& lhs, goods_container_iterator<Owner1> const& rhs);

	template<class Owner1>
	friend bool operator != (goods_container_iterator<Owner1> const& lhs, goods_container_iterator<Owner1> const& rhs);

private:
	member_type		member;
};

template<class Owner1>
bool operator == (goods_container_iterator<Owner1> const& lhs, goods_container_iterator<Owner1> const& rhs)
{
	return lhs.member == rhs.member;
}

template<class Owner1>
bool operator != (goods_container_iterator<Owner1> const& lhs, goods_container_iterator<Owner1> const& rhs)
{
	return lhs.member != rhs.member;
}

template<class Owner>
inline goods_container_const_iterator<Owner> goods_container_const_iterator<Owner>::operator ++ (int) 
{
	goods_container_iterator<Owner> tmp(*this);
	++*this;
	return tmp;
}

template<class Owner>
inline goods_container_const_iterator<Owner> goods_container_const_iterator<Owner>::operator -- (int) 
{
	goods_container_iterator<Owner> tmp(*this);
	--*this;
	return tmp;
}

template<typename Container>
class goods_set_owner_traits;

template<>
struct goods_set_owner_traits<set_owner>
{
	typedef ref<set_owner>		container_type;
	typedef ref<set_member>		member_type;
	typedef ref<object>			value_type;

	static member_type get_first(container_type const& container) 
	{
		if(container.is_nil())
			return nullptr;
		return container->first;
	}
	static member_type get_last(container_type const& container) 
	{
		if(container.is_nil())
			return nullptr;
		return container->last;
	}
	static member_type begin(container_type const& container)
	{
		if(container.is_nil())
			return nullptr;
		return container->first;
	}
	static member_type end(container_type const& container)
	{
		return nullptr;
	}
	static bool empty(container_type const& container) 
	{
		return container.is_nil() || container->n_members == 0;
	}
	static void push_back(container_type& container, value_type value) 
	{
		modify(container)->insert(create_member(value));
	}
	static member_type create_member(value_type value)
	{
		static char key = 0;
		return set_member::create(value, &key, sizeof(char));
	}
	static size_t size(container_type const& container)
	{
		return container->n_members;
	}
};


template<class Container = set_owner>
class goods_container 
{
public:
	typedef goods_set_owner_traits<Container>					traits_type;
	typedef typename traits_type::container_type				container_type;
	typedef typename traits_type::member_type					member_type;
	typedef typename traits_type::value_type					value_type;

	typedef goods_container_iterator<goods_container>			iterator;
	typedef goods_container_const_iterator<goods_container>		const_iterator;
	typedef std::reverse_iterator<iterator>						reverse_iterator;
	typedef std::reverse_iterator<const_iterator>				const_reverse_iterator;
	typedef std::bidirectional_iterator_tag						iterator_category;

	typedef size_t												difference_type;
	typedef value_type*											pointer;
	typedef value_type											reference;
	typedef const value_type* 									const_pointer;
	typedef value_type const&									const_reference;
		
public:
	explicit goods_container(container_type const& cont) : container(cont)
	{
	}

	iterator begin() const
	{
		return iterator(traits_type::get_first(container));
	}
	reverse_iterator rbegin() const
	{
		member_type mbr = traits_type::create_member(nullptr);
		modify(mbr)->prev = traits_type::get_last(container);
		return reverse_iterator(iterator(mbr));
	}
	const_iterator cbegin() const 
	{		
		return const_iterator(traits_type::get_first(container));
	}
	iterator end() const
	{
		return iterator(traits_type::end(container));
	}
	reverse_iterator rend() const
	{
		return reverse_iterator(begin());
	}
	const_iterator cend() const 
	{
		return const_iterator(traits_type::end(container));
	}

	bool empty() const 
	{
		return traits_type::empty(container);
	}

	void push_back(value_type value) 
	{
		traits_type::push_back(container, value);
	}

	size_t size() const
	{
		return traits_type::size(container);
	}

private:
	container_type container;
};

template<class Container>
typename goods_container<Container>::iterator begin(goods_container<Container> const& container)
{
	return container.begin();
}

template<class Container>
typename goods_container<Container>::iterator end(goods_container<Container> const& container)
{
	return container.end();
}

template<class Container>
typename goods_container<Container>::const_iterator cbegin(goods_container<Container> const& container)
{
	return container.cbegin();
}

template<class Container>
typename goods_container<Container>::const_iterator cend(goods_container<Container> const& container)
{
	return container.cend();
}


template<class Container>
typename goods_container<Container>::iterator rbegin(goods_container<Container> const& container)
{
	return container.rbegin();
}

template<class Container>
typename goods_container<Container>::iterator rend(goods_container<Container> const& container)
{
	return container.rend();
}

template<class Container>
typename goods_container<Container>::const_iterator crbegin(goods_container<Container> const& container)
{
	return container.crbegin();
}

template<class Container>
typename goods_container<Container>::const_iterator crend(goods_container<Container> const& container)
{	
	return container.crend();
}


typedef goods_container<set_owner> std_set_owner;

inline std_set_owner::iterator begin(ref<set_owner> const& container)
{
	return std_set_owner(container).begin();
}

inline std_set_owner::iterator end(ref<set_owner> const& container)
{
	return std_set_owner(container).end();
}

inline std_set_owner::const_iterator cbegin(ref<set_owner> const& container)
{
	return std_set_owner(container).cbegin();
}

inline std_set_owner::const_iterator cend(ref<set_owner> const& container)
{
	return std_set_owner(container).cend();
}

inline std_set_owner::reverse_iterator begin(std::pair<std_set_owner::reverse_iterator, std_set_owner::reverse_iterator> i)
{
	return i.first;
}

inline std_set_owner::reverse_iterator end(std::pair<std_set_owner::reverse_iterator, std_set_owner::reverse_iterator> i)
{
	return i.second;
}

inline std::pair<std_set_owner::reverse_iterator, std_set_owner::reverse_iterator> reverse_set_owner(const std_set_owner& range)
{
	return std::make_pair(range.rbegin(), range.rend());
}
