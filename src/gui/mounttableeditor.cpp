#include <libfilezilla/glue/wx.hpp>

#include <set>

#include <wx/button.h>
#include <wx/grid.h>
#include <wx/log.h>
#include <wx/sizer.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/headerctrl.h>
#include <wx/simplebook.h>
#include <wx/richmsgdlg.h>
#include <wx/hyperlink.h>

#include "../filezilla/util/filesystem.hpp"
#include "fluidcolumnlayoutmanager.hpp"

#include "mounttableeditor.hpp"

#include "glue.hpp"
#include "helpers.hpp"
#include "locale.hpp"
#include "settings.hpp"
#include "../filezilla/tvfs/validation.hpp"

enum : int
{
	tvfs_path_col = 0,
	native_path_col = 1,

	num_of_cols
};

class MountTableEditor::Table : public wxGridTableBase
{
public:
	void SetMountTable(fz::tvfs::mount_table* mount_table);
	fz::tvfs::mount_table* GetMountTable();

	void AddRow();
	void RemoveSelectedRows();
	int GetNumberRows() override;
	bool Validate(int col = -1, int row = -1);
	void SetNativePathFormat(fz::util::fs::path_format native_path_format);
	void SetUserLogsInWithSystemCredentials(bool value);

	wxString GetRowLabelValue(int row) override;

	fz::tvfs::mount_point *GetMountPoint(int row) const;
	fz::util::fs::path_format GetNativePathFormat();

private:
	friend MountTableEditor;

	int GetNumberCols() override;
	wxString GetValue(int row, int col) override;
	void SetValue(int row, int col, const wxString& value) override;
	wxString GetColLabelValue(int col) override;

	void SetView(wxGrid*) override;

private:
	fz::tvfs::mount_table* mount_table_{};
	fz::util::fs::path_format native_path_format_{};
	bool with_system_credentials_{true};
};

class MountTableEditor::Grid : public wxGrid
{
public:
	using wxGrid::wxGrid;

	fz::tvfs::mount_point *GetCurrentMountPoint();
};


MountTableEditor::MountTableEditor() = default;

enum perms: size_t {
	enabled,
	no_mount_table,
	no_entries,
	multiple_selection
};

bool MountTableEditor::Create(wxWindow* parent, wxWindowID winid, const wxPoint& pos, const wxSize& size, long style, const wxString& name)
{
	if (!wxPanel::Create(parent, winid, pos, size, style, name))
		return false;

	wxHyperlinkCtrl *placeholders_link;

	wxGBox(this, 2, {0}, {0}, wxGBoxDefaultGap, wxALIGN_TOP) = {
		{ wxSizerFlags(1).Expand() >>= grid_ = wxCreate<Grid>(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS | wxBORDER_THEME) },
		{ wxSizerFlags(0).Expand(), wxStaticVBox(this, _S("Mount options")) = perms_ = wxCreate<wxNavigationEnabled<wxSimplebook>>(this) | [&](wxSimplebook *p) {
			wxPage(p, wxS("**enabled**")) | [&](auto p) {
				wxVBox(p, 0) = {
					{ 0, wxLabel(p, _S("Access mod&e:")) },
					{ 0, access_ = new wxChoice(p, wxID_ANY) | [&](wxChoice * p) {
						  p->Append(_S("Read only"));
						  p->Append(_S("Read + Write"));
						  p->Append(_S("Disabled"));

						  p->SetSelection(1);
					}},
					{ 0, recursive_ = new wxCheckBox(p, wxID_ANY, _S("Apply permissions to su&bdirectories")) },
					{ 0, modify_structure_ = new wxCheckBox(p, wxID_ANY, _S("Wri&table directory structure")) },
					{ 0, autocreate_ = new wxCheckBox(p, wxID_ANY, _S("&Create native directory if it does not exist")) }
				};
			};

			wxPage(p, wxS("**no mount table**"), true) |[&](auto p) {
				wxVBox(p) = {
					wxEmptySpace,
					{ wxSizerFlags(0).Align(wxALIGN_CENTER), wxLabel(p, _S("Mount table not available.")) },
					wxEmptySpace
				};
			};

			wxPage(p, wxS("**no entries**"), true) |[&](auto p) {
				wxVBox(p) = {
					wxEmptySpace,
					{ wxSizerFlags(0).Align(wxALIGN_CENTER), wxLabel(p, _S("Add a mountpoint first.")) },
					wxEmptySpace
				};
			};

			wxPage(p, wxS("**multiple selection**"), true) |[&](auto p) {
				wxVBox(p) = {
					wxEmptySpace,
					{ wxSizerFlags(0).Align(wxALIGN_CENTER), wxLabel(p, _S("Select just one mountpoint.")) },
					wxEmptySpace
				};
			};
		}},

		wxSizerFlags(0).Align(wxALIGN_CENTER_HORIZONTAL) >>= wxGBox(this, 2) = {
			{ 0, add_button_ = new wxButton(this, wxID_ANY, _S("A&dd")) },
			{ 0, remove_button_ = new wxButton(this, wxID_ANY, _S("&Remove")) }
		},

		placeholders_link = new wxHyperlinkCtrl(this, wxID_ANY, _S("You can use placeholders in native paths."), wxEmptyString, wxDefaultPosition, wxDefaultSize, wxHL_ALIGN_LEFT),
	};

	placeholders_link->Bind(wxEVT_HYPERLINK, [](wxHyperlinkEvent &) {
		wxMsg::Success(_S("The following placeholders can be used in the native paths:")).Ext(
			_S("%<home> - the absolute path to the home directory of the user. Works only if the user's authentication has been set up to use the system credentials.")
			+ wxT("\n\n") +
			_S("%<user> - the name of the user")
			+ wxT("\n\n") +
			_S("In the rare event that you don't want the placeholders to expand, prepend them with a literal '%'.\nFor example: %%<home>.")
		).Title(_S("Info about placeholders"));
	});

	grid_->UseNativeColHeader();
	if (auto* h = grid_->GetGridColHeader()) {
		h->SetFont(h->GetFont().GetBaseFont());
	}
	grid_->Bind(wxEVT_GRID_LABEL_RIGHT_CLICK, [](auto &){ /* Eat the event */ });

	if (table_ = new Table; !grid_->SetTable(table_, true, wxGrid::wxGridSelectRows))
		return false;

	grid_->SetRowLabelSize(0);
	grid_->DisableDragRowSize();
	grid_->DisableDragColMove();
	grid_->SetColLabelAlignment(wxALIGN_LEFT, wxALIGN_CENTER);
	grid_->SetDefaultCellAlignment(wxALIGN_LEFT, wxALIGN_BOTTOM);
	grid_->SetTabBehaviour(wxGrid::Tab_Leave);


	// This is needed because «wxGrid::DoSetColSize invalidates the best size, without a manually set min size, the best size influences the returned GetEffectiveMinSize()» (Tim K.)
	// This in turn means that without a minsize, wxFluidColumnLayoutManager makes the wxGrid keep enlarging.
	grid_->SetMinSize({1, 1});

	auto on_recursive_change = [this](bool v, fz::tvfs::mount_point *mp) {
		if (mp) {
			if (v) {
				modify_structure_->Enable();
				mp->recursive = modify_structure_->GetValue() ? mp->apply_permissions_recursively_and_allow_structure_modification : mp->apply_permissions_recursively;
			}
			else {
				mp->recursive = mp->do_not_apply_permissions_recursively;
				modify_structure_->SetValue(false);
				modify_structure_->Disable();
			}
		}
	};

	auto on_modify_structure_change = [this](bool v, fz::tvfs::mount_point *mp) {
		if (mp) {
			if (v)
				mp->recursive = mp->apply_permissions_recursively_and_allow_structure_modification;
			else
				mp->recursive = recursive_->GetValue() ? mp->apply_permissions_recursively : mp->do_not_apply_permissions_recursively;
		}
	};

	auto on_autocreate_change = [](bool v, fz::tvfs::mount_point *mp) {
		if (mp) {
			if (v)
				mp->flags |= fz::tvfs::mount_point::autocreate;
			else
				mp->flags &= ~fz::tvfs::mount_point::autocreate;
		}
	};

	auto on_access_change = [this, on_recursive_change, on_modify_structure_change](fz::tvfs::mount_point::access_t v, fz::tvfs::mount_point *mp) {
		if (mp)
			mp->access = v;

		switch (v) {
			case fz::tvfs::mount_point::disabled:
				recursive_->Disable();
				recursive_->SetValue(0);
				on_recursive_change(0, mp);

				modify_structure_->Disable();
				modify_structure_->SetValue(0);
				on_modify_structure_change(0, mp);

				grid_->SetReadOnly(grid_->GetGridCursorRow(), native_path_col, true);
				break;

			case fz::tvfs::mount_point::read_only:
				recursive_->Enable();

				modify_structure_->Disable();
				modify_structure_->SetValue(0);
				on_modify_structure_change(0, mp);
				grid_->SetReadOnly(grid_->GetGridCursorRow(), native_path_col, false);
				break;

			case fz::tvfs::mount_point::read_write:
				recursive_->Enable();
				modify_structure_->Enable(mp->recursive != mp->do_not_apply_permissions_recursively);
				grid_->SetReadOnly(grid_->GetGridCursorRow(), native_path_col, false);
				break;
		}
	};

	auto select_row = [this, on_access_change, on_recursive_change, on_modify_structure_change](int row) {
		if (auto mp = table_->GetMountPoint(row)) {
			if (!suspend_selection_)
				grid_->SelectRow(row);

			access_->SetSelection(mp->access);
			modify_structure_->SetValue(mp->recursive == fz::tvfs::mount_point::apply_permissions_recursively_and_allow_structure_modification);
			recursive_->SetValue(mp->recursive != fz::tvfs::mount_point::do_not_apply_permissions_recursively);
			autocreate_->SetValue(mp->flags & fz::tvfs::mount_point::autocreate);

			on_modify_structure_change(modify_structure_->GetValue(), mp);
			on_recursive_change(recursive_->GetValue(), mp);
			on_access_change(static_cast<fz::tvfs::mount_point::access_t>(access_->GetSelection()), mp);
		}
	};

	auto remove_selected_rows = [select_row, this] {
		table_->RemoveSelectedRows();
		if (table_->GetNumberRows() == 0) {
			remove_button_->Disable();
			perms_->ChangeSelection(perms::no_entries);
		}

		select_row(grid_->GetGridCursorRow());
	};

	access_->Bind(wxEVT_CHOICE, [on_access_change, this](wxCommandEvent &ev){
		ev.Skip();

		on_access_change(static_cast<fz::tvfs::mount_point::access_t>(ev.GetInt()), grid_->GetCurrentMountPoint());
	});

	recursive_->Bind(wxEVT_CHECKBOX, [this, on_recursive_change](wxCommandEvent &ev) {
		ev.Skip();

		on_recursive_change(ev.GetInt(), grid_->GetCurrentMountPoint());
	});

	modify_structure_->Bind(wxEVT_CHECKBOX, [this, on_modify_structure_change](wxCommandEvent &ev) {
		ev.Skip();

		on_modify_structure_change(ev.GetInt(), grid_->GetCurrentMountPoint());
	});

	autocreate_->Bind(wxEVT_CHECKBOX, [this, on_autocreate_change](wxCommandEvent &ev) {
		ev.Skip();

		on_autocreate_change(ev.GetInt(), grid_->GetCurrentMountPoint());
	});

	grid_->Bind(wxEVT_GRID_SELECT_CELL, [select_row](wxGridEvent &ev) {
		ev.Skip();
		select_row(ev.GetRow());
	});

	add_button_->Bind(wxEVT_BUTTON, [&](auto) {
		if (table_->Validate()) {
			table_->AddRow();
			remove_button_->Enable();
			perms_->ChangeSelection(perms::enabled);
		}
	});

	remove_button_->Bind(wxEVT_BUTTON, [remove_selected_rows](auto) {
		remove_selected_rows();
	});

	grid_->Bind(wxEVT_CHAR_HOOK, [select_row, remove_selected_rows, this](wxKeyEvent& ev) {
		auto disable_cell_editor = [this] {
			if (grid_->IsCellEditControlEnabled()) {
				auto editor = grid_->GetCellEditor(grid_->GetGridCursorRow(),
												   grid_->GetGridCursorCol());
				editor->Reset();
				editor->DecRef();
				grid_->DisableCellEditControl();
				return true;
			}

			return false;
		};

		switch (ev.GetKeyCode()) {
			case WXK_ESCAPE:
				if (disable_cell_editor())
					return;
				break;

			case WXK_DELETE:
				if (!grid_->IsCellEditControlEnabled()) {
					select_row(grid_->GetGridCursorRow());
					remove_selected_rows();
					return;
				}
		}

		ev.Skip();
	});

	grid_->Bind(wxEVT_GRID_CELL_CHANGED, [&, processing = false](wxGridEvent &ev) mutable {
		// When the grid loses focus due to a pop up (raised in table->Validate) this event is sent again.
		// We don't want to process it, then.
		if (processing)
			return;

		auto row = ev.GetRow();
		auto col = ev.GetCol();

		auto mount_table = table_->GetMountTable();

		wxCHECK2_MSG(mount_table != nullptr, ev.Veto(); return, "Mount table was not set");
		wxCHECK2_MSG(0 <= row && row < (int)mount_table->size(), ev.Veto(); return, "Not enough rows");
		wxCHECK2_MSG(0 <= col && col < num_of_cols, ev.Veto(); return, "Not enough cols");

		processing = true;
		bool is_valid = table_->Validate(col, row);
		processing = false;

		if (!is_valid) {
			last_validation_successful_ = false;
			ev.Veto();
			return;
		}

		last_validation_successful_ = true;
		ev.Skip();
	});

	grid_->Bind(wxEVT_GRID_EDITOR_HIDDEN, [select_row](wxGridEvent &ev) {
		ev.Skip();
		select_row(ev.GetRow());
	});

	grid_->Bind(wxEVT_GRID_RANGE_SELECT, [&](wxGridRangeSelectEvent &ev) {
		ev.Skip();

		const auto &selected_rows = grid_->GetSelectedRows();

		if (!selected_rows.empty() && std::find(selected_rows.begin(), selected_rows.end(), grid_->GetGridCursorRow()) == selected_rows.end()) {
			suspend_selection_ = true;
			grid_->GoToCell(selected_rows[0],  0);
			suspend_selection_ = false;
		}

		if (selected_rows.size() > 1)
			perms_->ChangeSelection(perms::multiple_selection);
		else
		if (!table_->GetMountTable())
			perms_->ChangeSelection(perms::no_mount_table);
		else
		if (!table_->GetNumberRows())
			perms_->ChangeSelection(perms::no_entries);
		else
			perms_->ChangeSelection(perms::enabled);


		suspend_selection_ = false;
	});

	Bind(wxEVT_PAINT, [this, shown = false](wxPaintEvent &ev) mutable {
		ev.Skip();

		if (!shown && Settings()->native_path_warn()) {
			auto msg = _S("Pay attention while setting up native paths.\n"
						  "Make sure they exist or select the appropriate checkbox to have the server create them for you.\n\n"
						  "For further information, consult the manual.");

			wxPushDialog<wxRichMessageDialog>(this, msg, _S("Warning: Setting Up Native Paths"), wxOK | wxCENTRE | wxICON_WARNING) | [](auto &diag) {
				diag.ShowCheckBox(_S("Don't show this message again."), false);
				diag.ShowModal();

				if (diag.IsCheckBoxChecked()) {
					Settings()->native_path_warn() = false;
					Settings().Save();
				}
			};

			shown = true;
		}
	});

	SetTable(nullptr);

	return true;
}

void MountTableEditor::SetTable(fz::tvfs::mount_table* mount_table)
{
	table_->SetMountTable(mount_table);

	if (mount_table) {
		add_button_->Enable();

		if (mount_table->empty()) {
			remove_button_->Disable();
			perms_->ChangeSelection(perms::no_entries);
		}
		else {
			remove_button_->Enable();
			perms_->ChangeSelection(perms::enabled);
			grid_->GoToCell(0, 0);
		}

		grid_->Enable();
	} else {
		add_button_->Disable();
		remove_button_->Disable();

		grid_->Disable();

		perms_->ChangeSelection(perms::no_mount_table);
	}
}

void MountTableEditor::SetNativePathFormat(fz::util::fs::path_format native_path_format)
{
	table_->SetNativePathFormat(native_path_format);
}

void MountTableEditor::SetUserLogsInWithSystemCredentials(bool value)
{
	table_->SetUserLogsInWithSystemCredentials(value);
}

bool MountTableEditor::Validate()
{
	if (!wxPanel::Validate())
		return false;

	if (!grid_->IsEnabled())
		return true;

	last_validation_successful_ = true;
	if (grid_->IsCellEditControlEnabled())
		grid_->DisableCellEditControl();

	return last_validation_successful_ && table_->Validate();
}

void MountTableEditor::Table::SetView(wxGrid* grid)
{
	auto cl = new FluidColumnLayoutManager(grid);
	cl->SetColumnWeight(tvfs_path_col, 1);
	cl->SetColumnWeight(native_path_col, 1);

	wxGridTableBase::SetView(grid);
}

int MountTableEditor::Table::GetNumberRows()
{
	if (mount_table_)
		return (int)mount_table_->size();

	return 0;
}

bool MountTableEditor::Table::Validate(int col, int row)
{
	wxCHECK_MSG(mount_table_ != nullptr, false, "Mount table was not set");
	wxCHECK_MSG((row == -1) || (0 <= row && row < (int)mount_table_->size()), false, "Not enough rows");
	wxCHECK_MSG((col == -1) || (0 <= col && col < num_of_cols), false, "Not enough cols");

	auto invalid2 = [&](std::size_t row, int col, const wxString &msg, const wxString &ext, auto &&... args) {
		wxMsg::Error(_S("Error on row number %u: %s"), (unsigned int)(row+1), msg.Lower())
			.Ext(ext, std::forward<decltype(args)>(args)...);

		GetView()->CallAfter([grid = GetView(), row, col] {
			grid->SetFocus();
			grid->SelectRow(int(row));
			grid->GoToCell(int(row), int(col));
			grid->EnableCellEditControl();
		});

		return false;
	};

	auto invalid = [&](std::size_t row, int col, const wxString &msg, auto &&... args) {
		return invalid2(row, col, msg, wxEmptyString, std::forward<decltype(args)>(args)...);
	};

	static const auto make_set = [](fz::util::fs::path_format native_path_format)
	{
		struct less_than_functor
		{
			bool case_insensitive;

			bool operator()(const std::string &lhs, const std::string &rhs) const
			{
				if (case_insensitive) {
					return fz::stricmp(fz::to_wstring_from_utf8(lhs), fz::to_wstring_from_utf8(rhs)) < 0;
				}

				return lhs < rhs;
			}
		};

		return std::set<std::string, less_than_functor>{
			less_than_functor{ native_path_format == fz::util::fs::windows_format }
		};
	};

	auto already_seen = [set = make_set(native_path_format_)](const std::string& s) mutable {
		auto [_, inserted] = set.insert(s);
		return !inserted;
	};

	static auto are_paths_empty = [](const fz::tvfs::mount_point &mp){
		return mp.tvfs_path.empty() && mp.native_path.empty();
	};

	auto handle_validation_result = [&](const fz::tvfs::validation::result &res, auto row, auto col) {
		const auto &type = col == tvfs_path_col
			? _S("virtual path")
			: _S("native path");

		InvalidPathExplanation exp(res, native_path_format_, col == tvfs_path_col, type);

		return invalid2(row, col, exp.main, exp.extra);
	};

	std::ptrdiff_t end_row = row == -1 ? std::ptrdiff_t(mount_table_->size()) : row+1;
	std::ptrdiff_t start_row = row == -1 ? 0 : row;

	for (auto row = std::size_t(start_row); row != std::size_t(end_row); ++row) {
		using namespace fz::util::fs;

		auto &mp = (*mount_table_)[row];

		if (col == -1 && are_paths_empty(mp))
			continue;

		if (col == -1 || col == tvfs_path_col) {
			if (auto res = fz::tvfs::validation::validate_tvfs_path(mp.tvfs_path); !res) {
				return handle_validation_result(res, row, tvfs_path_col);
			}

			mp.tvfs_path = unix_path(std::move(mp.tvfs_path));
		}

		if (col == -1 || col == native_path_col) {
			if (mp.access != mp.disabled) {
				using namespace fz::tvfs::placeholders;

				static const auto map = fz::tvfs::placeholders::map {
					{ user_name, fz::to_native(wxT("fictional_user")) },
					{ only_at_beginning(home_dir), with_system_credentials_
						? native_path_format_ == unix_format
							? fz::to_native(_S("/fictional/absolute_path"))
							: fz::to_native(_S("X:\\fictional\\absolute_path"))
						: make_invalid_value(fz::to_native(
							_F("Placeholder %%%s can be used only for users whose authentication is set to \"Use system credentials to log in\"", fz::tvfs::placeholders::home_dir)
						))
					},
					{ home_dir, make_invalid_value(fz::to_native(
						_F("Placeholder %%%s can be used only at the beginning of the native path", fz::tvfs::placeholders::home_dir)
					))},
					{ anything(fzT("%p")), make_invalid_value(fz::to_native(_S("%%<%p> is not a recognized placeholder")))}
				};

				auto native_path = substitute_placeholders(mp.native_path, map);
				if (auto res = fz::tvfs::validation::validate_native_path(native_path, native_path_format_); !res) {
					return handle_validation_result(res, row, native_path_col);
				}

				mp.native_path = native_path_format_ == unix_format
					? unix_native_path(std::move(mp.native_path)).str()
					: windows_native_path(std::move(mp.native_path)).str();
			}
		}

		if (col == -1) {
			if (already_seen(mp.tvfs_path)) {
				return invalid(row, tvfs_path_col, _S("Virtual paths must be unique"));
			}
		}
	}

	// Remove empty rows
	if (col == -1) {
		mount_table_->erase(
			std::remove_if(mount_table_->begin() + start_row, mount_table_->begin() + end_row, [row = start_row, this](const auto &mp) mutable {
				auto empty = are_paths_empty(mp);

				if (empty) {
					wxGridTableMessage msg(this, wxGRIDTABLE_NOTIFY_ROWS_DELETED, row, 1);
					GetView()->ProcessTableMessage(msg);
				}

				row += 1;
				return empty;
			}),
			mount_table_->begin() + end_row
		);
	}

	return true;
}

void MountTableEditor::Table::SetNativePathFormat(
	fz::util::fs::path_format native_path_format)
{
	native_path_format_ = native_path_format;
}

void MountTableEditor::Table::SetUserLogsInWithSystemCredentials(bool value)
{
	with_system_credentials_ = value;
}


wxString MountTableEditor::Table::GetRowLabelValue(int)
{
	return wxEmptyString;
}

fz::tvfs::mount_point *MountTableEditor::Table::GetMountPoint(int row) const
{
	if (!mount_table_ || (row < 0 || std::size_t(row) >= mount_table_->size()))
		return nullptr;

	return &(*mount_table_)[std::size_t(row)];
}

fz::util::fs::path_format MountTableEditor::Table::GetNativePathFormat()
{
	return native_path_format_;
}

int MountTableEditor::Table::GetNumberCols()
{
	return num_of_cols;
}

wxString MountTableEditor::Table::GetValue(int row, int col)
{
	wxASSERT(mount_table_ != nullptr);

	if (row < 0 || col < 0)
		return wxEmptyString;

	wxCHECK_MSG(0 <= row && row < (int)mount_table_->size(), wxEmptyString, "Not enough rows");
	wxCHECK_MSG(0 <= col && col < num_of_cols, wxEmptyString, "Not enough cols");

	auto& mount_point = (*mount_table_)[(size_t)row];

	switch (col) {
		case tvfs_path_col:
			return fz::to_wxString(mount_point.tvfs_path);
		case native_path_col:
			return fz::to_wxString(mount_point.native_path);
	}

	return wxEmptyString;
}

void MountTableEditor::Table::SetValue(int row, int col, const wxString& value)
{
	if (row < 0 || col < 0)
		return;

	wxCHECK_RET(mount_table_ != nullptr, "Mount table was not set");
	wxCHECK_RET(0 <= row && row < (int)mount_table_->size(), "Not enough rows");
	wxCHECK_RET(0 <= col && col < num_of_cols, "Not enough cols");

	auto& mount_point = (*mount_table_)[(std::size_t)row];

	switch (col) {
		case tvfs_path_col:
			mount_point.tvfs_path = fz::to_utf8(value);
			break;
		case native_path_col:
			mount_point.native_path = fz::to_native(value);
			break;
	}
}

wxString MountTableEditor::Table::GetColLabelValue(int col)
{
	wxCHECK_MSG((col >= 0 && col < num_of_cols), wxEmptyString, wxS("invalid column index in MountTable::Table"));

	switch (col) {
		case tvfs_path_col: return _S("Virtual path");
		case native_path_col: return _S("Native path");
	}

	return wxEmptyString;
}

void MountTableEditor::Table::SetMountTable(fz::tvfs::mount_table* mount_table)
{
	auto old_rows = GetView()->GetNumberRows();
	mount_table_ = mount_table;

	wxGridUpdateLocker updater(GetView());

	if (auto delta_rows = GetNumberRows() - old_rows; delta_rows > 0) {
		wxGridTableMessage msg(this, wxGRIDTABLE_NOTIFY_ROWS_APPENDED, delta_rows);
		GetView()->ProcessTableMessage(msg);
	}
	else
	if (delta_rows < 0) {
		wxGridTableMessage msg(this,
							   wxGRIDTABLE_NOTIFY_ROWS_DELETED,
							   old_rows + delta_rows,
							   -delta_rows);

		GetView()->ProcessTableMessage(msg);
	}
}

fz::tvfs::mount_table *MountTableEditor::Table::GetMountTable()
{
	return mount_table_;
}

void MountTableEditor::Table::AddRow()
{
	wxGridUpdateLocker updater(GetView());

	mount_table_->emplace_back();

	wxGridTableMessage msg(this, wxGRIDTABLE_NOTIFY_ROWS_APPENDED, 1);
	GetView()->ProcessTableMessage(msg);

	GetView()->CallAfter([this] {
		GetView()->SetFocus();
		GetView()->SelectRow(int(mount_table_->size() - 1));
		GetView()->GoToCell(int(mount_table_->size() - 1), 0);
		GetView()->EnableCellEditControl();
	});
}

void MountTableEditor::Table::RemoveSelectedRows()
{
	wxGridUpdateLocker updater(GetView());

	for (auto& row : GetView()->GetSelectedRows()) {
		mount_table_->erase(std::next(mount_table_->begin(), row));
		wxGridTableMessage msg(this, wxGRIDTABLE_NOTIFY_ROWS_DELETED, row, 1);
		GetView()->ProcessTableMessage(msg);
	}
}

fz::tvfs::mount_point *MountTableEditor::Grid::GetCurrentMountPoint()
{
	if (auto table = dynamic_cast<Table *>(GetTable()))
		return table->GetMountPoint(GetGridCursorRow());

	return nullptr;
}
