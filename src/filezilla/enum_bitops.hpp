#ifndef FZ_ENUM_BITOPS_HPP
#define FZ_ENUM_BITOPS_HPP

#include <type_traits>
#include "preprocessor/cat.hpp"

namespace fz::enum_bitops {

template <typename E, std::enable_if_t<std::is_enum_v<E>>* = nullptr>
struct convertible_to_bool
{
	constexpr explicit operator bool() const
	{
		return bool(std::underlying_type_t<E>(e_));
	}

	constexpr operator E() const
	{
		return e_;
	}

	E e_;
};

}

#define FZ_ENUM_BITOPS_DEFINE_FOR(E) FZ_ENUM_BITOPS_DEFINE_( , E)
#define FZ_ENUM_BITOPS_FRIEND_DEFINE_FOR(E) FZ_ENUM_BITOPS_DEFINE_(friend, E)

#define FZ_ENUM_BITOPS_DEFINE_(F, E)    \
	FZ_ENUM_BITOPS_DEFINE_UOP_(F, E, ~) \
	FZ_ENUM_BITOPS_DEFINE_BOP_(F, E, &) \
	FZ_ENUM_BITOPS_DEFINE_BOP_(F, E, |) \
	FZ_ENUM_BITOPS_DEFINE_BOP_(F, E, ^) \
/***/

#define FZ_ENUM_BITOPS_DEFINE_UOP_(F, E, OP)                                      \
	F inline constexpr fz::enum_bitops::convertible_to_bool<E> operator OP(E a) { \
		return {static_cast<E>(OP static_cast<std::underlying_type_t<E>>(a))} ;   \
	}                                                                             \
/***/

#define FZ_ENUM_BITOPS_DEFINE_BOP_(F, E, OP)                                                                             \
	F inline constexpr fz::enum_bitops::convertible_to_bool<E> operator OP(E a, E b) {                                   \
		return {static_cast<E>(static_cast<std::underlying_type_t<E>>(a) OP static_cast<std::underlying_type_t<E>>(b))}; \
	}                                                                                                                    \
	F inline constexpr E& operator FZ_PP_CAT(OP,=)(E& a, E b) {                                                          \
		return a = a OP b;                                                                                               \
	}                                                                                                                    \
/***/

#endif // FZ_ENUM_BITOPS_HPP

