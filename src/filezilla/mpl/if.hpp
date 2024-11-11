#ifndef FZ_MPL_IF_HPP
#define FZ_MPL_IF_HPP

#include <type_traits>

namespace fz::mpl {

template <typename Const, typename Then, typename Else>
using if_ = std::conditional_t<Const::value, Then, Else>;

template <bool Const, typename Then, typename Else>
using if_c = std::conditional_t<Const, Then, Else>;

}

#endif // FZ_MPL_IF_HPP
