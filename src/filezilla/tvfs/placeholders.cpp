#include "placeholders.hpp"

#include "../string.hpp"

namespace fz::tvfs::placeholders
{

const native_string home_dir = fzT("<home>");
const native_string user_name = fzT("<user>");

static constexpr native_string::value_type invalid_tag_array[] = { '\0', '\0', '\0', '\0', ':' };
static constexpr native_string_view invalid_tag{invalid_tag_array, std::size(invalid_tag_array)};

native_string anything(const native_string &placeholder)
{
	if (placeholder.size() > 1)
		return anything_between<native_string>(fzT("<")[0], fzT(">")[0], placeholder[0], placeholder.substr(1));

	return {};
}

native_string only_at_beginning(const native_string &placeholder)
{
	return fz::only_at_beginning(placeholder);
}

native_string make_invalid_value(native_string_view explanation)
{
	native_string res{invalid_tag};
	return res.append(remove_ctrl_chars(native_string(explanation))).append(1, native_string::value_type('\0'));
}

native_string_view next_invalid_value_explanation(native_string_view s, std::size_t &pos)
{
	native_string_view res;

	pos = s.find(invalid_tag, pos);
	if (pos != s.npos) {
		size_t start = pos + invalid_tag.length();
		size_t end = s.find(native_string_view::value_type('\0'), start);

		res = s.substr(start, end-start);
		pos = end+1;
	}

	return res;
}

native_string substitute_placeholders(native_string_view path, const placeholders::map &map)
{
	return unescaped(path, fzT("%"), map);
}

native_string convert_old_style_to_new(native_string_view path)
{
	// Convert the old placeholders to the new style.
	return fz::unescaped(fz::escaped(path, fzT("%")), fzT(":"), {
		{ fzT("h"), fzT("%<home>") },
		{ fzT("u"), fzT("%<user>") },
	});
}


}
