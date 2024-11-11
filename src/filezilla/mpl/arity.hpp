#ifndef FZ_MPL_ARITY_HPP
#define FZ_MPL_ARITY_HPP

#include <type_traits>
#include <utility>

namespace fz::mpl {

template <typename F>
struct arity;

template <template <typename...> typename F, typename... Ts>
struct arity<F<Ts...>>: std::integral_constant<std::size_t, sizeof...(Ts)> {
	using indices = std::make_index_sequence<sizeof...(Ts)>;
};

}
#endif // FZ_MPL_ARITY_HPP
