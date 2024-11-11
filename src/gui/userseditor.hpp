#ifndef WXFZFTPUSERSEDITOR_HPP
#define WXFZFTPUSERSEDITOR_HPP

#include <wx/panel.h>

#include "../filezilla/authentication/file_based_authenticator.hpp"

#include "userslist.hpp"

class MountTableEditor;
class wxButton;
class wxTextCtrl;
class wxCheckBox;
class RearrangedItemsPickerCtrl;
class wxSimplebook;
class FiltersEditor;
class CredentialsEditor;
class LimitsEditor;

class UsersEditor: public wxPanel
{
public:
	UsersEditor() = default;

	bool Create(wxWindow *parent,
				wxWindowID winid = wxID_ANY,
				const wxPoint& pos = wxDefaultPosition,
				const wxSize& size = wxDefaultSize,
				long style = wxTAB_TRAVERSAL | wxNO_BORDER,
				const wxString& name = wxS("wxfzftpuserseditor"));

	void SetGroups(const fz::authentication::file_based_authenticator::groups &groups);
	void SetUsers(fz::authentication::file_based_authenticator::users &users, std::string server_name);
	void SetServerPathFormat(fz::util::fs::path_format server_path_format);

private:
	bool TransferDataFromWindow() override;
	bool TransferDataToWindow() override;

	wxCheckBox *enabled_check_{};
	bool no_auth_{};
	MountTableEditor *mount_table_editor_{};
	UsersList *users_list_{};
	wxSimplebook *book_{};
	CredentialsEditor *credentials_editor_{};
	wxCheckBox *allow_token_refreshing_{};
	RearrangedItemsPickerCtrl *groups_chooser_{};
	FiltersEditor *filters_editor_{};
	LimitsEditor *limits_editor_{};
	wxTextCtrl *description_editor_{};

	fz::authentication::file_based_authenticator::users *users_{};
	std::string server_name_;

	fz::authentication::file_based_authenticator::users::value_type *current_user_{};
};

#endif // WXFZFTPUSERSEDITOR_HPP
