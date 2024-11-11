#ifndef FZ_STRING_HPP
#define FZ_STRING_HPP

#include <libfilezilla/string.hpp>

namespace fz {

namespace impl {

	inline constexpr native_string::value_type anything_between_tag_array[] = { '\0', '\0', '\0', '<', '>' };
	inline constexpr native_string_view anything_between_tag{anything_between_tag_array, std::size(anything_between_tag_array)};

	inline constexpr native_string::value_type only_at_beginning_tag_array[] = { '\0', '\0', '\0', '^', '!' };
	inline constexpr native_string_view only_at_beginning_tag{only_at_beginning_tag_array, std::size(only_at_beginning_tag_array)};

	template <typename String, typename View, typename EscapeMap>
	String escaped(View str, View escape_string, const EscapeMap &escape_map)
	{
		String ret(str);

		if (!escape_string.empty()) {
			fz::replace_substrings(ret, escape_string, String(escape_string).append(escape_string));
		}

		for (auto &p: escape_map) {
			fz::replace_substrings(ret, p.second, String(escape_string).append(p.first));
		}

		return ret;
	}

	template <typename String, typename View, typename EscapeMap>
	String unescaped(View str, View escape_string, const EscapeMap &escape_map = {})
	{
		String ret;

		while (true) begin_loop: {
			auto esc_pos = str.find(escape_string);
			ret.append(str.substr(0, esc_pos));

			if (esc_pos == View::npos)
				break;

			str.remove_prefix(esc_pos+1);

			for (auto &p: escape_map) {
				// Deal with a special type of placeholder: it matches anything between two characters.
				// The match is then substituted onto the string according to the info in the placeholder itself.
				// The format of the placeholder is: [anything_between_tag] [left char] [right char] [escape char] [other placeholder to be found in placeholder's value]

				if (auto placeholder = View(p.first); fz::starts_with(placeholder, anything_between_tag)) {
					placeholder.remove_prefix(anything_between_tag.size());
					if (placeholder.size() >= 4) {
						auto left = placeholder.substr(0, 1);
						auto right = placeholder.substr(1, 1);
						auto value_escape = placeholder.substr(2, 1);
						auto value_placeholder = placeholder.substr(3);

						if (fz::starts_with(str, left)) {
							auto right_pos = str.substr(1).find(right);
							if (right_pos != View::npos) {
								auto match = str.substr(1, right_pos);
								ret.append(unescaped<String>(View(p.second), value_escape, std::initializer_list<std::pair<View, View>>{std::pair{value_placeholder, match}}));
								str.remove_prefix(right_pos+1);
								goto begin_loop;
							}
						}
					}
				}

				// Deal with a special type of placeholder: it matches only at the beginning of the string
				if (auto placeholder = View(p.first); fz::starts_with(placeholder, only_at_beginning_tag)) {
					placeholder.remove_prefix(only_at_beginning_tag.size());
					if (ret.empty() && fz::starts_with(str, placeholder)) {
						ret.append(p.second);
						str.remove_prefix(placeholder.size());
						goto begin_loop;
					}
				}

				if (fz::starts_with(str, View(p.first))) {
					ret.append(p.second);
					str.remove_prefix(p.first.size());
					goto begin_loop;
				}
			}

			ret.append(escape_string);

			if (fz::starts_with(str, escape_string))
				str.remove_prefix(escape_string.size());
		}

		return ret;
	}

}

template <typename String>
String anything_between(typename String::value_type left, typename String::value_type right, typename String::value_type escape, const String &placeholder)
{
	return String(impl::anything_between_tag).append(1, left).append(1, right).append(1, escape).append(placeholder);
}

template <typename String>
String only_at_beginning(const String &placeholder)
{
	return String(impl::only_at_beginning_tag).append(placeholder);
}

template <typename EscapeMap = std::initializer_list<std::pair<std::string_view, std::string_view>>, typename To = typename EscapeMap::value_type::first_type, typename From = typename EscapeMap::value_type::second_type, std::enable_if_t<
	std::is_constructible_v<std::string_view, To> &&
	std::is_constructible_v<std::string_view, From>
>* = nullptr>
std::string escaped(std::string_view str, std::string_view escape_string, const EscapeMap &escape_map = {})
{
	return impl::escaped<std::string>(str, escape_string, escape_map);
}

template <typename EscapeMap = std::initializer_list<std::pair<std::wstring_view, std::wstring_view>>, typename To = typename EscapeMap::value_type::first_type, typename From = typename EscapeMap::value_type::second_type, std::enable_if_t<
	std::is_constructible_v<std::wstring_view, To> &&
	std::is_constructible_v<std::wstring_view, From>
>* = nullptr>
std::wstring escaped(std::wstring_view str, std::wstring_view escape_string, const EscapeMap &escape_map = {})
{
	return impl::escaped<std::wstring>(str, escape_string, escape_map);
}

template <typename EscapeMap = std::initializer_list<std::pair<std::string_view, std::string_view>>, typename To = typename EscapeMap::value_type::first_type, typename From = typename EscapeMap::value_type::second_type, std::enable_if_t<
	std::is_constructible_v<std::string_view, To> &&
	std::is_constructible_v<std::string_view, From>
>* = nullptr>
std::string unescaped(std::string_view str, std::string_view escape_string, const EscapeMap &escape_map = {})
{
	return impl::unescaped<std::string>(str, escape_string, escape_map);
}

template <typename EscapeMap = std::initializer_list<std::pair<std::wstring_view, std::wstring_view>>, typename To = typename EscapeMap::value_type::first_type, typename From = typename EscapeMap::value_type::second_type, std::enable_if_t<
	std::is_constructible_v<std::wstring_view, To> &&
	std::is_constructible_v<std::wstring_view, From>
>* = nullptr>
std::wstring unescaped(std::wstring_view str, std::wstring_view escape_string, const EscapeMap &escape_map = {})
{
	return impl::unescaped<std::wstring>(str, escape_string, escape_map);
}

template <typename String>
struct join_traits
{
	static inline const String default_separator = fzS(typename String::value_type, " ");
	static inline const String default_prefix = fzS(typename String::value_type, "");

	template <typename Value>
	static inline auto to_string(const Value &v) -> decltype(toString<String>(v))
	{
		return toString<String>(v);
	}
};

template <typename String, typename Container>
auto join(
	const Container &c,
	const decltype(join_traits<String>::default_separator) &sep = join_traits<String>::default_separator,
	const decltype(join_traits<String>::default_prefix) &pre = join_traits<String>::default_prefix
) -> decltype(join_traits<String>::to_string(*c.begin()))
{
	String ret;

	for (const auto &s: c)
		ret.append(pre).append(join_traits<String>::to_string(s)).append(sep);

	if (!ret.empty())
		ret.resize(ret.size() - sep.size());

	return ret;
}

template <typename Container, typename String = std::decay_t<decltype(*std::cbegin(std::declval<Container>()))>>
auto join(
	const Container &c,
	const decltype(join_traits<String>::default_separator) &sep = join_traits<String>::default_separator,
	const decltype(join_traits<String>::default_prefix) &pre = join_traits<String>::default_prefix
) -> decltype(join_traits<String>::to_string(*std::cbegin(c)))
{
	String ret;

	for (const auto &s: c)
		ret.append(pre).append(join_traits<String>::to_string(s)).append(sep);

	if (!ret.empty())
		ret.resize(ret.size() - sep.size());

	return ret;
}

template <typename String>
struct quote_traits: join_traits<String>
{
	static inline const String default_opening = fzS(typename String::value_type, "\"");
	static inline const String default_closing = fzS(typename String::value_type, "\"");
};

template <typename String>
String quote(const String &v, const String &opening = quote_traits<String>::default_opening, const String &closing = quote_traits<String>::default_closing)
{
	String ret;

	ret.append(opening).append(v).append(closing);

	return ret;
}

template <typename String, typename Value, std::enable_if_t<!std::is_same_v<String, Value>>* = nullptr>
auto quote(const Value &v, const decltype(quote_traits<String>::default_opening) &opening = quote_traits<String>::default_opening, const decltype(quote_traits<String>::default_closing) &closing = quote_traits<String>::default_closing) -> decltype(quote(join_traits<String>::to_string(v), opening, closing))
{
	return quote(join_traits<String>::to_string(v), opening, closing);
}


template <typename String>
String &&remove_ctrl_chars(String &&s)
{
	s.erase(std::remove_if(s.begin(), s.end(), [](auto c) {
		return ((c != 9 && c <= 31) || c == 127);
	}), s.end());

	return std::forward<String>(s);
}

template <typename String>
String removed_ctrl_chars(const String &s)
{
	String copy(s);
	return remove_ctrl_chars(std::move(copy));
}

template <typename String>
String html_encoded(const String &s)
{
	using C = typename String::value_type;

	return escaped(s, {}, {
		{ fzS(C, "&amp;"),  fzS(C, "&")   },
		{ fzS(C, "&lt;"),   fzS(C, "<")   },
		{ fzS(C, "&gt;"),   fzS(C, ">")   },
		{ fzS(C, "&quot;"), fzS(C, "\""), },
		{ fzS(C, "&apos;"), fzS(C, "'"),  },
	});
}

}

#endif // FZ_STRING_HPP
