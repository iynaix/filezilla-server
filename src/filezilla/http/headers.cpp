#include <string_view>

#include "headers.hpp"

#include "../build_info.hpp"

namespace fz::http
{

datetime headers::get_retry_at(datetime now) const
{
	datetime at;

	if (const auto &ra = get("Retry-After"); ra && !at.set_rfc822(ra)) {
		if (auto secs = fz::to_integral<unsigned int>(ra)) {
			at = now + duration::from_seconds(secs);
		}
	}

	return at;
}

fz::datetime headers::get_retry_at_with_min_delay(unsigned int min_seconds_later) const
{
	auto now = datetime::now();
	auto later = now + duration::from_seconds(min_seconds_later);

	datetime at = get_retry_at(now);

	if (at < later)
		at = later;

	return at;
}

// FIXME: should handle mime type parameters
field::value headers::match_preferred_content_type(std::initializer_list<std::string_view> list)
{
	if (list.begin() == list.end()) {
		return {};
	}

	auto accept = get(Accept).as_list();

	if (!accept) {
		return std::string(*list.begin());
	}

	std::string_view best_match;
	std::string_view highest_q = "0";

	for (auto c: list) {
		auto v = accept.get(c);
		if (!v) {
			if (auto slash = c.find('/'); slash != std::string_view::npos) {
				auto type = c.substr(0, slash);
				v = accept.get(std::string(type).append("/*"));
			}
		}

		if (!v) {
			v = accept.get("*/*");
		}

		std::string_view q_value = v ? v.get_param("q").value_or("1") : "0";

		// FIXME: perhaps should validate q_value
		if (q_value > highest_q) {
			highest_q = q_value;
			best_match = c;
		}
	}

	return best_match;
}

const std::string &headers::default_user_agent()
{
	static const auto user_agent = [] {
		auto ret = fz::replaced_substrings(build_info::package_name, " ", "-");
		ret += "/";
		ret += build_info::version;
		ret += " (";
		ret += build_info::host;
		ret += ")";

		return ret;
	}();

	return user_agent;
}

field::component_view headers::get_cookie(field::component_view name, bool secure)
{
	static thread_local	std::string secure_name;

	if (secure) {
		secure_name = "__Secure-";
		secure_name += name;
		name = secure_name;
	}

	auto list = get(Cookie).as_list();

	for (auto c: list.iterable()) {
		if (auto p = c.as_params_list().get(name)) {
			return *p;
		}
	}

	return {};
}

void headers::set_cookie(field::component_view name, field::component_view value, field::component_view path, bool secure, bool http_only, duration duration)
{
	operator[](Set_Cookie) = make_cookie(name, value, path, secure, http_only, duration);
}

std::string headers::make_cookie(field::component_view name, field::component_view value, field::component_view path, bool secure, bool http_only, duration duration)
{
	std::string ret;

	if (secure) {
		ret += "__Secure-";
	}

	ret += name;
	ret += '=';
	ret += value;
	ret += ';';

	if (!path.empty()) {
		ret += "Path=";
		ret += path;
		ret += ';';
	}

	if (secure) {
		ret += "Secure;";
	}

	if (http_only) {
		ret += "HttpOnly;";
	}

	if (duration) {
		ret += "Max-Age=";
		ret += fz::to_string(duration.get_seconds());
		ret += ';';
	}

	ret += "SameSite=Strict";

	return ret;
}

using namespace std::string_view_literals;

const headers::key_type headers::Accept = "Accept"sv;
const headers::key_type headers::Allowed = "Allowed"sv;
const headers::key_type headers::Authorization = "Authorization"sv;
const headers::key_type headers::Cache_Control = "Cache-Control"sv;
const headers::key_type headers::Connection = "Connection"sv;
const headers::key_type headers::Content_Disposition = "Content-Disposition"sv;
const headers::key_type headers::Content_Length = "Content-Length"sv;
const headers::key_type headers::Content_Type = "Content-Type"sv;
const headers::key_type headers::Cookie = "Cookie"sv;
const headers::key_type headers::Expect = "Expect"sv;
const headers::key_type headers::Host = "Host"sv;
const headers::key_type headers::Last_Modified = "Last-Modified"sv;
const headers::key_type headers::Location = "Location"sv;
const headers::key_type headers::Pragma = "Pragma"sv;
const headers::key_type headers::Set_Cookie = "Set-Cookie"sv;
const headers::key_type headers::Transfer_Encoding = "Transfer-Encoding"sv;
const headers::key_type headers::User_Agent = "User-Agent"sv;
const headers::key_type headers::Vary = "Vary"sv;
const headers::key_type headers::WWW_Authenticate = "WWW-Authenticate"sv;
const headers::key_type headers::X_FZ_INT_Original_Path = "X-FZ-INT-Original-Path"sv;
const headers::key_type headers::X_FZ_INT_File_Name = "X-FZ-INT-File-Name"sv;
const headers::key_type headers::X_FZ_Action = "X-FZ-Action"sv;
const headers::key_type headers::X_FZ_Recursive = "X-FZ-Recursive"sv;

}
