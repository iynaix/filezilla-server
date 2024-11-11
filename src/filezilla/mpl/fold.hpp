#ifndef FZ_MPL_FOLD_HPP
#define FZ_MPL_FOLD_HPP

#include "vector.hpp"

namespace fz::mpl {

template <typename Seq, typename State, template <typename...> typename Op>
struct fold;

template <template<typename...> typename Seq, typename State, template <typename...> typename Op>
struct fold<Seq<>, State, Op> {
	using type = State;
};

template <template<typename...> typename Seq, typename Head, typename ...Tail, typename State, template <typename...> typename Op>
struct fold<Seq<Head, Tail...>, State, Op> {
	using next_state = typename Op<State, Head>::type;
	using type = typename fold<vector<Tail...>, next_state, Op>::type;
};

template <typename Seq, typename State, template <typename...> typename Op>
using fold_t = typename fold<Seq, State, Op>::type;

}

#endif // FZ_MPL_FOLD_HPP
