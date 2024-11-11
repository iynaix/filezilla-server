#include <libfilezilla/glue/wx.hpp>

#include "passwordeditor.hpp"
#include "locale.hpp"
#include "glue.hpp"

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/checkbox.h>
#include <wx/valtext.h>

#include "helpers.hpp"
#include "textvalidatorex.hpp"


bool PasswordEditor::Create(wxWindow *parent, bool allow_no_password, wxWindowID winid, const wxPoint &pos, const wxSize &size, long style, const wxString &name)
{
	if (!wxPanel::Create(parent, winid, pos, size, style, name))
		return false;

	if (allow_no_password) {
		wxHBox(this, 0) = {
			{ wxSizerFlags(0).Align(wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT), password_enabler_ = new wxCheckBox(this, wxID_ANY, wxS("")) },
			{ 1, password_text_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD) }
		};

		password_enabler_->Bind(wxEVT_CHECKBOX, [&](wxCommandEvent &ev) {
			if (!password_)
				return;

			bool enabled = ev.GetInt();

			if (!enabled) {
				password_text_->Disable();
				password_text_->Clear();
				password_text_->SetHint(wxEmptyString);
				password_text_->SetValidator(wxValidator());
			}
			else {
				password_text_->Enable();
				password_text_->SetValidator(TextValidatorEx(wxFILTER_NONE, nullptr, field_must_not_be_empty(_S("Password"))));
			}

			Event::Changed.Process(this, this);
		});
	}
	else {
		wxHBox(this, 0) = {
			{ 1, password_text_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD) }
		};
	}

	return true;
}

void PasswordEditor::SetPassword(fz::authentication::any_password *password)
{
	password_ = password;

	if (!password || !password->has_valid_password()) {
		password_text_->Clear();
		password_text_->SetHint(wxEmptyString);

		if (password_enabler_) {
			password_text_->SetValidator(wxValidator());
			password_enabler_->SetValue(false);
			password_text_->Disable();
		}
		else {
			password_text_->Enable();
			password_text_->SetValidator(TextValidatorEx(wxFILTER_NONE, nullptr, field_must_not_be_empty(_S("Password"))));
		}
	}
	else {
		password_text_->Clear();
		password_text_->SetHint(_S("Leave empty to keep existing password"));
		password_text_->Enable();
		password_text_->SetValidator({});
		if (password_enabler_)
			password_enabler_->SetValue(true);
	}
}

bool PasswordEditor::HasPassword() const
{
	return password_text_->IsThisEnabled();
}

bool PasswordEditor::TransferDataToWindow()
{
	if (!wxPanel::TransferDataToWindow())
		return false;

	SetPassword(password_);
	return true;
}

namespace {
	class password_criteria
	{
		inline static const wxString yes = wxS("\u2713");
		inline static const wxString no = wxS("");

	public:
		wxString has_min_length = no;
		wxString has_number = no;
		wxString has_special_char = no;
		wxString has_upper_case = no;
		wxString has_lower_case = no;

		explicit password_criteria(const wxString& password)
		{
			if (password.length() >= 12) {
				has_min_length = yes;
			}

			for (const auto& ch : password) {
				if (wxIsdigit(ch)) {
					has_number = yes;
				}
				else
				if (wxIsupper(ch)) {
					has_upper_case = yes;
				}
				else
				if (wxIslower(ch)) {
					has_lower_case = yes;
				}
				else
				if (wxIspunct(ch)) {
					has_special_char = yes;
				}
			}
		}

		explicit operator bool() const
		{
			return has_min_length == yes &&
				   has_number == yes &&
				   has_special_char == yes &&
				   has_upper_case == yes &&
				   has_lower_case == yes;
		}
	};
}

bool PasswordEditor::CheckIsStrongEnough()
{
	bool ok = true;

	if (!password_text_->IsThisEnabled()) {
		ok = wxMsg::WarningConfirm(_S("Not setting a password is not secure.")).Ext(_S("Do you wish to proceed with the current password choice?")) == wxID_YES;
	}
	else
	if (password_enabler_) {
		password_criteria pc(password_text_->GetValue());

		if (!pc) {
			ok = wxMsg::WarningConfirm(_S("The chosen password does not meet the recommended security criteria.")).Ext(_F(
				"For optimal security, your password should include the following:\n"
				"\n"
				"  1. A minimum length of 12 characters. %s\n"
				"  2. At least one numeral (0-9). %s\n"
				"  3. At least one special character (e.g., !, @, #, $). %s\n"
				"  4. At least one uppercase letter (A-Z). %s\n"
				"  5. At least one lowercase letter (a-z). %s\n"
				"\n"
				"Consider using a sequence of words or a sentence, as it can be both secure and memorable.\n"
				"\n"
				"Do you wish to proceed with the current password choice?",
				pc.has_min_length,
				pc.has_number,
				pc.has_special_char,
				pc.has_upper_case,
				pc.has_lower_case
			)) == wxID_YES;
		}
	}

	return ok;
}

bool PasswordEditor::TransferDataFromWindow()
{
	if (!wxPanel::TransferDataFromWindow())
		return false;

	if (!password_)
		return true;

	bool old_pass_none = !password_->has_password();
	bool new_pass_none = !password_text_->IsThisEnabled();

	if ((!old_pass_none && !password_text_->GetValue().IsEmpty()) || old_pass_none != new_pass_none) {
		if (!CheckIsStrongEnough()) {
			return false;
		}

		if (password_text_->IsThisEnabled()) {
			*password_ = fz::authentication::default_password(fz::to_utf8(password_text_->GetValue()));
			SetPassword(password_);
		}
		else {
			*password_= fz::authentication::password::none();
		}
	}

	return true;
}

// BUG: Clang requires these: clang's bug or something I don't understand about C++?
template struct wxValidateOnlyIfCurrentPage<PasswordEditor>;
template struct wxCreator::CtrlFix<wxValidateOnlyIfCurrentPage<PasswordEditor>>;

