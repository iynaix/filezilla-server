#ifndef FZ_MPL_RENAME_HPP
#define FZ_MPL_RENAME_HPP

namespace fz::mpl {

template <typename From, template <typename ...> typename To>
struct rename;

template <template <typename ...> typename From, template <typename ...> typename To, typename... Ts>
struct rename<From<Ts...>, To> {
	using type = To<Ts...>;
};

template <typename From, template <typename ...> typename To>
using rename_t = typename rename<From, To>::type;

}

#endif // FZ_MPL_RENAME_HPP
