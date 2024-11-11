#ifndef FZ_RECEIVER_DETAIL_HPP
#define FZ_RECEIVER_DETAIL_HPP

#include <stack>
#include <tuple>

namespace fz::receiver_detail {

template <std::size_t Size, std::size_t Align>
struct blob_stack
{
	static void *push()
	{
		return get_stack().emplace().data;
	}

	static void *top()
	{
		return get_stack().top().data;
	}

	static void pop()
	{
		get_stack().pop();
	}

private:
	struct blob
	{
		alignas(Align) char data[Size];
	};

	// This works around an issue with MingW that results in multiple definitions of the TLS initialization for a stack<blob> static member variable
	static std::stack<blob> &get_stack()
	{
		static thread_local std::stack<blob> stack;
		return stack;
	}
};

template <typename F, typename T>
struct is_invocable_with_tuple;

template <typename F, typename... Args>
struct is_invocable_with_tuple<F, std::tuple<Args...>>: std::is_invocable<F, std::add_lvalue_reference_t<Args>...>
{};

template <typename... Ts>
struct callable_with_args
{
	struct cannot_receive_into_these_variables{};
	using type = cannot_receive_into_these_variables;
};

struct ignore_t
{
	constexpr ignore_t() {}

	template <typename T>
	constexpr ignore_t(T &&) {}

	template <typename T>
	constexpr ignore_t &operator=(T &&) { return *this; }
};

template <typename T>
using transform_ignore = std::conditional_t<
	std::is_same_v<std::decay_t<decltype(std::ignore)>, std::decay_t<T>>,
	ignore_t,
	T
>;

template <typename... Ts>
struct callable_with_args<Ts &...>
{
	struct type
	{
		type(Ts &... args)
			: v_{args...}
		{}

		void operator()(Ts &... args)
		{
			v_ = std::forward_as_tuple(std::move(args)...);
		}

		std::tuple<Ts &...> v_;
	};
};

template <typename... Ts>
struct callable_with_args<std::tuple<Ts &...>>
{
	struct type
	{
		type(std::tuple<Ts &...> && v)
			: v_(std::move(v))
		{}

		void operator()(transform_ignore<Ts&>... args)
		{
			v_ = std::forward_as_tuple(std::move(args)...);
		}

		std::tuple<transform_ignore<Ts &>...> v_;
	};
};

template <typename... Ts>
struct callable_with_args<std::pair<Ts...>&>
{
	struct type
	{
		type(std::pair<Ts...> & v)
			: v_(v)
		{}

		void operator()(transform_ignore<Ts>... args)
		{
			v_ = std::pair<transform_ignore<Ts&&>...>(std::move(args)...);
		}

		std::pair<transform_ignore<Ts>...> &v_;
	};
};

template <typename... Ts>
struct callable_with_args<std::tuple<Ts...>&>
{
	struct type
	{
		type(std::tuple<Ts...> & v)
			: v_(v)
		{}

		void operator()(transform_ignore<Ts>... args)
		{
			v_ = std::tuple<transform_ignore<Ts&&>...>(std::move(args)...);
		}

		std::tuple<transform_ignore<Ts>...> &v_;
	};
};

template <typename F, typename T>
inline constexpr bool is_invocable_with_tuple_v = is_invocable_with_tuple<F, T>::value;

}

#endif // FZ_RECEIVER_DETAIL_HPP
