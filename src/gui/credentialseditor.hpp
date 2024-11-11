#ifndef CREDENTIALSEDITOR_HPP
#define CREDENTIALSEDITOR_HPP

#include "../filezilla/authentication/credentials.hpp"

#include <wx/panel.h>
#include "eventex.hpp"

class wxSimplebook;
class wxChoicebook;
class wxCheckBox;
class PasswordEditor;

class CredentialsEditor: public wxPanel
{
public:
	enum AuthMode {
		NoAuth,
		OwnPassword,
		SystemPassword
	};

	CredentialsEditor() = default;
	~CredentialsEditor() override;

	bool Create(wxWindow *parent,
				wxWindowID winid = wxID_ANY,
				const wxPoint& pos = wxDefaultPosition,
				const wxSize& size = wxDefaultSize,
				long style = wxTAB_TRAVERSAL | wxNO_BORDER,
				const wxString& name = wxS("credentialseditor"));


	/// \brief Sets the credentials to be edited.
	/// *Doesn't* take ownership of the pointer.
	void SetCredentials(bool system_user, fz::authentication::credentials *credentials, bool *no_auth, const std::string &user_name);

	bool TransferDataFromWindow() override;

	struct Event: wxEventEx<Event>
	{
		inline const static Tag ChangedMode;

		AuthMode GetMode() const
		{
			return mode_;
		}

	private:
		friend Tag;

		using wxEventEx::wxEventEx;

		Event(const Tag &tag, AuthMode mode)
			: wxEventEx(tag)
			, mode_(mode)
		{}

		AuthMode mode_;
	};

private:
	wxChoicebook *creds_choices_{};
	wxSimplebook *creds_book_{};
	PasswordEditor *password_editor_{};
	wxCheckBox *system_user_only_permission_checks_too_{};
	wxCheckBox *all_permission_checks_too_{};
	bool system_user_{};
	fz::authentication::credentials *credentials_{};
	bool *no_auth_{};
	std::string user_name_;
	std::optional<fz::authentication::any_password> password_;
};

#endif // CREDENTIALSEDITOR_HPP
