#ifndef FZ_SERIALIZATION_TYPES_COMMON_HPP
#define FZ_SERIALIZATION_TYPES_COMMON_HPP

#include <cstdint>
#include <type_traits>
#include <utility>

#include "../../serialization/trait.hpp"

namespace fz::serialization
{
	//! Save enums
	template <typename Archive, typename E, std::enable_if_t<std::is_enum_v<E>>* = nullptr>
	auto save_minimal(const Archive &, const E &e) {
		return std::underlying_type_t<E>(e);
	}

	//! Load enums
	template <typename Archive, typename E, std::enable_if_t<std::is_enum_v<E>>* = nullptr>
	void load_minimal(const Archive &, E &e, const std::underlying_type_t<E> &v) {
		e = static_cast<E>(v);
	}

	//! Save types aliased to enums
	template <typename Archive, typename T, std::enable_if_t<std::is_enum_v<typename T::serialization_alias> && std::is_convertible_v<typename T::serialization_alias, T>>* = nullptr>
	auto save_minimal(const Archive &, const T &t) {
		return std::underlying_type_t<typename T::serialization_alias>(static_cast<typename T::serialization_alias>(t));
	}

	//! Load types aliased to enums
	template <typename Archive, typename T, std::enable_if_t<std::is_enum_v<typename T::serialization_alias> && std::is_convertible_v<T, typename T::serialization_alias>>* = nullptr>
	void load_minimal(const Archive &, T &t, const std::underlying_type_t<typename T::serialization_alias> &v) {
		t = static_cast<T>(static_cast<typename T::serialization_alias>(v));
	}

}

#endif // FZ_SERIALIZATION_TYPES_COMMON_HPP
