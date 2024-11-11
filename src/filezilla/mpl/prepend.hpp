#ifndef FZ_MPL_PREPEND_HPP
#define FZ_MPL_PREPEND_HPP

namespace fz::mpl {

template <typename Seq, typename... Ts>
struct prepend;

template <template <typename ...> typename Seq, typename... Us, typename... Ts>
struct prepend<Seq<Us...>, Ts...> {
	using type = Seq<Ts..., Us...>;
};

template <typename Seq, typename... Ts>
using prepend_t = typename prepend<Seq, Ts...>::type;

}

#endif // FZ_MPL_PREPEND_HPP
