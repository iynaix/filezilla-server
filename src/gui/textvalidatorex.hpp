#ifndef TEXTVALIDATOREX_H
#define TEXTVALIDATOREX_H

#include <libfilezilla/time.hpp>

#include <wx/valtext.h>

#include <functional>
#include <variant>
#include <set>

#include "../filezilla/util/filesystem.hpp"

struct TextValidatorEx: wxTextValidator
{
	using ValidatorFunc = std::function<wxString(const wxString &val, bool final_validation)>;

	using Value = std::variant<
		std::nullptr_t,
		std::string *, std::wstring *, wxString *,
		std::vector<std::string> *
	>;

	TextValidatorEx(long style = wxFILTER_NONE, Value value = {}, ValidatorFunc func = {});
	TextValidatorEx(long style, ValidatorFunc func);

private:
	wxObject *Clone() const override;
	wxString IsValid(const wxString& val) const override;
	bool Validate(wxWindow *parent) override;
	bool TransferFromWindow() override;
	bool TransferToWindow() override;

	ValidatorFunc func_;
	bool final_validation_{};

	Value value_ptr_{};
};

wxString ip_validation(const wxString &val, bool final_validation);

TextValidatorEx::ValidatorFunc absolute_path_validation(fz::util::fs::path_format server_path_format);
TextValidatorEx::ValidatorFunc absolute_uri_validation(std::set<std::string> allowed_schemes = {});
TextValidatorEx::ValidatorFunc field_must_not_be_empty(const wxString &field_name);

#endif // TEXTVALIDATOREX_H
