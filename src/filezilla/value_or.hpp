#ifndef FZ_VALUE_OR_HPP
#define FZ_VALUE_OR_HPP

#include <utility>

namespace fz {

template <typename T, typename U>
constexpr T value_or(T && t, U && u)
{
	return bool(t) ? std::forward<T>(t) : static_cast<T>(std::forward<U>(u));
}

template <typename T, typename... Args>
constexpr T value_or(T && t, std::in_place_t, Args &&... args)
{
	return bool(t) ? std::forward<T>(t) : T(std::forward<Args>(args)...);
}

template <typename T, typename U, typename... Args>
constexpr T value_or(T && t, std::in_place_type_t<U>, Args &&... args)
{
	return bool(t) ? std::forward<T>(t): static_cast<T>(U(std::forward<Args>(args)...));
}

}

#endif // FZ_VALUE_OR_HPP
