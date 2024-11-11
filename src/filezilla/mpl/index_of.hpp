#ifndef FZ_MPL_INDEX_OF_HPP
#define FZ_MPL_INDEX_OF_HPP

#include <variant>

#include "size_t.hpp"
#include "identity.hpp"

namespace fz::mpl {

template <typename Seq, typename T>
struct index_of;

template <template <typename ...> typename Seq, typename... Us, typename T>
struct index_of<Seq<Us...>, T>
	: size_t_<std::variant<identity<Us>...>(identity<T>()).index()>
{ };

template <typename Seq, typename T>
inline constexpr auto index_of_v = index_of<Seq, T>::value;

}

#endif // FZ_MPL_INDEX_OF_HPP
