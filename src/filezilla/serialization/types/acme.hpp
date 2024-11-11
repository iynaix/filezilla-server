#ifndef FZ_SERIALIZATION_TYPES_ACME_HPP
#define FZ_SERIALIZATION_TYPES_ACME_HPP

#include "../trait.hpp"
#include "../../acme/daemon.hpp"

namespace fz::acme {

template <typename Archive>
void serialize(Archive &ar, serve_challenges::internally &i)
{
	using namespace fz::serialization;

	ar(nvp(i.addresses_info, "", "listener"));
}

template <typename Archive>
void serialize(Archive &ar, serve_challenges::externally &e)
{
	using namespace fz::serialization;

	ar.attribute(e.well_known_path, "well_known_path")
	  .attribute(e.create_if_not_existing, "create_if_not_existing");
}

template <typename Archive>
void serialize(Archive &ar, extra_account_info &e)
{
	using namespace fz::serialization;

	ar(
		nvp(e.directory, "directory"),
		nvp(e.contacts, "contacts", "contact"),
		nvp(e.created_at, "created_at"),
		nvp(e.jwk, "jwk")
	);
}

}

#endif // FZ_SERIALIZATION_TYPES_ACME_HPP
