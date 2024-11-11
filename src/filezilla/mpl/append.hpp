#ifndef FZ_MPL_APPEND_HPP
#define FZ_MPL_APPEND_HPP

namespace fz::mpl {

template <typename Seq, typename... Ts>
struct append;

template <template <typename ...> typename Seq, typename... Us, typename... Ts>
struct append<Seq<Us...>, Ts...> {
	using type = Seq<Us..., Ts...>;
};

template <typename Seq, typename... Ts>
using append_t = typename append<Seq, Ts...>::type;

}

#endif // FZ_MPL_APPEND_HPP
