#ifndef FZ_MPL_IDENTITY_HPP
#define FZ_MPL_IDENTITY_HPP

namespace fz::mpl {

template <typename T>
struct identity {
	using type = T;
};

}
#endif // FZ_MPL_IDENTITY_HPP
