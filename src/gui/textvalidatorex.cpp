#include <libfilezilla/glue/wx.hpp>
#include <libfilezilla/uri.hpp>
#include <libfilezilla/string.hpp>

#include <wx/event.h>
#include <wx/textentry.h>

#include "../filezilla/util/overload.hpp"

#include "glue.hpp"
#include "textvalidatorex.hpp"
#include "locale.hpp"
#include "helpers.hpp"

TextValidatorEx::TextValidatorEx(long style, Value value_ptr, ValidatorFunc func)
	: wxTextValidator(style)
	, func_(std::move(func))
	, value_ptr_(std::move(value_ptr))
{}

TextValidatorEx::TextValidatorEx(long style, ValidatorFunc func)
	: wxTextValidator(style)
	, func_(std::move(func))
	, value_ptr_()
{}

wxObject *TextValidatorEx::Clone() const
{
	return new TextValidatorEx(*this);
}

wxString TextValidatorEx::IsValid(const wxString &val) const
{
	auto err = wxTextValidator::IsValid(val);

	if (err.empty() && func_)
		err = func_(val, final_validation_);

	return err;
}

bool TextValidatorEx::Validate(wxWindow *parent)
{
	final_validation_ = true;
	auto res = wxTextValidator::Validate(parent);
	final_validation_ = false;

	return res;
}

bool TextValidatorEx::TransferFromWindow()
{
	auto && value = GetTextEntry()->GetValue();

	std::visit(fz::util::overload{
		[&](std::nullptr_t) { },
		[&](std::string *v) { if (v) *v = fz::to_utf8(std::move(value)); },
		[&](std::wstring *v) { if (v) *v = fz::to_wstring(std::move(value)); },
		[&](wxString *v) { if (v) *v = std::move(value); },
		[&](std::vector<std::string> *v) { if (v) *v = fz::strtok(fz::to_utf8(value.Trim(true).Trim(false)), " \n\r\t"); }
	}, value_ptr_);

	return true;
}

bool TextValidatorEx::TransferToWindow()
{
	auto && value = std::visit(fz::util::overload{
		[&](std::nullptr_t) { return wxString{}; },
		[&](std::vector<std::string> *v) { return v ? fz::join<wxString>(*v) : wxString{}; },
		[&](auto *v) { return v ? fz::to_wxString(*v) : wxString{}; }
	}, value_ptr_);

	GetTextEntry()->SetValue(std::move(value));

	return true;
}

wxString ip_validation(const wxString &val, bool final_validation)
{
	if (final_validation) {
		if (fx::IdentifyAddressType(val) == fz::address_type::unknown) {
			return _S("Invalid IP address.");
		}
	}

	return {};
}

TextValidatorEx::ValidatorFunc absolute_path_validation(fz::util::fs::path_format server_path_format)
{
	return [server_path_format](const wxString &val, bool final_validation) -> wxString {
		using namespace fz::util::fs;

		if (final_validation) {
			if (val.empty())
				return _S("Path must not be empty.");

			auto && path = fz::to_native(val);

			bool is_absolute = server_path_format == unix_format
				? unix_native_path(path).is_absolute()
				: windows_native_path(path).is_absolute();

			if (!is_absolute)
				return _S("Path must be absolute.");

			bool is_valid = server_path_format == unix_format
				? unix_native_path(path, server_path_format).is_valid()
				: windows_native_path(path, server_path_format).is_valid();

			if (!is_valid)
				return _S("Path contains invalid characters");
		}

		return {};
	};
}

TextValidatorEx::ValidatorFunc absolute_uri_validation(std::set<std::string> allowed_schemes)
{
	return [allowed_schemes = std::move(allowed_schemes)](const wxString &val, bool final_validation) -> wxString {
		using namespace fz::util::fs;

		if (final_validation) {
			if (val.empty())
				return _S("The URI must not be empty.");

			fz::uri uri(fz::to_utf8(val));

			if (!uri)
				return _S("The URI is not valid.");

			if (!uri.is_absolute())
				return _S("The URI is not absolute.");

			if (!allowed_schemes.empty() && !allowed_schemes.count(uri.scheme_))
				return _F("Allowed URI schemes are: %s", fz::join<wxString>(allowed_schemes, wxT(", ")));
		}

		return {};
	};
}

TextValidatorEx::ValidatorFunc field_must_not_be_empty(const wxString &field_name)
{
	return [field_name = field_name](const wxString &val, bool final_validation) -> wxString {
		if (final_validation && val.empty())
			return _F("%s must not be empty", field_name);

		return {};
	};
}
