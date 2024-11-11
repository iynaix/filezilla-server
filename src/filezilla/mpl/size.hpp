#ifndef FZ_MPL_SIZE_HPP
#define FZ_MPL_SIZE_HPP

#include "size_t.hpp"

namespace fz::mpl {

	template <typename Seq>
	struct size;

	template <template <typename...> typename Seq, typename... Ts>
	struct size<Seq<Ts...>>: size_t_<sizeof...(Ts)>{};

}

#endif // FZ_MPL_SIZE_HPP
