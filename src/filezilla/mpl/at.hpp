#ifndef FZ_MPL_AT_HPP
#define FZ_MPL_AT_HPP

#include <tuple>

namespace fz::mpl {

template <typename Seq, typename Idx>
struct at;

template <template <typename...> typename Seq, typename... Ts, typename Idx>
struct at<Seq<Ts...>, Idx>: std::tuple_element<Idx::value, std::tuple<Ts...>> {};

template <typename Seq, std::size_t Idx>
using at_c = at<Seq, std::integral_constant<std::size_t, Idx>>;

template <typename Seq, typename Idx>
using at_t = typename at<Seq, Idx>::type;

template <typename Seq, std::size_t Idx>
using at_c_t = typename at_c<Seq, Idx>::type;

}
#endif // FZ_MPL_AT_HPP
