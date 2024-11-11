#include <libfilezilla/glue/wx.hpp>

#include <set>
#include <tuple>
#include <memory>

#include <wx/grid.h>
#include <wx/sizer.h>
#include <wx/button.h>
#include <wx/log.h>
#include <wx/combobox.h>
#include <wx/artprov.h>
#include <wx/dcmemory.h>
#include <wx/renderer.h>
#include <wx/headerctrl.h>
#include <wx/dcclient.h>

#include "addressinfolisteditor.hpp"
#include "locale.hpp"
#include "glue.hpp"
#include "helpers.hpp"
#include "integraleditor.hpp"
#include "textvalidatorex.hpp"


#include "../filezilla/hostaddress.hpp"

class AddressInfoListEditor::Table: public wxPanel {
public:
	struct Line
	{
		wxTextCtrl *address_ctrl{};
		IntegralEditor *port_ctrl{};
		wxChoice *proto_ctrl{};

		struct Values {
			std::string address;
			unsigned int port;
		};

		std::unique_ptr<Values> v;
	};

	class Header;

	Table(wxWindow *parent, bool edit_proto);

	void SetLists(std::initializer_list<ListInfo> lists);
	void AddRow();
	void RemoveSelectedRows();
	std::size_t GetNumberSelectedRows() const;
	std::size_t GetNumberRows() const;

	bool TransferDataFromWindow() override;

private:
	friend AddressInfoListEditor;

	void SetAddressAndPortValidator(validator_t validator);
	wxString GetMatchingAddress(const wxString &address, unsigned int port, bool any_is_equivalent);
	unsigned int GetFirstPortInRange(unsigned int min, unsigned int max);

private:
	std::vector<wxString> proto_names_;
	std::vector<Line> lines_;
	std::vector<std::function<void()>> list_clearers_;
	std::map<wxString /*proto name*/, std::function<void(fz::tcp::address_info &&)> /*list appender */> list_appenders_;

	bool edit_proto_{};

	validator_t validator_;
	wxFlexGridSizer *main_sizer_{};
	Header *header_{};
	int selected_row_{-1};

	void Append(const std::string &address, unsigned int port, const wxString &proto_name);
	void DrawGrid(wxScrolledWindow *win, wxDC &dc);
	void OnChildFocus(wxScrolledWindow *win, wxWindow *child);
};

class AddressInfoListEditor::Table::Header: public wxHeaderCtrl
{
	class Column: public wxHeaderColumn
	{
	public:
		Column(const Header &header)
			: header_(header)
		{}

		wxString GetTitle() const override
		{
			auto idx = this - header_.cols_;

			switch (idx) {
				case 0: return _S("Address");
				case 1: return _S("Port");
				case 2: return _S("Protocol");
			}

			return wxEmptyString;
		}

		wxBitmap GetBitmap() const override
		{
			return {};
		}

		int GetWidth() const override
		{
			auto idx = this - header_.cols_;
			auto table = static_cast<Table*>(header_.GetParent());

			if (auto s = table->main_sizer_) {
				if (s->GetItemCount() > 0 && idx < s->GetCols()) {
					auto width = s->GetColWidths()[std::size_t(idx)];

					width += s->GetHGap();

					return width;
				}
			}

			return idx < 3 ? 100 : table->GetClientSize().GetWidth() - 200;
		}

		int GetMinWidth() const override
		{
			return 100;
		}

		wxAlignment GetAlignment() const override
		{
			return wxALIGN_LEFT;
		}

		int GetFlags() const override
		{
			return 0;
		}

		bool IsSortKey() const override
		{
			return false;
		}

		bool IsSortOrderAscending() const override
		{
			return true;
		}

	private:
		const Header &header_;
	};

public:
	Header(Table &table)
		: wxHeaderCtrl(&table, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0)
		, cols_{*this, *this, *this}
	{
		SetColumnCount(2 + table.edit_proto_);

#if defined(__WXMSW__)
		// Windows doesn't automatically recheck the columns sizes on EVT_SIZE
		Bind(wxEVT_SIZE, [this](wxSizeEvent &ev) {
			ev.Skip();

			CallAfter([this] {
				SetColumnCount(GetColumnCount());
			});
		});
#endif
	}

	const Column &GetColumn(unsigned int idx) const override
	{
		auto table = static_cast<Table*>(GetParent());

		wxASSERT(idx <= 1u + table->edit_proto_);

		return cols_[idx];
	}

private:
	Column cols_[3];
};

bool AddressInfoListEditor::Create(wxWindow *parent, bool edit_proto)
{
	SetExtraStyle(GetExtraStyle() | wxWS_EX_PROCESS_UI_UPDATES);

	if (!wxPanel::Create(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxNO_BORDER, wxS("AddressInfoListEditor")))
		return false;

	wxHBox(this, 0) = {
		{ wxSizerFlags(1).Expand(), table_ = wxCreate<Table>(this, edit_proto) },
		{ wxSizerFlags(0).Expand(), wxVBox(this, 0) = {
			{ 0, add_button_ = new wxButton(this, wxID_ANY, _S("A&dd")) },
			{ 0, remove_button_ = new wxButton(this, wxID_ANY, _S("Re&move"), wxDefaultPosition, add_button_->GetSize()) }
		}}
	};

	add_button_->Bind(wxEVT_BUTTON, [this](auto){
		table_->SetFocus();
		table_->AddRow();

		GetParent()->Layout();
	});

	remove_button_->Bind(wxEVT_BUTTON, [this](auto){
		table_->RemoveSelectedRows();

		GetParent()->Layout();
	});

	Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent &ev) {
		ev.Skip();

		bool this_is_enabled = IsEnabled();
		add_button_->Enable(this_is_enabled);
		remove_button_->Enable(this_is_enabled && table_->GetNumberSelectedRows() > 0);
	});

	SetLists({});

	return true;
}

void AddressInfoListEditor::SetLists(std::initializer_list<ListInfo> lists)
{
	table_->SetLists(lists);

	if (lists.size() > 0) {
		add_button_->Enable();

		table_->Enable();
	}
	else {
		add_button_->Disable();
		remove_button_->Disable();
		table_->Disable();
	}
}

void AddressInfoListEditor::SetAddressAndPortValidator(validator_t validator)
{
	table_->SetAddressAndPortValidator(std::move(validator));
}

wxString AddressInfoListEditor::GetMatchingAddress(const wxString &address, unsigned int port, bool any_is_equivalent)
{
	return table_->GetMatchingAddress(address, port, any_is_equivalent);
}

unsigned int AddressInfoListEditor::GetFirstPortInRange(unsigned int min, unsigned int max)
{
	return table_->GetFirstPortInRange(min, max);
}

std::size_t AddressInfoListEditor::Table::GetNumberSelectedRows() const
{
	return !lines_.empty() && selected_row_ >= 0 ? 1 : 0;
}

std::size_t AddressInfoListEditor::Table::GetNumberRows() const
{
	return lines_.size();
}

wxString AddressInfoListEditor::Table::GetMatchingAddress(const wxString &address, unsigned int port, bool any_is_equivalent)
{
	auto it = std::find_if(lines_.begin(), lines_.end(), [&](const Line &l) {
		auto h1 = fz::hostaddress(l.v->address, fz::hostaddress::format::ipvx, l.v->port);
		auto h2 = fz::hostaddress(address.ToStdWstring(), fz::hostaddress::format::ipvx, port);

		return h1.equivalent_to(h2, any_is_equivalent) && l.v->port == port;
	});

	if (it != lines_.end())
		return fz::to_wxString(it->v->address);

	return {};
}

unsigned int AddressInfoListEditor::Table::GetFirstPortInRange(unsigned int min, unsigned int max)
{
	for (const auto &l: lines_) {
		if (min <= l.v->port && l.v->port <= max)
			return l.v->port;
	}

	return 0;
}

AddressInfoListEditor::Table::Table(wxWindow *parent, bool edit_proto)
	: wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxBORDER_THEME)
	, edit_proto_(edit_proto)
{
	struct ScrolledWindow: wxScrolledWindow
	{
		ScrolledWindow(wxWindow *parent)
			: wxScrolledWindow()
		{
			//SetBackgroundStyle(wxBG_STYLE_PAINT);
			Create(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxFULL_REPAINT_ON_RESIZE);
		}
	};

	wxVBox(this, 0, 0) = {
		wxSizerFlags(0).Expand() >>= header_ = new Header(*this),
		wxSizerFlags(1).Expand() >>= new ScrolledWindow(this) | [&](auto p) {
			p->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOX));
			p->SetScrollRate(wxDlg2Px(p, wxDefaultGap), wxDlg2Px(p, wxDefaultGap));

			wxVBox(p, wxDefaultGap/2) = main_sizer_ = wxGBox(p, 2+edit_proto_, {0, 1}) = {};

			p->Bind(wxEVT_PAINT, [p, this](wxPaintEvent &) {
				wxPaintDC dc(p);
				p->DoPrepareDC(dc);
				DrawGrid(p, dc);
			});

			p->Bind(wxEVT_CHILD_FOCUS, [p, this](wxChildFocusEvent &ev){
				ev.Skip();

				OnChildFocus(p, ev.GetWindow());
			});
		}
	};
}

void AddressInfoListEditor::Table::SetLists(std::initializer_list<ListInfo> lists) {
	Freeze();

	lines_.clear();
	proto_names_.clear();
	list_appenders_.clear();
	list_clearers_.clear();

	if (main_sizer_)
		main_sizer_->Clear(true);

	for (auto &i: lists) {
		for (auto &p: i.append_address_to_list_) {
			proto_names_.push_back(p.first);
			list_appenders_.try_emplace(p.first, p.second);
		}
	}

	for (auto &i: lists) {
		if (!i.append_address_to_list_.empty()) {
			list_clearers_.push_back(i.clear_list_);

			i.append_list_to_table_(*this);
		}
	}

	GetParent()->Layout();

	Thaw();
}

void AddressInfoListEditor::Table::DrawGrid(wxScrolledWindow *w, wxDC &dc)
{
	if (lines_.empty())
		return;

	auto line_color = wxSystemSettings::GetColour(wxSYS_COLOUR_3DSHADOW);
	auto selected_color = wxSystemSettings::GetColour(wxSYS_COLOUR_ACTIVECAPTION);

	dc.SetPen(line_color);

	// Draw the horizontal lines
	int y = 0;

	{
		int x = w->GetClientSize().x;

		int current_row = 0;
		for (auto h: main_sizer_->GetRowHeights()) {
			h += main_sizer_->GetHGap();
			y += h;

			dc.DrawLine(0, y, x, y);

			// Highlight the selected row
			if (current_row == selected_row_) {
				wxRect rect(0, y - h, x, h);

				dc.SetBrush(selected_color);
				dc.SetPen(*wxTRANSPARENT_PEN);

				dc.DrawRectangle(rect);

				dc.SetBrush(*wxTRANSPARENT_BRUSH);
				dc.SetPen(line_color);
			}

			current_row += 1;
		}
	}

	// Draw the vertical lines
	{
		int x = 0;

		for (unsigned int i = 0; i < header_->GetColumnCount() - !w->HasScrollbar(wxVERTICAL); ++i) {
			x += header_->GetColumn(i).GetWidth();

			dc.DrawLine(x-1, 0, x-1, y);
		}
	}
}

void AddressInfoListEditor::Table::OnChildFocus(wxScrolledWindow *win, wxWindow *child)
{
	if (!child)
		return;

	int new_selected_row = 0;

	for (auto &l: lines_) {
		if (l.address_ctrl == child || l.port_ctrl == child || l.proto_ctrl == child) {
			if (new_selected_row != selected_row_) {
				selected_row_ = new_selected_row;

				win->Refresh();
			}

			return;
		}

		new_selected_row += 1;
	}
}

void AddressInfoListEditor::Table::Append(const std::string &address, unsigned int port, const wxString &proto_name)
{
	wxASSERT(main_sizer_ != nullptr);

	auto p = main_sizer_->GetContainingWindow();

	Line line = {};
	line.v = std::make_unique<Line::Values>();

	line.v->address = address;
	line.v->port = port;

	wxAddToSizer(main_sizer_, p, {
		line.address_ctrl = new wxTextCtrl(p, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE),
		line.port_ctrl = wxCreate<IntegralEditor>(p, wxEmptyString, 1, wxBORDER_NONE),
	});

	line.address_ctrl->SetHint(_S("IPv4 or IPv6 address"));
	line.address_ctrl->SetValidator(TextValidatorEx(wxFILTER_NONE, &line.v->address, ip_validation));
	line.address_ctrl->SetValue(fz::to_wxString(line.v->address));

	line.port_ctrl->SetRef(line.v->port, 1, 65535);

	if (edit_proto_) {
		wxAddToSizer(main_sizer_, p, {
			line.proto_ctrl = new wxChoice(p, wxID_ANY, wxDefaultPosition, wxDefaultSize, int(proto_names_.size()), proto_names_.data(), wxBORDER_NONE)
		});

		line.proto_ctrl->SetSelection(line.proto_ctrl->FindString(proto_name));
	}

	lines_.push_back(std::move(line));

	GetParent()->Layout();
	header_->SetColumnCount(header_->GetColumnCount());
	p->Refresh();

	lines_.back().address_ctrl->SetFocus();
}

void AddressInfoListEditor::Table::AddRow()
{
	wxASSERT(list_clearers_.size() > 0);

	Append("", 0, proto_names_[0]);
}

void AddressInfoListEditor::Table::RemoveSelectedRows()
{
	if (selected_row_ < 0)
		return;

	wxASSERT(std::size_t(selected_row_) < lines_.size());

	auto l = lines_.begin() + selected_row_;

	l->address_ctrl->Destroy();
	l->port_ctrl->Destroy();

	if (l->proto_ctrl)
		l->proto_ctrl->Destroy();

	lines_.erase(l);

	GetParent()->Layout();
	header_->SetColumnCount(header_->GetColumnCount());
	main_sizer_->GetContainingWindow()->Refresh();

	// Set focus back to an existing row
	if (lines_.size() <= std::size_t(selected_row_))
		selected_row_ = int(lines_.size())-1;

	if (selected_row_ >= 0)
		lines_[std::size_t(selected_row_)].address_ctrl->SetFocus();
}

void AddressInfoListEditor::append_to_table(Table &t, const std::string &address, unsigned int port, const wxString &proto_name)
{
	t.Append(address, port, proto_name);
}

bool AddressInfoListEditor::Table::TransferDataFromWindow()
{
	if (!wxPanel::TransferDataFromWindow())
		return false;

	static const auto address_and_port_must_be_unique = [](const Line &l) {
		return _F("Duplicated address %s", fz::join_host_and_port(l.v->address, l.v->port));
	};

	auto invalid = [&](const wxString &msg, wxWindow *ctrl) {
		wxMsg::Error(msg);
		CallAfter([ctrl] {
			ctrl->SetFocus();
		});

		return false;
	};

	auto already_seen = [&, set = std::set<std::tuple<std::string, unsigned int>>{}](const Line &i) mutable {
		auto [it, inserted] = set.insert({i.v->address, i.v->port});
		return !inserted;
	};

	for (const auto &i: lines_) {
		if (validator_) {
			if (const wxString &error = validator_(fz::to_wxString(i.v->address), i.v->port); !error.empty())
				return invalid(error, i.address_ctrl);
		}

		if (already_seen(i))
			return invalid(address_and_port_must_be_unique(i), i.address_ctrl);
	}

	for (auto &clear: list_clearers_)
		clear();

	for (auto &l: lines_) {
		const auto &proto = edit_proto_ ? l.proto_ctrl->GetStringSelection() : proto_names_[0];

		if (auto it = list_appenders_.find(proto); it != list_appenders_.end()) {
			it->second({l.v->address, l.v->port});
		}
	}

	return true;
}

void AddressInfoListEditor::Table::SetAddressAndPortValidator(validator_t validator)
{
	validator_ = std::move(validator);
}

