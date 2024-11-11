#ifndef FZ_SERIALIZATION_TYPES_LOCAL_FILESYS_HPP
#define FZ_SERIALIZATION_TYPES_LOCAL_FILESYS_HPP

#include <libfilezilla/local_filesys.hpp>

#include "../helpers.hpp"

namespace fz::serialization {

template <typename Archive>
void serialize(Archive &ar, result &res) {
	ar(nvp(res.error_, "error"), nvp(res.raw_, "raw"));
}

}

#endif // FZ_SERIALIZATION_TYPES_LOCAL_FILESYS_HPP
