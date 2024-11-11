#ifndef WXFZFTPGROUPSEDITOR_HPP
#define WXFZFTPGROUPSEDITOR_HPP

#include <wx/panel.h>

#include "../filezilla/authentication/file_based_authenticator.hpp"

#include "groupslist.hpp"

class MountTableEditor;
class wxButton;
class wxTextCtrl;
class wxCheckBox;
class RearrangedItemsPickerCtrl;
class wxSimplebook;
class FiltersEditor;
class LimitsEditor;

class GroupsEditor: public wxPanel
{
public:
	GroupsEditor() = default;

	bool Create(wxWindow *parent,
				wxWindowID winid = wxID_ANY,
				const wxPoint& pos = wxDefaultPosition,
				const wxSize& size = wxDefaultSize,
				long style = wxTAB_TRAVERSAL | wxNO_BORDER,
				const wxString& name = wxS("groupseditor"));

	void SetGroups(fz::authentication::file_based_authenticator::groups &groups);
	void SetServerPathFormat(fz::util::fs::path_format server_path_format);

private:
	bool TransferDataFromWindow() override;
	bool TransferDataToWindow() override;

	wxSimplebook *book_{};

	GroupsList *groups_list_{};
	MountTableEditor *mount_table_editor_{};
	FiltersEditor *filters_editor_{};
	LimitsEditor *limits_editor_{};
	wxTextCtrl *description_editor_{};

	fz::authentication::file_based_authenticator::groups *groups_{};
	fz::authentication::file_based_authenticator::groups::value_type *current_group_{};
};

#endif // WXFZFTPGROUPSEDITOR_HPP
