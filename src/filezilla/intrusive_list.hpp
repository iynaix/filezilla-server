#ifndef FZ_INTRUSIVE_LIST_HPP
#define FZ_INTRUSIVE_LIST_HPP

#include <type_traits>

namespace fz {

template <typename Node>
struct intrusive_list;

struct intrusive_node
{
	constexpr intrusive_node()
		: prev_(this)
		, next_(this)
	{}

	intrusive_node &remove()
	{
		next_->prev_ = prev_;
		prev_->next_ = next_;

		return *this;
	}

private:
	template <typename Node>
	friend struct intrusive_list;

	void reset()
	{
		prev_ = next_ = this;
	}

	operator bool() const
	{
		return prev_ != this || next_ != this;
	}

	intrusive_node *prev_;
	intrusive_node *next_;
};

struct virtual_intrusive_node: intrusive_node
{
	virtual ~virtual_intrusive_node() = default;
};

template <typename Node>
struct intrusive_list
{
	using node_type = Node;

	constexpr intrusive_list() noexcept
	{
		static_assert(std::is_base_of_v<intrusive_node, Node>, "Node template parameter must be a class derived from intrusive_node");
	}

	intrusive_list(intrusive_list const&) = delete;
	intrusive_list& operator=(intrusive_list const&) = delete;

	constexpr intrusive_list(intrusive_list&& rhs) noexcept
	{
		if (&rhs != this)
			splice(end(), rhs);
	}

	constexpr intrusive_list& operator=(intrusive_list&& rhs) noexcept
	{
		if (&rhs != this)
			splice(end(), rhs);

		return *this;
	}

	struct iterator
	{
		constexpr Node& operator *()
		{
			return static_cast<Node&>(*cur_);
		}

		constexpr Node* operator ->()
		{
			return static_cast<Node*>(cur_);
		}

		constexpr bool operator !=(iterator const& rhs)
		{
			return cur_ != rhs.cur_;
		}

		constexpr bool operator ==(iterator const& rhs)
		{
			return cur_ == rhs.cur_;
		}

		constexpr iterator& operator++()
		{
			cur_ = cur_->next_;
			return *this;
		}

		constexpr iterator& operator--()
		{
			cur_ = cur_->prev_;
			return *this;
		}

	private:
		friend struct intrusive_list;
		constexpr iterator(intrusive_node *n) noexcept
			: cur_(n)
		{
		}

		intrusive_node *cur_;
	};

	struct const_iterator
	{
		constexpr Node const& operator *() const
		{
			return static_cast<Node const&>(*cur_);
		}

		constexpr Node const* operator ->() const
		{
			return static_cast<Node const*>(cur_);
		}

		constexpr bool operator !=(const_iterator const& rhs) const
		{
			return cur_ != rhs.cur_;
		}

		constexpr bool operator ==(const_iterator const& rhs) const
		{
			return cur_ == rhs.cur_;
		}

		constexpr const_iterator& operator++()
		{
			cur_ = cur_->next_;
			return *this;
		}

		constexpr const_iterator& operator--()
		{
			cur_ = cur_->prev_;
			return *this;
		}

	private:
		friend struct intrusive_list;
		constexpr const_iterator(intrusive_node const* n) noexcept
			: cur_(n)
		{
		}

		intrusive_node const* cur_;
	};

	constexpr iterator begin()
	{
		return head_.next_;
	}

	constexpr iterator end()
	{
		return &head_;
	}

	constexpr const_iterator begin() const
	{
		return head_.next_;
	}

	constexpr const_iterator end() const
	{
		return &head_;
	}

	constexpr void insert(Node& next, Node& n)
	{
		n.intrusive_node::prev_ = next.intrusive_node::prev_;
		n.intrusive_node::next_ = &next;

		n.intrusive_node::prev_->next_ = &n;
		n.intrusive_node::next_->prev_ = &n;
	}

	constexpr iterator insert(const iterator& next, Node &n)
	{
		insert(static_cast<Node &>(*next.cur_), n);

		return &n;
	}

	constexpr Node &remove(Node& n)
	{
		return static_cast<Node&>(static_cast<intrusive_node &>(n).remove());
	}

	constexpr iterator erase(const iterator& i)
	{
		auto next = i.cur_->next_;
		i.cur_->remove();
		return next;
	}

	constexpr void push_back(Node& n)
	{
		insert(end(), n);
	}

	constexpr void push_front(Node& n)
	{
		insert(begin(), n);
	}

	constexpr void pop_front()
	{
		erase(begin());
	}

	constexpr void pop_back()
	{
		erase(--end());
	}

	constexpr Node& back()
	{
		return static_cast<Node&>(*head_.prev_);
	}

	constexpr Node& front()
	{
		return static_cast<Node&>(*head_.next_);
	}

	constexpr void splice(iterator const i, intrusive_list &rhs)
	{
		auto first = rhs.head_.next_;
		auto last = rhs.head_.prev_;

		if (first == last)
			return;

		rhs.clear();

		first->prev_ = i.cur_->prev_;
		i.cur_->prev_->next_ = first;

		last->next_ = i.cur_;
		i.cur_->prev_ = last;
	}

	constexpr void clear()
	{
		head_.reset();
	}

	constexpr bool empty() const
	{
		return !head_;
	}

private:
	intrusive_node head_;
};

}

#endif // FZ_INTRUSIVE_LIST_HPP
