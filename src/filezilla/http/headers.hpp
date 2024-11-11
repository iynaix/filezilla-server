#ifndef FZ_HTTP_HEADERS_HPP
#define FZ_HTTP_HEADERS_HPP

#include <map>

#include <libfilezilla/time.hpp>
#include <libfilezilla/logger.hpp>

#include "field.hpp"

namespace fz::http
{

class headers: public std::map<field::name, field::value>
{
public:
	static const key_type Accept;
	static const key_type Allowed;
	static const key_type Authorization;
	static const key_type Connection;
	static const key_type Cache_Control;
	static const key_type Content_Disposition;
	static const key_type Content_Length;
	static const key_type Content_Type;
	static const key_type Cookie;
	static const key_type Expect;
	static const key_type Host;
	static const key_type Last_Modified;
	static const key_type Location;
	static const key_type Pragma;
	static const key_type Set_Cookie;
	static const key_type Transfer_Encoding;
	static const key_type User_Agent;
	static const key_type Vary;
	static const key_type WWW_Authenticate;
	static const key_type X_FZ_INT_Original_Path;
	static const key_type X_FZ_INT_File_Name;
	static const key_type X_FZ_Action;
	static const key_type X_FZ_Recursive;

public:
	using map::map;

	template <typename K, std::enable_if_t<std::is_constructible_v<key_type, K>>* = nullptr>
	decltype(auto) operator[](K && key) {
		return map::operator[](key_type(std::forward<K>(key)));
	}

	template <typename Key = key_type, typename Def = field::value_view>
		std::enable_if_t<std::is_constructible_v<key_type, Key> && std::is_constructible_v<field::value_view, Def>,
	std::common_type_t<Def, field::value_view>>
	get(Key && key, Def && def = {}) const
	{
		if (auto it = find(key_type(std::forward<Key>(key))); it != end())
			return Def(it->second);

		return std::forward<Def>(def);
	}

	fz::datetime get_retry_at(datetime now = datetime::now()) const;
	fz::datetime get_retry_at_with_min_delay(unsigned int min_seconds_later) const;

	field::value match_preferred_content_type(std::initializer_list<std::string_view> list);

	static const std::string &default_user_agent();

	field::component_view get_cookie(field::component_view name, bool secure);

	void set_cookie(field::component_view name, field::component_view value, field::component_view path, bool secure, bool http_only, fz::duration duration);
	static std::string make_cookie(field::component_view name, field::component_view value, field::component_view path, bool secure, bool http_only, fz::duration duration);
};

}

#endif // FZ_HTTP_HEADERS_HPP
