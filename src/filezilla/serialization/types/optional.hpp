#ifndef FZ_SERIALIZATION_TYPES_OPTIONAL_HPP
#define FZ_SERIALIZATION_TYPES_OPTIONAL_HPP

#include <optional>
#include "../../serialization/serialization.hpp"

namespace fz::serialization {

	template <typename Archive, typename T>
	void save(Archive &ar, const std::optional<T> &o)
	{
		const bool has_value = o.has_value();

		if (ar(FZ_NVP(has_value)); has_value && ar)
			ar(nvp(*o, "value"));
	}

	template <typename Archive, typename T>
	void load(Archive &ar, std::optional<T> &o)
	{
		if (bool has_value; ar(FZ_NVP(has_value))) {
			if (has_value) {
				if (T value; ar(FZ_NVP(value)))
					o = std::move(value);
			}
			else
				o = std::nullopt;
		}
	}

}
#endif // FZ_SERIALIZATION_TYPES_OPTIONAL_HPP
