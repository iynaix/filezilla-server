#ifndef FZ_MPL_COUNT_IF_HPP
#define FZ_MPL_COUNT_IF_HPP

#include <type_traits>

namespace fz::mpl {

template <typename Seq, template <typename> typename Pred>
struct count_if;

template <template <typename ...> typename Seq, typename... Ts, template <typename> typename Pred>
struct count_if<Seq<Ts...>, Pred>: std::integral_constant<std::size_t, (Pred<Ts>::value + ...)> {};

template <typename Seq, template <typename> typename Pred>
using count_if_t = typename count_if<Seq, Pred>::type;

template <typename Seq, template <typename> typename Pred>
inline constexpr auto count_if_v = count_if_t<Seq, Pred>::value;

}

#endif // FZ_MPL_COUNT_IF_HPP
