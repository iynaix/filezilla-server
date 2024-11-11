#ifndef FZ_UTIL_TUPLE_SLICE_HPP
#define FZ_UTIL_TUPLE_SLICE_HPP

#include <tuple>
#include <algorithm>

#include "../util/traits.hpp"

namespace fz::util
{

	namespace detail
	{
		template <std::size_t Start, std::size_t... Is, typename Tuple>
		constexpr auto slice_impl(Tuple && args, std::index_sequence<Is...>)
		{
			if constexpr(is_like_tuple<std::decay_t<Tuple>>::value) {
				return typename is_like_tuple<std::decay_t<Tuple>>::template
					tuple<std::tuple_element_t<Start+Is, std::decay_t<Tuple>>...>{
						std::get<Start+Is>(std::forward<Tuple>(args))...
					};
			}
			else
			if constexpr(is_like_array<std::decay_t<Tuple>>::value) {
				return typename is_like_array<std::decay_t<Tuple>>::template
					array<std::tuple_element_t<Start, std::decay_t<Tuple>>, sizeof...(Is)>{
						std::get<Start+Is>(std::forward<Tuple>(args))...
					};
			}
		}
	}

	template <std::size_t Start, std::size_t End = std::numeric_limits<std::size_t>::max(), typename Tuple>
	constexpr auto tuple_slice(Tuple && args)
	{
		constexpr auto start = (Start == std::numeric_limits<std::size_t>::max() ? std::tuple_size_v<std::decay_t<Tuple>> : Start);
		constexpr auto end = (End == std::numeric_limits<std::size_t>::max() ? std::tuple_size_v<std::decay_t<Tuple>> : End);

		static_assert(start <= end, "Start position must be <= End position");

		return detail::slice_impl<Start>(std::forward<Tuple>(args), std::make_index_sequence<end-start>());
	}


}
#endif // FZ_UTIL_TUPLE_SLICE_HPP
