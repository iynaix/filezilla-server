#include <libfilezilla/glue/wx.hpp>
#include <libfilezilla/util.hpp>

#include <wx/listctrl.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/splitter.h>
#include <wx/valtext.h>
#include <wx/checkbox.h>
#include <wx/msgdlg.h>
#include <wx/combo.h>
#include <wx/checklst.h>
#include <wx/rearrangectrl.h>
#include <wx/simplebook.h>
#include <wx/layout.h>

#include "rearrangeditemspickerctrl.hpp"
#include "groupseditor.hpp"
#include "mounttableeditor.hpp"
#include "groupslist.hpp"
#include "filterseditor.hpp"
#include "passwordeditor.hpp"
#include "limitseditor.hpp"
#include "locale.hpp"
#include "integraleditor.hpp"
#include "glue.hpp"

#include "helpers.hpp"

namespace {

enum page: size_t {
	enabled  = 0,
	disabled = 1
};

}

bool GroupsEditor::Create(wxWindow *parent, wxWindowID winid, const wxPoint &pos, const wxSize &size, long style, const wxString &name)
{
	if (!wxPanel::Create(parent, winid, pos, size, style, name))
		return false;

	wxHBox(this, 0) = {
		{ wxSizerFlags(0).Expand(), groups_list_ = new GroupsList(this) },
		{ wxSizerFlags(1).Expand(), book_ = wxCreate<wxNavigationEnabled<wxSimplebook>>(this) | [&](auto p) {
			wxPage<wxNotebook>(p, wxS("*Enabled*"), false, wxID_ANY) |[&](auto p) {
				wxPage(p,  _S("General"), true) |[&](auto p) {
					wxVBox(p) = {
						{ 0, wxLabel(p, _S("Mount p&oints:")) },
						{ 3, mount_table_editor_ = wxCreate<MountTableEditor>(p) },

						{ 0, wxLabel(p, _S("Descr&iption:")) },
						{ 1, description_editor_ = new wxTextCtrl(p, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_RICH2) }
					};
				};


				wxPage(p, _S("Filters")) |[&](auto p) {
					wxVBox(p) = filters_editor_ = wxCreate<FiltersEditor>(p);
				};

				wxPage(p, _S("Limits")) |[&](auto p) {
					wxVBox(p) = {
						limits_editor_ = wxCreate<LimitsEditor>(p),
					};
				};
			};

			wxPage(p, wxS("*Disabled*"), true) |[&](auto p) {
				wxVBox(p) = {
					wxEmptySpace,
					{ wxSizerFlags(0).Align(wxALIGN_CENTER), wxLabel(p, _S("Select or add a group in the list to the left")) },
					wxEmptySpace
				};
			};
		}}
	};

	groups_list_->Bind(GroupsList::Event::AboutToDeselectGroup, [this](auto &ev){
		if (!Validate() || !TransferDataFromWindow())
			ev.Veto();
		else
			ev.Skip();
	});

	groups_list_->Bind(GroupsList::Event::SelectedGroup, [this](auto &ev){
		ev.Skip();

		TransferDataToWindow();
	});

	return true;
}

void GroupsEditor::SetServerPathFormat(fz::util::fs::path_format server_path_format)
{
	mount_table_editor_->SetNativePathFormat(server_path_format);
}

bool GroupsEditor::TransferDataFromWindow()
{
	if (!wxPanel::TransferDataFromWindow())
		return false;

	current_group_ = groups_list_->GetSelectedGroup();

	if (current_group_) {
		current_group_->second.description = fz::to_utf8(description_editor_->GetValue());
	}

	return true;
}

void GroupsEditor::SetGroups(fz::authentication::file_based_authenticator::groups &groups)
{
	groups_ = &groups;
	groups_list_->SetGroups(groups);
}

bool GroupsEditor::TransferDataToWindow()
{
	if (wxPanel::TransferDataToWindow()) {
		current_group_ = groups_list_->GetSelectedGroup();

		if (!current_group_) {
			book_->ChangeSelection(page::disabled);

			mount_table_editor_->SetTable(nullptr);
			filters_editor_->SetIps(nullptr, nullptr);
			limits_editor_->SetSpeedLimits(nullptr);
			limits_editor_->SetTvfsLimits(nullptr);
			limits_editor_->SetSessionCountLimit(nullptr);
			description_editor_->ChangeValue(wxEmptyString);
		}
		else {
			book_->ChangeSelection(page::enabled);

			mount_table_editor_->SetTable(&current_group_->second.mount_table);
			filters_editor_->SetIps(&current_group_->second.disallowed_ips, &current_group_->second.allowed_ips);
			limits_editor_->SetSpeedLimits(&current_group_->second.rate_limits);
			limits_editor_->SetTvfsLimits(&current_group_->second.session_open_limits);
			limits_editor_->SetSessionCountLimit(&current_group_->second.session_count_limit);
			description_editor_->ChangeValue(fz::to_wxString(current_group_->second.description));
		}

		return true;
	}

	return false;
}
