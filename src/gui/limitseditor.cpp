#include <libfilezilla/glue/wx.hpp>

#include <wx/statline.h>
#include <wx/checkbox.h>

#include "limitseditor.hpp"
#include "integraleditor.hpp"

#include "locale.hpp"

#include "helpers.hpp"

#include "../../src/filezilla/util/traits.hpp"

template <typename Unlimited>
static wxSizer *editor_sizer(wxWindow *parent, IntegralEditor *&editor, const wxString &title, Unlimited unlimited, const wxString &unit = wxEmptyString,
							 std::uintmax_t scale = 1)
{
	wxCheckBox *enabler;

	wxSizer *ret = wxVBox(parent) = {
		{ 0, enabler = new wxCheckBox(parent, wxID_ANY, title) },
		{ 0, editor = wxCreate<IntegralEditor>(parent, unit, scale) }
	};
	
	editor->Bind(IntegralEditor::Event::Changed, [editor, enabler, unlimited](IntegralEditor::Event &) {
		fz::util::underlying_t<Unlimited> cur_limit;

		bool enabled = editor->Get(cur_limit) && cur_limit != unlimited;

		editor->Enable(enabled);
		enabler->SetValue(enabled);
	});

	enabler->Bind(wxEVT_CHECKBOX, [editor, unlimited](wxCommandEvent &ev) {
		if (ev.GetInt()) {
			editor->SetToMin();
			fz::util::underlying_t<Unlimited> cur_limit;

			if (editor->Get(cur_limit) && cur_limit == unlimited)
				editor->Increment();
		}
		else
			editor->SetValue(unlimited);
	});

	return ret;
}

template <typename Unlimited>
static wxSizer *couple_editor_sizer(wxWindow *parent, IntegralEditor *&upload_editor, IntegralEditor *&download_editor, const wxString &title, const wxString &upload_title, const wxString &download_title, Unlimited unlimited, const wxString &unit = wxEmptyString,
									std::uintmax_t scale = 1)
{
	return wxStaticHBox(parent, title, 0) = {
		{ 1, editor_sizer(parent, upload_editor, upload_title, unlimited, unit, scale) },
		{ 1, editor_sizer(parent, download_editor, download_title, unlimited, unit, scale) }
	};
}

bool LimitsEditor::Create(wxWindow *parent, wxWindowID winid, const wxPoint &pos, const wxSize &size, long style, const wxString &name)
{
	if (!wxPanel::Create(parent, winid, pos, size, style, name))
		return false;

	wxVBox(this, 0) = {
		couple_editor_sizer(this, upload_shared_ctrl_, download_shared_ctrl_, _S("Speed limits shared by all sessions"), _S("Do&wnload from server:"), _S("Upload to ser&ver:"), fz::rate::unlimited, _S("KiB/s"), 1024),
		couple_editor_sizer(this, upload_session_ctrl_, download_session_ctrl_, _S("Speed limits specific to each session"), _S("D&ownload from server:"), _S("U&pload to server:"), fz::rate::unlimited, _S("KiB/s"), 1024),

		couple_editor_sizer(this, files_session_ctrl_, directories_session_ctrl_, _S("Filesystem limits specific to each session"), _S("&Files:"), _S("Di&rectories:"), fz::tvfs::open_limits::unlimited),

		editor_sizer(this, session_count_limit_ctrl_, _S("Concurrent sessions limi&t:"), fz::tvfs::open_limits::unlimited),
	};

	SetSpeedLimits(nullptr);
	SetTvfsLimits(nullptr);
	SetSessionCountLimit(nullptr);

	return true;
}

void LimitsEditor::SetTvfsLimits(fz::tvfs::open_limits *limits)
{
	if (!limits) {
		files_session_ctrl_->SetRef(nullptr);
		directories_session_ctrl_->SetRef(nullptr);
	}
	else {
		const auto mapping = std::pair{fz::tvfs::open_limits::unlimited, _S("Unlimited")};

		files_session_ctrl_->SetRef(limits->files)->set_mapping({mapping});
		directories_session_ctrl_->SetRef(limits->directories)->set_mapping({mapping});
	}
}

void LimitsEditor::SetSpeedLimits(fz::authentication::file_based_authenticator::rate_limits *limits)
{
	if (!limits) {
		download_shared_ctrl_->SetRef(nullptr);
		upload_shared_ctrl_->SetRef(nullptr);
		download_session_ctrl_->SetRef(nullptr);
		upload_session_ctrl_->SetRef(nullptr);
	}
	else {
		const auto mapping = std::pair{fz::rate::unlimited, _S("Unlimited")};

		download_shared_ctrl_->SetRef(limits->inbound, 1024)->set_mapping({mapping});
		upload_shared_ctrl_->SetRef(limits->outbound, 1024)->set_mapping({mapping});
		download_session_ctrl_->SetRef(limits->session_inbound, 1024)->set_mapping({mapping});
		upload_session_ctrl_->SetRef(limits->session_outbound, 1024)->set_mapping({mapping});
	}
}

void LimitsEditor::SetSessionCountLimit(std::uint16_t *limit)
{
	if (!limit) {
		session_count_limit_ctrl_->SetRef(nullptr);
	}
	else {
		const auto mapping = std::pair{std::uint16_t(0), _S("Unlimited")};

		session_count_limit_ctrl_->SetRef(*limit)->set_mapping({mapping});
	}
}

