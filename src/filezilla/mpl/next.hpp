#ifndef FZ_MPL_NEXT_HPP
#define FZ_MPL_NEXT_HPP

#include <type_traits>

namespace fz::mpl {

template <typename T>
struct next;

template <typename T, T v>
struct next<std::integral_constant<T, v>>: std::integral_constant<T, v+1>{};

}

#endif // FZ_MPL_NEXT_HPP
