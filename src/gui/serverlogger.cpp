#include <libfilezilla/glue/wx.hpp>

#include <glue.hpp>

#include <wx/menu.h>
#include <wx/listctrl.h>
#include <wx/timer.h>
#include <wx/artprov.h>

#include "serverlogger.hpp"
#include "locale.hpp"
#include "helpers.hpp"
#include "listctrlex.hpp"

#include "../filezilla/util/filesystem.hpp"
#include "../filezilla/logger/type.hpp"
#include "../filezilla/util/bits.hpp"
#include "../filezilla/string.hpp"
#include "../filezilla/transformed_view.hpp"

class ServerLogger::List: public wxListCtrlEx
{
	int date_col_{-1};
	int info_col_{-1};
	int type_col_{-1};
	int message_col_{-1};

public:
	List(ServerLogger &parent)
		: wxListCtrlEx(&parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxLC_VIRTUAL | wxLC_REPORT)
		, parent_(parent)
	{
		error_attr_.SetTextColour(fx::colors::error);
		command_attr_.SetTextColour(fx::colors::command);
		reply_attr_.SetTextColour(fx::colors::reply);
		warning_attr_.SetTextColour(fx::colors::warning);
		trace_attr_.SetTextColour(fx::colors::trace);
		private_attr_.SetTextColour(wxColour(0, 128, 128));

		SetName(fz::sprintf(wxS("%s::List"), parent.GetName()));

		auto cl = new FluidColumnLayoutManager(this, true);

		if (parent.enabled_cols_ & parent.Date) {
			date_col_ = AppendColumn(_S("Date/Time"));
			wxASSERT(date_col_ != -1);

			// Hardcode default sizes that work well with the default Windows font.
			SetColumnWidth(date_col_, wxDlg2Px(this, 64));
		}

		if (parent.enabled_cols_ & parent.Info) {
			info_col_ = AppendColumn(_S("Info"));
			wxASSERT(info_col_ != -1);

			// Hardcode default sizes that work well with the default Windows font.
			SetColumnWidth(info_col_, wxDlg2Px(this, 75));
		}

		if (parent.enabled_cols_ & parent.Type) {
			type_col_ = AppendColumn(_S("Type"));
			wxASSERT(type_col_ != -1);
		}

		message_col_ = AppendColumn(_S("Message"));
		wxASSERT(message_col_ != -1);

		cl->SetColumnWeight(message_col_, 1);
	}

	auto GetItems(bool only_selected)
	{
		fz::scoped_lock lock(parent_.mutex_);

		return wxListCtrlEx::GetItems({date_col_, info_col_, type_col_, message_col_}, only_selected);
	}

	std::vector<Line::Session> GetSelectedSessions()
	{
		std::vector<Line::Session> ret;

		fz::scoped_lock lock(parent_.mutex_);

		for (long item = -1;;) {
			item = GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
			if (item < 0)
				break;

			if (std::size_t(item) > parent_.lines_.size())
				continue;

			if (auto &s = parent_.lines_[(std::size_t(item)+parent_.lines_begin_idx_) % parent_.max_number_lines_].session)
				ret.push_back(s);
		}

		ret.erase(std::unique(ret.begin(), ret.end()), ret.end());

		return ret;
	}

	void SelectAll() override
	{
		fz::scoped_lock lock(parent_.mutex_);
		wxListCtrlEx::SelectAll();
	}

private:
	long GetUpdatedItemCount() const override
	{
		fz::scoped_lock lock(parent_.mutex_);

		return static_cast<long>(parent_.lines_.size());
	}

	wxString OnGetItemText(long row, long column) const override
	{
		static const auto type2str = [](fz::logmsg::type type) {
			using namespace fz;

			switch (type) {
				case logmsg::status:
					return _S("Status");

				case logmsg::error:
					return _S("Error");

				case logmsg::command:
					return _S("Command");

				case logmsg::reply:
					return _S("Response");

				case logmsg::warning:
					return _S("Warning");

				case logmsg::debug_warning:
				case logmsg::debug_info:
				case logmsg::debug_verbose:
				case logmsg::debug_debug:
					return _S("Trace");

				case logmsg::custom32:
					break;
			}

			if (type) {
				return _F("Private (%d)", util::log2_floor(type) - util::log2_floor(logmsg::custom1) + 1);
			}

			return wxString();
		};

		fz::scoped_lock lock(parent_.mutex_);

		if (row >= 0 && std::size_t(row) < parent_.lines_.size()) {
			auto &l = parent_.lines_[(std::size_t(row)+parent_.lines_begin_idx_) % parent_.max_number_lines_];

			if (column == date_col_) {
				return fz::to_wxString(l.datetime);
			}

			if (column == type_col_) {
				return type2str(l.type);
			}

			if (column == info_col_) {
				return l.info;
			}

			if (column == message_col_) {
				return l.message;
			}
		}

		return wxEmptyString;
	}

	int OnGetItemColumnImage(long /*row*/, long /*column*/) const override
	{
		//FIXME: return an image for the type column?
		return -1;
	}

	wxListItemAttr *OnGetItemAttr(long row) const override
	{
		fz::scoped_lock lock(parent_.mutex_);

		if (row >= 0 && std::size_t(row) < parent_.lines_.size()) {
			auto &l = parent_.lines_[(std::size_t(row)+parent_.lines_begin_idx_) % parent_.max_number_lines_];

			using namespace fz;

			switch (l.type) {
				case logmsg::error:
					return &error_attr_;

				case logmsg::command:
					return &command_attr_;

				case logmsg::reply:
					return &reply_attr_;

				case logmsg::warning:
					return &warning_attr_;

				case logmsg::debug_warning:
				case logmsg::debug_info:
				case logmsg::debug_verbose:
				case logmsg::debug_debug:
					return &trace_attr_;

				case logmsg::status:
					return nullptr;

				case logmsg::custom32:
					break;
			}
		}

		return &private_attr_;
	}

	ServerLogger &parent_;

	mutable wxListItemAttr command_attr_;
	mutable wxListItemAttr error_attr_;
	mutable wxListItemAttr reply_attr_;
	mutable wxListItemAttr warning_attr_;
	mutable wxListItemAttr trace_attr_;
	mutable wxListItemAttr private_attr_;
};


ServerLogger::ServerLogger()
{
}

ServerLogger::ServerLogger(wxWindow *parent, int enabled_cols, const wxString& name)
{
	Create(parent, enabled_cols, name);
}

bool ServerLogger::Create(wxWindow *parent, int enabled_cols, const wxString& name)
{
	if (!wxPanel::Create(parent, nullID, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxBORDER_NONE, name)) {
		return false;
	}

	enabled_cols_ = enabled_cols;

	wxVBox(this, 0) = list_ = wxCreate<List>(*this);

	SetMaxNumberLines(10'000);

	Bind(wxEVT_CONTEXT_MENU, [&](const wxContextMenuEvent &){
		std::vector<std::array<wxString, 4>> selected_lines;

		bool only_selected = list_->GetSelectedItemCount() > 0;

		wxMenu menu;

		enum { clear = wxID_CLEAR, select_all = wxID_SELECTALL, copy_as_csv = wxID_HIGHEST, copy_as_html, copy_as_log, ban_ip, copy_ip, copy_username };

		static const auto append = [](wxMenu &menu, auto id, auto text) {
			auto item = new wxMenuItem(&menu, id, text);
			menu.Append(item);
		};

		if (only_selected) {
			append(menu, copy_as_csv, _S("Copy selected lines as CSV"));
			append(menu, copy_as_html, _S("Copy selected lines as HTML"));
			append(menu, copy_as_log, _S("Copy selected lines as plaintext"));
		}
		else {
			append(menu, copy_as_csv, _S("Copy all lines as CSV"));
			append(menu, copy_as_html, _S("Copy all lines as HTML"));
			append(menu, copy_as_log, _S("Copy all lines as plaintext"));
		}

		menu.AppendSeparator();

		append(menu, clear, _S("Clear log"));
		append(menu, select_all, _S("Select all"));

		if (only_selected) {
			if (auto selected = list_->GetSelectedSessions(); !selected.empty()) {
				menu.AppendSeparator();
				append(menu, ban_ip, _S("Ban IPs"));
				append(menu, copy_ip, _S("Copy IPs"));
				append(menu, copy_username, _S("Copy user names"));
				menu.Bind(wxEVT_MENU, [this, selected = std::move(selected)](wxCommandEvent &ev) mutable {
					if (ev.GetId() == ban_ip) {
						Event::IPsNeedToBeBanned.Process(this, this, std::move(selected));
					}
					else
					if (ev.GetId() == copy_ip) {
						wxString to_copy;

						for (auto &i: selected) {
							to_copy.append(i.host).append(wxT("\n"));
						}

						wxCopyToClipboard(fz::to_wxString(to_copy));
					}
					else
					if (ev.GetId() == copy_username) {
						wxString to_copy;

						for (auto &i: selected) {
							to_copy.append(i.username).append(wxT("\n"));
						}

						wxCopyToClipboard(fz::to_wxString(to_copy));
					}
					else {
						ev.Skip();
					}
				});
			}
		}

		menu.Bind(wxEVT_MENU, [&](auto) {
			CopyToClipboardAsCsv(only_selected);
		}, copy_as_csv);

		menu.Bind(wxEVT_MENU, [&](auto) {
			CopyToClipboardAsHtml(only_selected);
		}, copy_as_html);

		menu.Bind(wxEVT_MENU, [&](auto) {
			CopyToClipboardAsLog(only_selected);
		}, copy_as_log);

		menu.Bind(wxEVT_MENU, [&](auto) {
			Clear();
		}, clear);

		menu.Bind(wxEVT_MENU, [&](auto) {
			list_->SelectAll();
		}, select_all);

		PopupMenu(&menu);
	});

	Bind(wxEVT_CHAR_HOOK, [&](wxKeyEvent &ev){
		ev.Skip();

		if (ev.ControlDown()) {
			if (ev.GetKeyCode() == 'C')
				CopyToClipboardAsLog(list_->GetSelectedItemCount() > 0);
		}
	});

	return true;
}

void ServerLogger::SetMaxNumberLines(std::size_t max)
{
	fz::scoped_lock lock(mutex_);

	max_number_lines_ = max;

	if (lines_.size() > max_number_lines_) {
		lines_.resize(max_number_lines_);
		list_->DelayedUpdate();
	}
	else {
		lines_.reserve(max_number_lines_);
	}
}

void ServerLogger::Log(Line &&line, bool remove_cntrl) {
	fz::scoped_lock lock(mutex_);

	if (remove_cntrl)
		fz::remove_ctrl_chars(line.message);

	if (lines_.size() == max_number_lines_) {
		if (max_number_lines_ > 0) {
			lines_[lines_begin_idx_++] = std::move(line);
			lines_begin_idx_ %= max_number_lines_;
		}
	}
	else {
		lines_.emplace_back(std::move(line));
	}

	list_->DelayedUpdate();
}

void ServerLogger::Clear()
{
	fz::scoped_lock lock(mutex_);
	lines_.clear();
	lines_begin_idx_ = 0;
	list_->DelayedUpdate();
}

void ServerLogger::CopyToClipboardAsCsv(bool only_selected)
{
	auto lines = list_->GetItems(only_selected);

	auto to_copy = fz::join(fz::transformed_view(lines, [](const auto &l) {
		return fz::join(fz::transformed_view(l, [](const auto &v) {
			return fz::quote(fz::escaped(fz::to_wstring(v), L"\""));
		}), L",");
	}), L"\n");

	wxCopyToClipboard(fz::to_wxString(to_copy));
}

void ServerLogger::CopyToClipboardAsHtml(bool only_selected)
{
	wxString ret = wxT("<!doctype html>\n");

	ret += wxT("<html><body><table>\n");

	const auto &rows = list_->GetItems(only_selected);
	if (auto row = rows.cbegin(); row != rows.cend()) {
		ret += wxT("\t<tr>");

		for (const auto &e: *row) {
			ret.append(wxT("<th style=\"text-align:left\">")).append(e).append(wxT("</th>"));
		}

		ret += wxT("</tr>\n");

		for (++row; row != rows.cend(); ++row) {
			ret += wxT("\t<tr>");

			for (const auto &e: *row) {
				ret.append(wxT("<td>")).append(e).append(wxT("</td>"));
			}

			ret += wxT("</tr>\n");
		}
	}

	ret += wxT("</table><body></html>\n");

	wxCopyToClipboard(ret, true);
}

void ServerLogger::CopyToClipboardAsLog(bool only_selected)
{
	auto lines = list_->GetItems(only_selected);

	auto to_copy = fz::join(fz::transformed_view(lines, [this](const auto &l) {
		std::wstring ret;
		unsigned idx = 0;

		if (enabled_cols_ & ListCol::Date) {
			ret += fz::sprintf(L"<%s> ", l[idx++]);
		}

		if (enabled_cols_ & ListCol::Info) {
			ret += fz::sprintf(L"%s ", l[idx++]);
		}

		if (enabled_cols_ & ListCol::Type) {
			ret += fz::sprintf(L"[%s] ", l[idx++]);
		}

		ret += fz::sprintf(L"%s", l[idx++]);

		return ret;
	}), L"\n");

	wxCopyToClipboard(fz::to_wxString(to_copy));
}

void ServerLogger::do_log(fz::logmsg::type t, const fz::logger::modularized::info_list &info_list, std::wstring &&msg)
{
	Log({fz::datetime::now(), t, fz::to_wxString(info_list.as_string), fz::to_wxString(msg), {}});
}

ServerLogger::Line::Session::operator bool() const
{
	return id && !host.empty();
}

bool ServerLogger::Line::Session::operator ==(const Session &rhs) const
{
	return std::tie(id, host, address_family) == std::tie(rhs.id, rhs.host, rhs.address_family);
}
