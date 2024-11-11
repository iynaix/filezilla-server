#ifndef FZ_SERIALIZATION_TYPES_JSON_HPP
#define FZ_SERIALIZATION_TYPES_JSON_HPP

#include <libfilezilla/json.hpp>

namespace fz::serialization {

template <typename Archive>
void load_minimal(const Archive &, json &j, const std::string &s)
{
	j = json::parse(s);
}

template <typename Archive>
std::string save_minimal(const Archive &, const json &j)
{
	return j.to_string();
}



}

#endif // FZ_SERIALIZATION_TYPES_JSON_HPP
