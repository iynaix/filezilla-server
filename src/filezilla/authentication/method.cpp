#include "method.hpp"
#include "../string.hpp"
#include "../mpl/with_index.hpp"
#include "../mpl/at.hpp"

namespace fz::authentication {

const available_methods available_methods::none;

available_methods available_methods::filter(const methods_set &set) const
{
	available_methods ret;

	for (const auto &s: *this) {
		if ((s & set) == set)
			ret.push_back(s);
	}

	return ret;
}

bool available_methods::has(const methods_set &set) const
{
	for (const auto &s: *this) {
		if (s == set)
			return true;
	}

	return false;
}


bool available_methods::can_verify(const methods_set &methods) const
{
	for (const auto &s: *this) {
		// Anything can verify none.
		// All the rest can verify only itself
		if (!s || (methods && (s & methods) == methods))
			return true;
	}

	return false;
}

bool available_methods::remove(const methods_set &set)
{
	auto old_size = size();

	erase(std::remove(begin(), end(), set), end());

	return size() != old_size;
}

bool available_methods::set_verified(const any_method &method)
{
	if (empty()) {
		return false;
	}

	std::vector<methods_set> res;

	for (auto &s: *this) {
		if (s.has(method)) {
			s.erase(method);

			if (s) {
				res.push_back(std::move(s));
			}
			else {
				assign({ {} });
				return false;
			}
		}
	}

	if (!res.empty()) {
		swap(res);
	}

	return true;
}

bool available_methods::is_auth_necessary() const
{
	for (const auto &s: *this) {
		if (!s)
			return false;
	}

	return !empty();
}

bool available_methods::is_auth_possible() const
{
	return !empty();
}

std::string to_string(const available_methods &am)
{
	return fz::join<std::string>(am, ",");
}

std::string to_string(const any_method &m)
{
	return std::visit([](auto &m) { return m.template name<char>; }, static_cast<const any_method::variant&>(m));
}

std::string to_string(const methods_list &ml)
{
	return fz::sprintf("(%s)", fz::join<std::string>(ml, ","));
}

std::wstring to_wstring(const available_methods &am)
{
	return fz::join<std::wstring>(am, L",");
}

std::wstring to_wstring(const any_method &m)
{
	return std::visit([](auto &m) { return m.template name<wchar_t>; }, static_cast<const any_method::variant&>(m));
}

std::wstring to_wstring(const methods_list &ml)
{
	return fz::sprintf(L"(%s)", fz::join<std::wstring>(ml, L","));
}

template <typename String, std::size_t Size>
// Taking in as first parameter a reference to the methods_set so that we're sure this definition will never collide with any other defined in third party libraries
static String to_string(const methods_set &, const std::bitset<Size> &bits)
{
	using C = typename String::value_type;

	if (bits.none()) {
		return method::none::name<C>;
	}

	String ret;

	for (std::size_t i = 0; i < Size; ++i) {
		if (bits.test(i)) {
			ret += mpl::with_index<Size>(i, [](auto i) {
				return mpl::at_c_t<any_method::variant, i+1>::template name<C>;
			});

			ret += fzS(C, "|");
		}
	}

	if (ret.size() > 0)
		ret.resize(ret.size()-1);

	return ret;
}

std::string to_string(const methods_set &set)
{
	return to_string<std::string>(set, set.bits_);
}

std::wstring to_wstring(const methods_set &set)
{
	return to_string<std::wstring>(set, set.bits_);
}

methods_set::methods_set(std::string_view v) noexcept {
	util::parseable_range r(v);

	if (unsigned long long number; parse_int(r, number) && eol(r)) {
		bits_ = bits(number);
	}
	else
	for (auto m: strtokenizer(v, "|", false)) {
		mpl::for_each<any_method::variant>([&](auto t, auto i) {
			std::string_view name = decltype(t)::type::template name<char>;
			if (m == name) {
				if (i > 0) {
					bits_.set(i-1, true);
				}
				return false;
			}

			return true;
		});
	}
}

}
