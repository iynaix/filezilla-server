#ifndef GLUE_HPP
#define GLUE_HPP

#include <libfilezilla/glue/wx.hpp>
#include <libfilezilla/time.hpp>
#include <libfilezilla/json.hpp>
#include "../filezilla/string.hpp"
#include "../filezilla/serialization/trait.hpp"

namespace fz {

inline wxString to_wxString(const std::string &s)
{
	return wxString::FromUTF8(s.data(), s.size());
}

inline wxString to_wxString(const std::string_view &s)
{
	return wxString::FromUTF8(s.data(), s.size());
}

template <std::size_t N, std::enable_if_t<(N>1)>* = nullptr>
inline wxString to_wxString(const char (&s)[N])
{
	return wxString::FromUTF8(s, N-1);
}

template <std::size_t N, std::enable_if_t<(N>1)>* = nullptr>
inline wxString to_wxString(const wchar_t (&s)[N])
{
	return wxString(s, N-1);
}

inline wxString to_wxString(const std::wstring &s)
{
	return wxString(s.data(), s.size());
}

inline wxString to_wxString(const std::wstring_view &s)
{
	return wxString(s.data(), s.size());
}

inline wxString to_wxString(const wxString &s)
{
	return s;
}

inline wxString to_wxString(const fz::datetime &dt)
{
	return fz::to_wxString(dt.format(L"%c", fz::datetime::zone::local));
}

inline wxString to_wxString(const fz::json &j)
{
	return to_wxString(j.string_value());
}

template <>
struct join_traits<wxString>
{
	inline static const wxString default_separator = wxT(" ");
	inline static const wxString default_prefix    = wxT("");

	template <typename Value>
	static inline auto to_string(const Value &v) -> decltype(to_wxString(v))
	{
		return to_wxString(v);
	}
};

template <>
struct quote_traits<wxString>: join_traits<wxString>
{
	static inline const wxString default_opening = wxT("\"");
	static inline const wxString default_closing = wxT("\"");
};

template <typename String, std::enable_if_t<std::is_same_v<wxString, std::decay_t<String>>>* = nullptr>
inline std::string to_string(const String &s)
{
	return fz::to_utf8(s.ToStdWstring());
}

}

template <typename Archive>
std::wstring save_minimal(const Archive &, const wxString &s)
{
	return fz::to_wstring(s);
}

template <typename Archive>
void load_minimal(const Archive &, wxString &s, const std::wstring &in)
{
	s = fz::to_wxString(in);
}

FZ_SERIALIZATION_SPECIALIZE_ALL(wxString, unversioned_nonmember_load_save_minimal)

#endif // GLUE_HPP
