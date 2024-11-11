#ifndef FZ_MPL_CONTAINS_HPP
#define FZ_MPL_CONTAINS_HPP

#include <type_traits>

namespace fz::mpl {

template <typename Seq, typename T>
struct contains;

template <template <typename ...> typename Seq, typename... Ts, typename T>
struct contains<Seq<Ts...>, T>: std::disjunction<std::is_same<Ts, T>...> {};

template <typename Seq, typename T>
inline constexpr auto contains_v = contains<Seq, T>::value;

}

#endif // FZ_MPL_CONTAINS_HPP
