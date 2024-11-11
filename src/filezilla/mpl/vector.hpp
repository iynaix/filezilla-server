#ifndef FZ_MPL_VECTOR_HPP
#define FZ_MPL_VECTOR_HPP

namespace fz::mpl {

	template <typename... Ts>
	struct vector {
		using type = vector<Ts...>;
	};

}

#endif // FZ_MPL_VECTOR_HPP
