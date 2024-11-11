#ifndef FZ_MPL_LAMBDA_HPP
#define FZ_MPL_LAMBDA_HPP

#include "placeholders.hpp"

namespace fz::mpl {

template <typename F>
struct lambda {
	template <typename... Ts>
	struct apply: placeholders::apply<F, apply<Ts...>> {};
};

}

#endif // FZ_MPL_LAMBDA_HPP
