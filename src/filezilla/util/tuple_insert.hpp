#ifndef FZ_UTIL_TUPLE_INSERT_HPP
#define FZ_UTIL_TUPLE_INSERT_HPP

#include <tuple>
#include <limits>

#include "../util/traits.hpp"

namespace fz::util
{
	inline constexpr std::size_t last_position = std::numeric_limits<std::size_t>::max();

	namespace detail
	{
		template <std::size_t Position = std::numeric_limits<std::size_t>::max(), typename T, typename Tuple, std::size_t... LIs, std::size_t... RIs>
		constexpr auto tuple_insert_impl(T && t, Tuple && args, std::index_sequence<LIs...>, std::index_sequence<RIs...>)
		{
			if constexpr(is_like_tuple<std::decay_t<Tuple>>::value) {
				return typename is_like_tuple<std::decay_t<Tuple>>::template
					tuple<std::tuple_element_t<LIs, std::decay_t<Tuple>>..., decltype(std::forward<T>(t)), std::tuple_element_t<Position+RIs, std::decay_t<Tuple>>...> {
						std::get<LIs>(std::forward<Tuple>(args))...,
						std::forward<T>(t),
						std::get<Position+RIs>(std::forward<Tuple>(args))...
					};
			}
			else
			if constexpr(is_like_array<std::decay_t<Tuple>>::value) {
				return typename is_like_array<std::decay_t<Tuple>>::template
					array<std::tuple_element_t<0, std::decay_t<Tuple>>, std::tuple_size_v<std::decay_t<Tuple>>+1> {
						std::get<LIs>(std::forward<Tuple>(args))...,
						std::forward<T>(t),
						std::get<Position+RIs>(std::forward<Tuple>(args))...
					};
			}
		}
	}

	template <std::size_t Position = last_position, typename T, typename Tuple>
	constexpr auto tuple_insert(T && t, Tuple && args)
	{
		constexpr auto pos = (Position == last_position ? std::tuple_size_v<std::decay_t<Tuple>> : Position);

		static_assert(pos <= std::tuple_size_v<std::decay_t<Tuple>>,  "Position must be either last_position or less than or equal to tuple_size<Tuple>");

		constexpr auto size_left  = pos;
		constexpr auto size_right = std::tuple_size_v<std::decay_t<Tuple>> - pos;

		return detail::tuple_insert_impl<pos>(std::forward<T>(t), std::forward<Tuple>(args), std::make_index_sequence<size_left>(), std::make_index_sequence<size_right>());
	}

}

#endif // FZ_UTIL_TUPLE_INSERT_HPP
