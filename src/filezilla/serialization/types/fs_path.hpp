#ifndef FZ_SERIALIZATION_TYPES_FS_PATH_HPP
#define FZ_SERIALIZATION_TYPES_FS_PATH_HPP

#include "../../util/filesystem.hpp"

namespace fz::util::fs {

template <typename Archive, typename Char, path_format Format, path_kind Kind>
void load_minimal(const Archive &, basic_path<Char, Format, Kind> &bp, const std::basic_string<Char> &s)
{
	bp = s;
}

template <typename Archive, typename Char, path_format Format, path_kind Kind>
std::basic_string<Char> save_minimal(const Archive &, const basic_path<Char, Format, Kind> &bp)
{
	return bp;
}

}

#endif // FZ_SERIALIZATION_TYPES_FS_PATH_HPP
