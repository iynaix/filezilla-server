#ifndef FZ_SERIALIZATION_TYPES_NETWORK_INTERFACE_HPP
#define FZ_SERIALIZATION_TYPES_NETWORK_INTERFACE_HPP

#include <libfilezilla/iputils.hpp>

#include "../helpers.hpp"

namespace fz::serialization
{

template <typename Archive>
void serialize(Archive &ar, network_interface &ni)
{
	ar(
		nvp(ni.name, "name"),
		nvp(ni.addresses, "addresses"),
		nvp(ni.mac, "mac")
	);
}

}

#endif // FZ_SERIALIZATION_TYPES_NETWORK_INTERFACE_HPP
