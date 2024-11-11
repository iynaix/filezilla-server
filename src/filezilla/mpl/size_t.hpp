#ifndef FZ_MPL_SIZE_T_HPP
#define FZ_MPL_SIZE_T_HPP

#include <type_traits>
#include <cstdint>

namespace fz::mpl {

	template <std::size_t S>
	using size_t_ = std::integral_constant<std::size_t, S>;

}

#endif // FZ_MPL_SIZE_T_HPP
