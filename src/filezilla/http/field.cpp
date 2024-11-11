#include <libfilezilla/string.hpp>

#include "field.hpp"

namespace fz::http::field {

namespace {

// FIXME: support quoted strings

std::optional<std::string_view> get_parameters_if_value_matches(std::string_view v, std::string_view what, char sep)
{
	if (starts_with<true>(v, what)) {
		v.remove_prefix(what.size());

		if (v.empty()) {
			return v;
		}

		if (v[0] == sep) {
			return v.substr(1);
		}
	}

	return std::nullopt;
}

std::string_view get_value_parameters(std::string_view v, char sep)
{
	auto semicolon = v.find(sep);
	if (semicolon != v.npos) {
		v = v.substr(semicolon+1);

		return v;
	}

	return {};
}

}

value_view::list value_view::as_list() const
{
	return { *this };
}

value::list value::as_list()
{
	return { *this };
}

value_view::params_list value_view::as_params_list(bool comma) const
{
	return { *this, comma };
}

value::params_list value::as_params_list(bool comma)
{
	return { *this, comma };
}


value &value::append(value_view v)
{
	as_list().append(v);
	return *this;
}

value &value::operator+=(value_view v)
{
	as_list().append(v);
	return *this;
}

value &value::append_param(std::string_view p)
{
	as_params_list().append(p);
	return *this;
}

value &value::operator%=(std::string_view p)
{
	as_params_list().append(p);
	return *this;
}

bool value::is(component_view v) const
{
	return value_view(s_).is(v);
}

std::optional<component_view> value::get_param(component_view key, bool case_insensitive) const
{
	return value_view(s_).get_param(key, case_insensitive);
}

bool value_view::is(component_view v) const
{
	return get_parameters_if_value_matches(s_, v, ';').has_value();
}

std::optional<component_view> value_view::get_param(component_view key, bool case_insensitive) const
{
	return value_view(get_value_parameters(s_, ';')).as_params_list().get(key, case_insensitive);
}

value_view value_view::list::get(component_view to_find) const
{
	for (auto v: iterable()) {
		if (v.empty()) {
			continue;
		}

		if (auto pl = get_parameters_if_value_matches(v, to_find, ';')) {
			return v;
		}
	}

	return {};
}

value_view value_view::list::last() const
{
	std::string_view ret = s_;

	auto last_comma = ret.rfind(',');
	if (last_comma != ret.npos) {
		ret.remove_prefix(last_comma+1);
	}

	trim(ret, " \t");

	return ret;
}

// FIXME: needs to quote v's that contain unallowed characters outside of quotes
value::list &value::list::append(value_view v)
{
	if (!s_.empty()) {
		s_.append(", ");
	}

	s_.append(v);

	return *this;
}

value_view value::list::get(component_view v)
{
	return value_view(s_).as_list().get(v);
}

value_view value::list::last()
{
	return value_view(s_).as_list().last();
}

std::optional<component_view> value_view::params_list::get(component_view to_find, bool case_insensitive) const
{
	for (auto v: iterable()) {
		auto p = v.str();

		if (p.empty()) {
			continue;
		}

		if (case_insensitive ? starts_with<true>(p, to_find.str()) : starts_with<false>(p, to_find.str())) {
			p.remove_prefix(to_find.str().size());

			if (p.empty()) {
				return p;
			}

			if (starts_with(p, std::string_view("="))) {
				p.remove_prefix(1);
				return p;
			}
		}
	}

	return std::nullopt;
}

fz::http::field::value::params_list &value::params_list::append(value_view v)
{
	if (!s_.empty()) {
		s_.append(sep_).append(" ");
	}

	s_.append(v);

	return *this;
}

}
