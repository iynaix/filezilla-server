#ifndef FZ_SERIALIZATION_VERSION_HPP
#define FZ_SERIALIZATION_VERSION_HPP

#include <type_traits>
#include <cstdint>

namespace fz::serialization {

enum version_t: std::uint32_t {};

template <typename T, typename SFINAEHelper = void>
struct version_of
{
	using type = version_t;
	static inline const type value { 0 };
};

template <typename T>
struct version_of<T, std::enable_if_t<!std::is_same_v<T, std::decay_t<T>>>>: version_of<std::decay_t<T>>
{};

template <typename T>
using version_of_t = typename version_of<T>::type;

template <typename T>
inline const auto version_of_v = version_of<T>::value;

#define FZ_SERIALIZATION_SET_VERSION_T(Type, T, ...)                                   \
	namespace fz::serialization {                                                      \
		template <> struct version_of<Type>{ static constexpr T value { __VA_ARGS__ }; \
	}                                                                                  \
/***/

#define FZ_SERIALIZATION_SET_VERSION(Type, Version)          \
	FZ_SERIALIZATION_SET_VERSION_T(Type, version_t, Version) \
/***/

}

#endif // FZ_SERIALIZATION_VERSION_HPP
