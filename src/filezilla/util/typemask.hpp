#ifndef FZ_UTIL_TYPEMASK_HPP
#define FZ_UTIL_TYPEMASK_HPP

#include <bitset>
#include "../mpl/count.hpp"
#include "../mpl/contains.hpp"

#include "traits.hpp"

namespace fz::util {

template <typename Seq>
struct typemask;

template <template <typename...> typename Seq, typename... Ts>
struct typemask<Seq<Ts...>>
{
	static_assert(((mpl::count_v<Seq<Ts...>, Ts> == 1) && ...), "Types must be unique");

	template <typename T>
	std::enable_if_t<mpl::contains_v<Seq<Ts...>, T>,
	bool> constexpr test() noexcept
	{
		return bits_[type_index_v<T, Seq<Ts...>>];
	}

	template <typename T>
	bool constexpr test(bool def) noexcept
	{
		if constexpr (mpl::contains_v<Seq<Ts...>, T>)
			return bits_[type_index_v<T, Seq<Ts...>>];
		else
			return def;
	}

	bool constexpr test(std::size_t idx) noexcept
	{
		return idx < sizeof...(Ts) && bits_[idx];
	}

	template <typename... Us>
	std::enable_if_t<(mpl::contains_v<Seq<Ts...>, Us> && ...)>
	constexpr set(bool v = true) noexcept
	{
		if constexpr (sizeof...(Us) == 0) {
			if (v == true)
				bits_.set();
			else
				bits_.reset();
		}
		else
			((bits_[type_index_v<Us, Seq<Ts...>>] = v), ...);
	}

	template <typename... Us>
	std::enable_if_t<(mpl::contains_v<Seq<Ts...>, Us> && ...)>
	constexpr reset() noexcept
	{
		set<Us...>(false);
	}

private:
	std::bitset<sizeof...(Ts)> bits_;
};

}
#endif // FZ_UTIL_TYPEMASK_HPP
