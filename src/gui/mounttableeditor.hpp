#ifndef WXFZFTPMOUNTTABLE_HPP
#define WXFZFTPMOUNTTABLE_HPP

#include <wx/panel.h>

#include "../filezilla/tvfs/mount.hpp"
#include "../filezilla/util/filesystem.hpp"

class wxButton;
class wxCheckBox;
class wxChoice;
class wxSimplebook;

class MountTableEditor: public wxPanel
{
public:
	MountTableEditor();

	bool Create(wxWindow *parent,
				wxWindowID winid = wxID_ANY,
				const wxPoint& pos = wxDefaultPosition,
				const wxSize& size = wxDefaultSize,
				long style = wxTAB_TRAVERSAL | wxNO_BORDER,
				const wxString& name = wxS("MountTableEditor"));

	//! Sets the mount table the editor will have to display and let the user edit.
	//! It *doesn't* take ownership of the mount table.
	void SetTable(fz::tvfs::mount_table *mount_table);
	void SetNativePathFormat(fz::util::fs::path_format native_path_format);
	void SetUserLogsInWithSystemCredentials(bool value);

	bool Validate() override;

private:
	class Grid;
	class Table;

	Grid *grid_{};
	Table *table_{};
	wxChoice *access_{};
	wxCheckBox *recursive_{};
	wxCheckBox *modify_structure_{};
	wxCheckBox *autocreate_{};
	wxButton *remove_button_{};
	wxButton *add_button_{};
	wxSimplebook *perms_{};
	bool suspend_selection_{};
	bool last_validation_successful_{};
};

#endif // WXFZFTPMOUNTTABLE_HPP
