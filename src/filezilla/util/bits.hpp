#ifndef FZ_UTIL_BITS_HPP
#define FZ_UTIL_BITS_HPP

#include <type_traits>
#include <climits>
#include <limits>

#include "traits.hpp"

namespace fz::util {

template <typename T, std::enable_if_t<(std::is_integral_v<T> || std::is_enum_v<T>) && sizeof(T) <= sizeof(long long)>* = nullptr>
constexpr std::size_t count_trailing_zeros(T v) {
	static_assert(std::is_unsigned_v<underlying_t<T>>, "T must be of unsigned type");

	if (v == 0)
		return sizeof(T) * CHAR_BIT;

	if constexpr (sizeof(T) <= sizeof(int))
		return static_cast<std::size_t>(__builtin_ctz(static_cast<unsigned int>(v)));
	else
	if constexpr (sizeof(T) <= sizeof(long))
		return static_cast<std::size_t>(__builtin_ctzl(static_cast<unsigned long>(v)));
	else
	if constexpr (sizeof(T) <= sizeof(long long))
		return static_cast<std::size_t>(__builtin_ctzll(static_cast<unsigned long long>(v)));
}

template <typename T, std::enable_if_t<(std::is_integral_v<T> || std::is_enum_v<T>)  && sizeof(T) <= sizeof(long long)>* = nullptr>
constexpr std::size_t count_trailing_ones(T v) {
	static_assert(std::is_unsigned_v<underlying_t<T>>, "T must be of unsigned type");

	return count_trailing_zeros(T(~v));
}

template <typename T, std::enable_if_t<(std::is_integral_v<T> || std::is_enum_v<T>)  && sizeof(T) <= sizeof(long long) && !std::is_signed_v<T>>* = nullptr>
constexpr std::size_t count_leading_zeros(T v) {
	static_assert(std::is_unsigned_v<underlying_t<T>>, "T must be of unsigned type");

	if (v == 0)
		return sizeof(T) * CHAR_BIT;

	if constexpr (sizeof(T) <= sizeof(int))
		return static_cast<std::size_t>(__builtin_clz(static_cast<unsigned int>(v))) - ((sizeof(int)-sizeof(T))*CHAR_BIT);
	else
	if constexpr (sizeof(T) <= sizeof(long))
		return static_cast<std::size_t>(__builtin_clzl(static_cast<unsigned long>(v))) - ((sizeof(long)-sizeof(T))*CHAR_BIT);
	else
	if constexpr (sizeof(T) <= sizeof(long long))
		return static_cast<std::size_t>(__builtin_clzll(static_cast<unsigned long long>(v))) - ((sizeof(long long)-sizeof(T))*CHAR_BIT);
}

template <typename T, std::enable_if_t<(std::is_integral_v<T> || std::is_enum_v<T>)  && sizeof(T) <= sizeof(long long)>* = nullptr>
constexpr std::size_t count_leading_ones(T v) {
	static_assert(std::is_unsigned_v<underlying_t<T>>, "T must be of unsigned type");

	return count_leading_zeros(T(~v));
}


template <typename T, std::enable_if_t<(std::is_integral_v<T> || std::is_enum_v<T>)  && sizeof(T) <= sizeof(long long)>* = nullptr>
constexpr std::size_t log2_floor(T i) {
	static_assert(std::is_unsigned_v<underlying_t<T>>, "T must be of unsigned type");

	return std::size_t(sizeof(i) * CHAR_BIT - count_leading_zeros(i)) - 1;
}

template <typename T, std::enable_if_t<(std::is_integral_v<T> || std::is_enum_v<T>)>* = nullptr>
constexpr T exp2_saturated(std::size_t n) {
	if (n >= std::numeric_limits<underlying_t<T>>::digits) {
		return std::numeric_limits<underlying_t<T>>::max();
	}

	return static_cast<T>(static_cast<underlying_t<T>>(1) << n);
}

}

#endif // FZ_UTIL_BITS_HPP
