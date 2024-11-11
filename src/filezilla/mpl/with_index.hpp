#ifndef FZ_MPL_WITH_INDEX_HPP
#define FZ_MPL_WITH_INDEX_HPP

#include <utility>

#include "size_t.hpp"

namespace fz::mpl {

namespace detail {

	template <typename F, std::size_t I, std::size_t... Is>
	auto with_index(std::size_t index, F && f, const std::index_sequence<I, Is...>&)
	{
		using return_type = decltype(f(size_t_<0>()));

		if constexpr (std::is_same_v<void, return_type>) {
			if (index == I)
				f(size_t_<I>());
			else
				static_cast<void>(((index == Is && ((f(size_t_<Is>())), true)) || ...));

		}
		else {
			return_type ret{};

			if (index == I)
				ret = f(size_t_<I>());
			else
				static_cast<void>(((index == Is && ((ret = f(size_t_<Is>())), true)) || ...));

			return ret;
		}
	}

}

template <std::size_t size, typename F>
auto with_index(std::size_t index, F && f)
{
	if constexpr (size > 0)
		return detail::with_index(index, std::forward<F>(f), std::make_index_sequence<size>());
}

}

#endif // FZ_MPL_WITH_INDEX_HPP
