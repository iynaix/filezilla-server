#ifndef FZ_SERIALIZATION_TYPES_TUPLE_HPP
#define FZ_SERIALIZATION_TYPES_TUPLE_HPP

#include <tuple>

namespace fz::serialization {

	template <typename Archive, typename ...Ts>
	void serialize(Archive &ar, std::tuple<Ts...> &t) {
		std::apply(ar, t);
	}

	template <typename Archive, typename ...Ts>
	void serialize(Archive &ar, std::pair<Ts...> &t) {
		std::apply(ar, t);
	}

 }

#endif // FZ_SERIALIZATION_TYPES_TUPLE_HPP
