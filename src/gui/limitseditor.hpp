#ifndef LIMITSEDITOR_H
#define LIMITSEDITOR_H

#include <wx/panel.h>

#include "../filezilla/tvfs/limits.hpp"
#include "../filezilla/authentication/file_based_authenticator.hpp"

class IntegralEditor;

class LimitsEditor: public wxPanel
{
public:
	LimitsEditor() = default;

	bool Create(wxWindow *parent,
				wxWindowID winid = wxID_ANY,
				const wxPoint& pos = wxDefaultPosition,
				const wxSize& size = wxDefaultSize,
				long style = wxTAB_TRAVERSAL | wxNO_BORDER,
				const wxString& name = wxS("tvfslimitseditor"));

	//! Sets the limits the editor will have to display and let the user edit.
	//! It *doesn't* take ownership of the objects.
	void SetTvfsLimits(fz::tvfs::open_limits *limits);

	//! Sets the limits the editor will have to display and let the user edit.
	//! It *doesn't* take ownership of the objects.
	void SetSpeedLimits(fz::authentication::file_based_authenticator::rate_limits *limits);

	//! Sets the limits the editor will have to display and let the user edit.
	//! It *doesn't* take ownership of the objects.
	void SetSessionCountLimit(std::uint16_t *limit);

private:
	IntegralEditor *upload_shared_ctrl_{};
	IntegralEditor *download_shared_ctrl_{};
	IntegralEditor *upload_session_ctrl_{};
	IntegralEditor *download_session_ctrl_{};
	
	IntegralEditor *files_session_ctrl_{};
	IntegralEditor *directories_session_ctrl_{};
	
	IntegralEditor *session_count_limit_ctrl_{};
};

#endif // LIMITSEDITOR_H
