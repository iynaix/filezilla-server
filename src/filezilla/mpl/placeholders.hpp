#ifndef FZ_MPL_PLACEHOLDERS_HPP
#define FZ_MPL_PLACEHOLDERS_HPP

#include <type_traits>

#include "at.hpp"
#include "arity.hpp"

namespace fz::mpl {

namespace placeholders {

	template <std::size_t I>
	struct arg: std::integral_constant<std::size_t, I-1>{};

	using _1 = arg<1>;
	using _2 = arg<2>;
	using _3 = arg<3>;
	using _4 = arg<4>;
	using _5 = arg<5>;
	using _6 = arg<6>;
	using _7 = arg<7>;
	using _8 = arg<8>;

	template <typename Seq, typename Pack>
	struct apply;

	template <template <typename...> typename Seq, typename... Ts, typename Pack>
	struct apply<Seq<Ts...>, Pack> {
		template <typename T, typename = void>
		struct subst;

		template <typename T>
		struct subst<T>{
			using type = T;
		};

		template <std::size_t I>
		struct subst<arg<I>> {
			using type = typename at<Pack, arg<I>>::type;
		};

		template <template <typename...> typename G, typename... Us>
		struct subst<G<Us...>> {
			using type = typename apply<G<Us...>, Pack>::type;
		};

		template <typename Indices = std::make_index_sequence<sizeof...(Ts)>, typename = void>
		struct do_substitution;

		template <std::size_t... Is>
		struct do_substitution<std::index_sequence<Is...>> {
			template <std::size_t I>
			using op = typename subst<typename at_c<Seq<Ts...>, I>::type>::type;

			using type = Seq<op<Is>...>;
		};

		using type = typename do_substitution<>::type;
	};

}

using placeholders::_1;
using placeholders::_2;
using placeholders::_3;
using placeholders::_4;
using placeholders::_5;
using placeholders::_6;
using placeholders::_7;
using placeholders::_8;

}
#endif // FZ_MPL_PLACEHOLDERS_HPP
