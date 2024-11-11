#include <libfilezilla/glue/wx.hpp>

#include "credentialseditor.hpp"
#include "locale.hpp"
#include "glue.hpp"
#include "passwordeditor.hpp"

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/checkbox.h>
#include <wx/valtext.h>
#include <wx/choicebk.h>
#include <wx/simplebook.h>

#include "helpers.hpp"

CredentialsEditor::~CredentialsEditor()
{
}

bool CredentialsEditor::Create(wxWindow *parent, wxWindowID winid, const wxPoint &pos, const wxSize &size, long style, const wxString &name)
{
	if (!wxPanel::Create(parent, winid, pos, size, style, name))
		return false;

	wxVBox(this, 0) = {
		creds_book_ = wxCreate<wxNavigationEnabled<wxSimplebook>>(this) | [&](auto p) {
			creds_choices_ = wxPage<wxChoicebook>(wxValidateOnlyIfCurrent)(p, wxT("*ALL*"), true, wxID_ANY) | [&](wxChoicebook *p) {
				wxPage(wxValidateOnlyIfCurrent)(p, _S("Do not require authentication")) | [&](auto p) {
					wxTransferDataToWindow(p, [&] {
						Event::ChangedMode.Process(this, this, AuthMode::NoAuth);
						return true;
					});
				};

				password_editor_ = wxPage<PasswordEditor>(wxValidateOnlyIfCurrent)(p, _S("Require a password to log in"), true, false);
				wxTransferDataToWindow(password_editor_, [this] {
					if (!password_)
						password_ = fz::authentication::password::none();

					password_editor_->SetPassword(&*password_);

					Event::ChangedMode.Process(this, this, AuthMode::OwnPassword);
					return true;
				});

				wxPage(wxValidateOnlyIfCurrent)(p, _S("Use system credentials to log in")) |[&] (auto p) {
					wxVBox(p, 0) = all_permission_checks_too_ = new wxCheckBox(p, wxID_ANY, _S("Use system credentials also for accessing files and directories"));

					wxTransferDataToWindow(p, [&] {
						Event::ChangedMode.Process(this, this, AuthMode::SystemPassword);
						return true;
					});
				};
			};

			wxPage<wxChoicebook>(wxValidateOnlyIfCurrent)(p, wxT("*SYSTEM USER ONLY*"), false, wxID_ANY) | [&](wxChoicebook * p) {
				wxPage(wxValidateOnlyIfCurrent)(p, _S("Use system credentials to log in")) |[&] (auto p) {
					wxVBox(p, 0) = system_user_only_permission_checks_too_ = new wxCheckBox(p, wxID_ANY, _S("Use system credentials also for accessing files and directories"));

					wxTransferDataToWindow(p, [&] {
						Event::ChangedMode.Process(this, this, AuthMode::SystemPassword);
						return true;
					});
				};
			};
		},
	};

	SetCredentials(false, nullptr, nullptr, {});

	return true;
}

void CredentialsEditor::SetCredentials(bool system_user, fz::authentication::credentials *credentials, bool *no_auth, const std::string &user_name)
{
	system_user_ = system_user;
	credentials_ = credentials;
	no_auth_ = no_auth;
	user_name_ = user_name;

	if (!credentials || !no_auth) {
		creds_choices_->ChangeSelection(0);
		creds_book_->ChangeSelection(0);
		creds_book_->Disable();
		password_editor_->SetPassword(nullptr);

		return;
	}

	creds_book_->Enable();

	if (system_user) {
		if (auto im = credentials->password.get_impersonation()) {
			creds_book_->ChangeSelection(1);
			password_ = {};
			system_user_only_permission_checks_too_->SetValue(!im->login_only);
		}
	}
	else {
		creds_book_->ChangeSelection(0);

		if (auto p = credentials->password.get()) {
			password_ = *p;
		}
		else
		if (credentials->password.get_impersonation()) {
			password_ = {};
		}
		else {
			password_ = fz::authentication::password::none();
		}

		if (*no_auth) {
			creds_choices_->ChangeSelection(0);
		}
		else
		if (auto im = credentials->password.get_impersonation()) {
			creds_choices_->ChangeSelection(2);

			all_permission_checks_too_->SetValue(!im->login_only);
			password_editor_->SetPassword(nullptr);
		}
		else {
			creds_choices_->ChangeSelection(1);

			password_editor_->SetPassword(&*password_);
		}
	}
}

bool CredentialsEditor::TransferDataFromWindow()
{
	if (!wxPanel::TransferDataFromWindow())
		return false;

	if (!credentials_)
		return true;

	if (creds_book_->GetSelection() == 0) {
		if (creds_choices_->GetSelection() == 1) {
			wxASSERT_MSG(password_, "Invalid state for password_");

			credentials_->password = *password_;
		}
		else
		if (creds_choices_->GetSelection() == 2)
			credentials_->password.impersonate(!all_permission_checks_too_->GetValue());

		if (no_auth_)
			*no_auth_ = creds_choices_->GetSelection() == 0;
	}
	else
		credentials_->password.impersonate(!system_user_only_permission_checks_too_->GetValue());

	return true;
}
