#ifndef FZ_MPL_COUNT_HPP
#define FZ_MPL_COUNT_HPP

#include <type_traits>

namespace fz::mpl {

template <typename Seq, typename T>
struct count;

template <template <typename ...> typename Seq, typename... Ts, typename T>
struct count<Seq<Ts...>, T>: std::integral_constant<std::size_t, (std::is_same_v<Ts, T> + ...)> {};

template <typename Seq, typename T>
using count_t = typename count<Seq, T>::type;

template <typename Seq, typename T>
inline constexpr auto count_v = count<Seq, T>::value;

}

#endif // FZ_MPL_COUNT_HPP

