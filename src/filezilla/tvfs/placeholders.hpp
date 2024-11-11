#ifndef FZ_TVFS_PLACEHOLDERS_map_HPP
#define FZ_TVFS_PLACEHOLDERS_map_HPP

#include <vector>
#include <utility>

#include <libfilezilla/string.hpp>

namespace fz::tvfs::placeholders {

struct map: std::vector<std::pair<fz::native_string /*the placeholder*/, fz::native_string /* the value of the placeholder */>>
{
	using vector::vector;
};

extern const native_string home_dir;
extern const native_string user_name;

native_string anything(const native_string &placeholder);
native_string only_at_beginning(const native_string &placeholder);
native_string make_invalid_value(native_string_view explanation);
native_string_view next_invalid_value_explanation(native_string_view s, std::size_t &pos);
native_string substitute_placeholders(native_string_view path, const placeholders::map &map);
native_string convert_old_style_to_new(native_string_view path);

}

#endif // FZ_TVFS_PLACEHOLDERS_map_HPP
