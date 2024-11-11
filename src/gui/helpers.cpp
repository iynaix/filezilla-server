#include <libfilezilla/glue/wx.hpp>
#include <libfilezilla/util.hpp>
#include <libfilezilla/hostname_lookup.hpp>

#include <optional>

#include <wx/treebook.h>
#include <wx/notebook.h>
#include <wx/choicebk.h>
#include <wx/msgdlg.h>
#include <wx/clipbrd.h>
#include <wx/app.h>
#include <wx/hyperlink.h>
#include <wx/button.h>
#include <wx/filedlg.h>
#include <wx/checkbox.h>
#include <wx/dcclient.h>
#include <wx/statline.h>
#include <wx/evtloop.h>

#include "helpers.hpp"
#include "locale.hpp"
#include "settings.hpp"

#include "../filezilla/util/io.hpp"
#include "../filezilla/util/overload.hpp"
#include "../filezilla/logger/type.hpp"
#include "../filezilla/util/dispatcher.hpp"

static constexpr std::size_t maxNumberOfCharactersInTextCtrl = 1*1024*1024;

static void maybeLimitCharactersAmountInTextCtrl(wxSizer *)
{}

static void maybeLimitCharactersAmountInTextCtrl(wxWindow *w)
{
	if (auto t = wxDynamicCast(w, wxTextCtrl)) {
		auto handler = [t](wxCommandEvent &ev) {
			const auto &text = ev.GetString();

			if (text.size() > maxNumberOfCharactersInTextCtrl) {
				t->ChangeValue(text.substr(0, maxNumberOfCharactersInTextCtrl));
				wxMsg::Error(_F("Not more than %d characters can be input.", maxNumberOfCharactersInTextCtrl));
				ev.Skip(false);
			}
			else
				ev.Skip(true);
		};

		t->Bind(wxEVT_TEXT, handler);
		t->Bind(wxEVT_TEXT_PASTE, handler);
	}
}

int wxDlg2Px(wxWindow *w, int dlg)
{
	return wxDLG_UNIT(w, wxPoint(0, dlg)).y;
}


void wxAddToSizer(wxBoxSizer *sizer, wxWindow *w, std::initializer_list<wxSizerPair<wxBoxSizerObject> > elems, int gap)
{
	if (!sizer || !w) {
		return;
	}

	bool horiz = sizer->GetOrientation() == wxHORIZONTAL;

	for (auto &e: elems)
	{
		auto flags = e.first;

		if (sizer->GetItemCount() > 0) {
			if ((flags.GetFlags() & wxDIRECTION_MASK) == 0)
				flags.Border(horiz ? wxLEFT : wxUP, wxDlg2Px(w, gap));
		}

		if ((flags.GetFlags() & wxALIGN_MASK) == 0 && (flags.GetFlags() & wxEXPAND) == 0)
			flags.Align(horiz ? wxALIGN_CENTER_VERTICAL : wxALIGN_NOT);

		if (!horiz && !(flags.GetFlags() & wxALIGN_CENTER_HORIZONTAL) && !(flags.GetFlags() & wxSHRINK))
			flags.Expand();

		std::visit(fz::util::overload{
			[&](auto *s) {
				if (!s)
					return;

				maybeLimitCharactersAmountInTextCtrl(s);
				sizer->Add(s, flags);
			},
			[&](int space) {
				if (space >= 0) {
					sizer->AddSpacer(wxDlg2Px(w, space));
				}
				else
					sizer->AddStretchSpacer(-space);
			}
		}, e.second);
	}
}

void wxAddToSizer(wxFlexGridSizer *sizer, wxWindow *w, std::initializer_list<wxSizerPair<wxGridSizerObject> > elems, wxAlignment default_alignment)
{
	if (!sizer || !w) {
		return;
	}

	if (sizer->GetCols() <= 0)
		return;

	for (auto &e: elems)
	{
		auto cur_col = sizer->GetItemCount() % std::size_t(sizer->GetCols());

		auto flags = e.first;

		if (flags.GetFlags() == 0 && sizer->IsColGrowable(cur_col))
			flags.Expand();

		if ((flags.GetFlags() & wxALIGN_MASK) == 0)
			flags.Align(default_alignment);

		std::visit(fz::util::overload{
			[&](auto *e) {
				if (!e)
					return;

				maybeLimitCharactersAmountInTextCtrl(e);
				sizer->Add(e, flags);
			},
			[&](int space) {
				if (space >= 0)
					sizer->AddSpacer(wxDlg2Px(w, space));
				else
					sizer->AddStretchSpacer(-space);
			}
		}, e.second);
	}
}

void wxAddToSizer(wxSDBSizer *sizer, wxWindow *w, std::initializer_list<wxSizerPair<wxSDBSizerObject> > elems)
{
	if (!sizer->sdb_) {
		wxAddToSizer(static_cast<wxBoxSizer*>(sizer), w, {
			new wxStaticLine(w),
			sizer->sdb_ = new wxStdDialogButtonSizer()
		}, wxDefaultGap);
	}

	for (auto &e: elems) {
		switch (e.first) {
			case wxAffirmative:
				sizer->sdb_->SetAffirmativeButton(e.second);
				break;

			case wxNegative:
				sizer->sdb_->SetNegativeButton(e.second);
				break;

			case wxCancel:
				sizer->sdb_->SetCancelButton(e.second);
				break;
		}
	}

	sizer->sdb_->Realize();
}

wxBoxSizerWrapper wxHBox(wxWindow *parent, wxPadding padding, int gap)
{
	return {parent, new wxBoxSizer(wxHORIZONTAL), padding, gap};
}

wxBoxSizerWrapper wxVBox(wxWindow *parent, wxPadding padding, int gap)
{
	return {parent, new wxBoxSizer(wxVERTICAL), padding, gap};
}

wxBoxSizerWrapper wxStaticHBox(wxWindow *p, const wxString &label, wxPadding padding, int gap)
{
	return {p, new wxStaticBoxSizer(wxHORIZONTAL, p, label), padding, gap};
}

wxBoxSizerWrapper wxStaticVBox(wxWindow *p, const wxString &label, wxPadding padding, int gap)
{
	return {p, new wxStaticBoxSizer(wxVERTICAL, p, label), padding, gap};
}

wxGridSizerWrapper wxGBox(wxWindow *parent, int cols, std::initializer_list<wxGBoxGrowableParams> growable_cols, std::initializer_list<wxGBoxGrowableParams> growable_rows, wxSize gap, wxAlignment default_alignment)
{
	gap.x = wxDlg2Px(parent, gap.x);
	gap.y = wxDlg2Px(parent, gap.y);

	auto s = new wxFlexGridSizer(cols, gap);

	for (auto c: growable_cols)
		s->AddGrowableCol(c.idx, c.proportion);

	for (auto r: growable_rows)
		s->AddGrowableRow(r.idx, r.proportion);

	return {parent, s, default_alignment};
}

wxSBDSizerWrapper wxSBDBox(wxWindow *parent)
{
	return { parent, new wxSDBSizer() };
}

void wxEnableStrictValidation(wxBookCtrlBase *b)
{
	auto changing = [b](wxBookCtrlEvent &ev) {
		if (ev.GetEventObject() != b) {
			ev.Skip();
			return;
		}

		auto p = b->GetCurrentPage();

		// If only the current page has to be validated, then we don't want validation to occur when the page is about to be left.
		auto only_if_current = dynamic_cast<wxValidateOnlyIfCurrentPageBase*>(p);
		bool can_be_validated = p && (!only_if_current || only_if_current->validate_when_leaving_);

		if (can_be_validated && (!p->Validate() || !p->TransferDataFromWindow()))
			ev.Veto();
		else {
			auto n = ev.GetSelection() != wxNOT_FOUND ? b->GetPage(std::size_t(ev.GetSelection())) : nullptr;
			auto c = dynamic_cast<wxValidateOnlyIfCurrentPageBase*>(n);

			// We want the page to which we are switching to to allow us in, even if it can only be validated if it's the current page.
			if (c)
				c->changing_ = true;

			if (n && !n->TransferDataToWindow())
				ev.Veto();
			else
				ev.Skip();

			if (c)
				c->changing_ = false;
		}
	};

	auto changed = [b](wxBookCtrlEvent &ev) {
		if (ev.GetEventObject() != b) {
			ev.Skip();
			return;
		}

		auto p = b->GetCurrentPage();

		if (p) {
			//p->TransferDataToWindow();

			// Some wx ports require this, otherwise some controls aren't properly drawn. (MacOS, for instance)
			p->CallAfter([p]{ p->Refresh(); });
		}

		ev.Skip();
	};

	if (dynamic_cast<wxNotebook *>(b)) {
		b->Bind(wxEVT_NOTEBOOK_PAGE_CHANGING, changing);
		b->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, changed);
	}
	else
	if (dynamic_cast<wxTreebook *>(b)) {
		b->Bind(wxEVT_TREEBOOK_PAGE_CHANGING, changing);
		b->Bind(wxEVT_TREEBOOK_PAGE_CHANGED, changed);
	}
	else
	if (dynamic_cast<wxChoicebook *>(b)) {
		b->Bind(wxEVT_CHOICEBOOK_PAGE_CHANGING, changing);
		b->Bind(wxEVT_CHOICEBOOK_PAGE_CHANGED, changed);
	}
	else {
		b->Bind(wxEVT_BOOKCTRL_PAGE_CHANGING, changing);
		b->Bind(wxEVT_BOOKCTRL_PAGE_CHANGED, changed);
	}
}

TitleCtrl::TitleCtrl(wxWindow *parent, const wxString &title, int style)
	: wxPanel(parent, wxID_ANY)
{
	auto back = GetBackgroundColour();
	auto fore = GetForegroundColour();

	wxPanel::SetBackgroundColour(fore);
	wxPanel::SetForegroundColour(back);

	wxVBox(this, 1) = {
		{ 0, label_ = wxLabel(this, title, style) }
	};

	label_->SetBackgroundColour(fore);
	label_->SetForegroundColour(back);
}

bool TitleCtrl::AcceptsFocus() const
{
	return false;
}

bool TitleCtrl::AcceptsFocusFromKeyboard() const
{
	return false;
}

bool TitleCtrl::SetBackgroundColour(const wxColour &colour)
{
	wxPanel::SetBackgroundColour(colour);
	return  label_->SetBackgroundColour(colour);
}

bool TitleCtrl::SetForegroundColour(const wxColour &colour)
{
	wxPanel::SetForegroundColour(colour);
	return label_->SetForegroundColour(colour);
}

void TitleCtrl::SetLabel(const wxString &label)
{
	wxPanel::SetLabel(label);
	label_->SetLabel(label);
}

bool TitleCtrl::SetFont(const wxFont &font)
{
	return label_->SetFont(font);
}

TitleCtrl *Title(wxWindow *parent, const wxString &title, int style)
{
	return new TitleCtrl(parent, title, style);
}

wxAutoRefocuser::wxAutoRefocuser()
{
	previously_focused_win_ = wxWindow::FindFocus();
}

wxAutoRefocuser::~wxAutoRefocuser()
{
	if (previously_focused_win_) {
		previously_focused_win_->SetFocus();
	}
}

wxTextFormatter wxLabel(wxWindow *parent, const wxString &text, int style)
{
	return new wxStaticText(parent, wxID_ANY, text, wxDefaultPosition, wxDefaultSize, style);
}

wxTextFormatter wxEscapedLabel(wxWindow *parent, const wxString &text, int style)
{
	return wxLabel(parent, fz::to_wxString(fz::replaced_substrings(text.ToStdWstring(), L"&", L"&&")), style);
}

namespace {

struct wxValidatingHelper: wxPanel
{
	using F = std::function<bool()>;

	wxValidatingHelper(wxWindow *parent, F f)
		: wxPanel(parent)
		, f_(std::move(f))
	{
		Hide();
	}

protected:
	F f_;
};

struct wxValidateHelper: wxValidatingHelper
{
	using wxValidatingHelper::wxValidatingHelper;

	bool Validate() override
	{
		return !f_ || f_();
	}
};

struct wxTransferDataToWindowHelper: wxValidatingHelper
{
	using wxValidatingHelper::wxValidatingHelper;


	bool TransferDataToWindow() override
	{
		return !f_ || f_();
	}
};

struct wxTransferDataFromWindowHelper: wxValidatingHelper
{
	using wxValidatingHelper::wxValidatingHelper;

	bool TransferDataFromWindow() override
	{
		return !f_ || f_();
	}
};

}

wxWindow *wxValidate(wxWindow *w, std::function<bool ()> func)
{
	return new wxValidateHelper(w, std::move(func));
}

wxWindow *wxTransferDataFromWindow(wxWindow *w, std::function<bool ()> func)
{
	return new wxTransferDataFromWindowHelper(w, std::move(func));
}

wxWindow *wxTransferDataToWindow(wxWindow *w, std::function<bool ()> func)
{
	return new wxTransferDataToWindowHelper(w, std::move(func));
}

// BUG: Clang requires these: clang's bug or something I don't understand about C++?
template struct wxValidateOnlyIfCurrentPage<wxChoicebook>;
template struct wxCreator::CtrlFix<wxValidateOnlyIfCurrentPage<wxChoicebook>>;

bool wxCopyToClipboard(const wxString &str, bool is_html)
{
	if (!wxTheClipboard->Open())
		return false;

	if (is_html) {
		auto obj = new wxDataObjectComposite;
		obj->Add(new wxHTMLDataObject(str));
		obj->Add(new wxTextDataObject(str));

		wxTheClipboard->SetData(obj);
	}
	else
		wxTheClipboard->SetData(new wxTextDataObject(str));

	wxTheClipboard->Close();

	return true;
}

wxWizardPageSimple *GetFirstPage(wxWizard *wiz)
{
	wxASSERT(wiz != nullptr);

	for (auto &n: wiz->GetPageAreaSizer()->GetChildren()) {
		if (n->IsWindow()) {
			if (auto page = dynamic_cast<wxWizardPageSimple *>(n->GetWindow()))
				return page;
		}
	}

	return nullptr;
}

int GetNumberOfFollowingPages(wxWizardPage *page)
{
	int n = -1;
	for (; page; ++n, page = page->GetNext());

	return n;
}

int GetIndexOfPage(wxWizardPage *page)
{
	int n = -1;
	for (; page; ++n, page = page->GetPrev());

	return n;
}

wxBoxSizerWrapper::wxBoxSizerWrapper(wxWindow *parent, wxBoxSizer *sizer, wxPadding padding, int gap)
	: wxSizerWrapper(parent, sizer)
	, padded_sizer_(padding.pad ? new wxBoxSizer(sizer->GetOrientation()) : nullptr)
	, gap_(gap)
{
	if (padded_sizer_) {
		sizer_->Add(padded_sizer_, wxSizerFlags(1).Expand().Border(padding.dir, wxDlg2Px(parent, padding.pad)));
	}
}

void wxAddToSizer(wxBoxSizerWrapper &w, std::initializer_list<wxSizerPair<wxBoxSizerObject> > elems)
{
	wxBoxSizer *sizer_to_add_to = w.padded_sizer_
		? w.padded_sizer_
		: w.sizer_;

	return ::wxAddToSizer(sizer_to_add_to, w.parent_, elems, w.gap_);
}

wxSBDSizerWrapper::wxSBDSizerWrapper(wxWindow *parent, wxSDBSizer *sizer)
	: wxSizerWrapper(parent, sizer)
{
}

wxGridSizerWrapper::wxGridSizerWrapper(wxWindow *parent, wxFlexGridSizer *sizer, wxAlignment default_alignment)
	: wxSizerWrapper(parent, sizer)
	, default_alignment_(default_alignment)
{}

void wxAddToSizer(wxGridSizerWrapper &w, std::initializer_list<wxSizerPair<wxGridSizerObject> > elems)
{
	::wxAddToSizer(w.sizer_, w.parent_, elems, w.default_alignment_);
}

static std::map<wxWindow *, std::pair<wxWeakRef<wxWindow>, fz::logger_interface *>> loggers_map;

void wxSetWindowLogger(wxWindow *win, fz::logger_interface *logger)
{
	if (logger)
		loggers_map[win] = { win, logger };
	else
		loggers_map.erase(win);
}

fz::logger_interface *wxGetWindowLogger(wxWindow *win)
{
	fz::logger_interface *logger = nullptr;

	do {
		if (auto it = loggers_map.find(win); it != loggers_map.end()) {
			if (!win || it->second.first)
				logger = it->second.second;
			else
				loggers_map.erase(it);

			break;
		}
	} while (win && (win = win->GetParent()));

	return logger;
}


void wxMsg::Builder::Open()
{
	bool has_top_window = wxTheApp->GetTopWindow() != nullptr;

	auto title = [&]() {
		if (title_.empty()) {
			if (flags_ & wxICON_ERROR)
				return _S("Error");
			else
			if (flags_ & wxICON_WARNING)
				return _S("Warning");
			else
			if (flags_ & wxICON_INFORMATION)
				return _S("Success");
			else
			if (flags_ & wxICON_QUESTION)
				return _S("Question");
		}

		return title_;
	}();

	if (!(flags_ & wxICON_NONE) && has_top_window) {
		if (result_) {
			wxGUIEventLoop loop;

			wxPushDialog<wxMessageDialog>(wxDialogQueue::use_top_window(), msg_, title, flags_) | [&](wxMessageDialog &diag) {
				diag.SetExtendedMessage(ext_);
				loop.Exit(diag.ShowModal());
			};

			*result_ = loop.Run();
		}
		else {
			wxPushDialog<wxMessageDialog>(wxDialogQueue::use_top_window(), msg_, title, flags_) | [ext = ext_](wxMessageDialog &diag) {
				diag.SetExtendedMessage(ext);
				diag.ShowModal();
			};
		}
	}
	else
	if (auto logger = wxGetWindowLogger(nullptr)) {
		auto type
			= flags_ & wxICON_ERROR
				? fz::logmsg::error
			: flags_ & wxICON_WARNING
				? fz::logmsg::warning
			: fz::logmsg::status;

		logger->log(type, wxT("%s: %s%s%s"), title, msg_, !ext_.empty() ? wxT(" - ") : wxT(""), ext_);
	}
}

wxMsg::Opener wxMsg::Builder::Error(const wxString &msg)
{
	return Opener(*this, wxOK | wxCENTER | wxICON_ERROR, msg);
}

wxMsg::Opener wxMsg::Builder::Warning(const wxString &msg)
{
	return Opener(*this, wxOK | wxCENTER | wxICON_WARNING, msg);
}

wxMsg::Opener wxMsg::Builder::Success(const wxString &msg)
{
	return Opener(*this, wxOK | wxCENTER | wxICON_INFORMATION, msg);
}

wxMsg::Opener wxMsg::Builder::Confirm(const wxString &msg)
{
	return Opener(*this, wxYES_NO | wxNO_DEFAULT | wxCENTER | wxICON_QUESTION, msg);
}

wxMsg::Opener wxMsg::Builder::ErrorConfirm(const wxString &msg)
{
	return Opener(*this, wxYES_NO | wxNO_DEFAULT | wxCENTER | wxICON_ERROR, msg);
}

wxMsg::Opener wxMsg::Builder::WarningConfirm(const wxString &msg)
{
	return Opener(*this, wxYES_NO | wxNO_DEFAULT | wxCENTER | wxICON_WARNING, msg);
}

wxMsg::Opener wxMsg::Error(const wxString &msg)
{
	return wxMsg::Builder().Error(msg);
}

wxMsg::Opener wxMsg::Warning(const wxString &msg)
{
	return wxMsg::Builder().Warning(msg);
}

wxMsg::Opener wxMsg::Success(const wxString &msg)
{
	return wxMsg::Builder().Success(msg);
}

wxMsg::Opener wxMsg::Confirm(const wxString &msg)
{
	return wxMsg::Builder().Confirm(msg);
}

wxMsg::Opener wxMsg::ErrorConfirm(const wxString &msg)
{
	return wxMsg::Builder().ErrorConfirm(msg);
}

wxMsg::Opener wxMsg::WarningConfirm(const wxString &msg)
{
	return wxMsg::Builder().WarningConfirm(msg);
}

wxMsg::Builder &wxMsg::Builder::Flags(int flags)
{
	flags_ = flags;
	return *this;
}

wxMsg::Builder &wxMsg::Builder::Message(const wxString &msg)
{
	msg_ = msg;
	return *this;
}

wxMsg::Builder &wxMsg::Builder::Title(const wxString &title)
{
	title_ = title;
	return *this;
}

wxMsg::Builder &wxMsg::Builder::Ext(const wxString &ext)
{
	ext_ = ext;
	return *this;
}

wxMsg::Builder &wxMsg::Builder::JustLog(bool just_log)
{
	if (just_log)
		flags_ |= wxICON_NONE;
	else
		flags_ &= ~wxICON_NONE;

	return *this;
}

wxMsg::Builder &wxMsg::Builder::Wait(int *result)
{
	static int dummy_result;

	if (!result)
		result = &dummy_result;

	result_ = result;

	return *this;
}

wxMsg::Opener::~Opener()
{
	if (!already_open_)
		bld_.Open();
}

wxMsg::Opener::operator int()
{
	int *other_result = bld_.result_;
	int this_result = 0;
	bld_.result_ = &this_result;
	bld_.Open();
	if (other_result)
		*other_result = this_result;

	already_open_ = true;
	return this_result;
}

wxMsg::Opener &&wxMsg::Opener::Ext(const wxString &ext) &&
{
	bld_.Ext(ext);
	return std::move(*this);
}

wxMsg::Opener &&wxMsg::Opener::Title(const wxString &title) &&
{
	bld_.Title(title);
	return std::move(*this);
}

wxMsg::Opener &&wxMsg::Opener::JustLog(bool just_log) &&
{
	bld_.JustLog(just_log);
	return std::move(*this);
}

wxMsg::Opener &&wxMsg::Opener::Wait(int *result) &&
{
	bld_.Wait(result);
	return std::move(*this);
}

wxMsg::Opener::Opener(const Builder &bld, int flags, const wxString &msg)
	: bld_(bld)
{
	bld_.flags_ |= flags;
	bld_.msg_ = msg;
}

wxWindow *wxPageLink(wxWindow *parent, const wxString &label, wxBookCtrlBase *book, std::size_t pageid, int style)
{
	auto link = new wxHyperlinkCtrl(parent, wxID_ANY, label, label, wxDefaultPosition, wxDefaultSize, style & ~(wxHL_CONTEXTMENU));
	link->Bind(wxEVT_HYPERLINK, [pageid, book](wxHyperlinkEvent &) {
		if (book)
			book->SetSelection(pageid);
	});

	link->SetVisitedColour(link->GetNormalColour());

	return link;
}

wxSizer *wxLoadFile(wxWindow *parent, std::function<void (std::string_view)> func, const wxString &label, const wxString &message, const wxString &default_name, const wxString &wildcards)
{
	wxButton *but = new wxButton(parent, wxID_ANY, label);
	but->Bind(wxEVT_BUTTON, [but, message, default_name, wildcards, func = std::move(func)](auto) {
		but->Disable();

		wxPushDialog<wxFileDialog>(but->GetParent(), message, wxEmptyString, default_name, wildcards, wxFD_OPEN | wxFD_FILE_MUST_EXIST) | [&func, but](wxFileDialog &d) {
			if (Settings()->working_dir().type() == fz::local_filesys::dir)
				d.SetDirectory(fz::to_wxString(Settings()->working_dir().str()));

			auto ret = d.ShowModal();

			Settings()->working_dir() = fz::to_native(d.GetDirectory());

			but->Enable();

			if (ret == wxID_CANCEL)
				return;

			auto f = fz::file(fz::to_native(d.GetPath()), fz::file::reading);

			if (f && f.size() > 0 && std::size_t(f.size()) > maxNumberOfCharactersInTextCtrl)
				wxMsg::Error(_S("File is too big."));
			else
			if (auto buf = fz::util::io::read(f); buf.empty())
				wxMsg::Error(_F("Couldn't load file '%s'.", d.GetPath()));
			else
				func(buf.to_view());
		};
	});

	return wxHBox(parent, 0) = but;
}

wxSizer *wxSaveFile(wxWindow *parent, std::function<std::string()> func, const wxString &label, const wxString &message, const wxString &default_name, const wxString &wildcards)
{
	wxButton *but = new wxButton(parent, wxID_ANY, label);
	but->Bind(wxEVT_BUTTON, [but, message, default_name, wildcards, func = std::move(func)](auto) {
		but->Disable();

		wxPushDialog<wxFileDialog>(but->GetParent(), message, wxEmptyString, default_name, wildcards, wxFD_SAVE | wxFD_OVERWRITE_PROMPT) | [&func, but](wxFileDialog &d) {
			if (Settings()->working_dir().type() == fz::local_filesys::dir)
				d.SetDirectory(fz::to_wxString(Settings()->working_dir().str()));

			auto ret = d.ShowModal();

			Settings()->working_dir() = fz::to_native(d.GetDirectory());

			but->Enable();

			if (ret == wxID_CANCEL)
				return;

			if (!fz::util::io::write(fz::to_native(d.GetPath()), func()))
				wxMsg::Error(_F("Couldn't save to file '%s'.", d.GetPath()));
		};
	});

	return wxHBox(parent, 0) = but;
}

wxSizer *wxSaveTextToFile(wxWindow *parent, std::function<wxString()> text_func, const wxString &label, const wxString &message, const wxString &default_name, const wxString &wildcards)
{
	return wxSaveFile(parent, [func = std::move(text_func)] {
		return fz::to_utf8(func());
	}, label, message, default_name, wildcards);
}

wxSizer *wxLoadTextFromFile(wxWindow *parent, std::function<void (const wxString &)> text_func, const wxString &label, const wxString &message, const wxString &default_name, const wxString &wildcards)
{
	return wxLoadFile(parent, [func = std::move(text_func)](std::string_view v){
		func(fz::to_wxString(v));
	}, label, message, default_name, wildcards);
}

wxSizer *wxSaveTextToFile(wxWindow *parent, wxTextCtrl &text, const wxString &label, const wxString &message, const wxString &default_name, const wxString &wildcards)
{
	return wxSaveTextToFile(parent, [&text]{ return text.GetValue(); }, label, message, default_name, wildcards);
}

wxSizer *wxLoadTextFromFile(wxWindow *parent, wxTextCtrl &text, const wxString &label, const wxString &message, const wxString &default_name, const wxString &wildcards)
{
	return wxLoadTextFromFile(parent, [&text](const wxString &t){ text.SetValue(t); }, label, message, default_name, wildcards);
}

wxSizer *wxCopyTextToClipboard(wxWindow *parent, wxTextCtrl &text, const wxString &label)
{
	wxButton *but = new wxButton(parent, wxID_ANY, label);
	but->Bind(wxEVT_BUTTON, [&text](auto) {
		if (!wxCopyToClipboard(text.GetValue()))
			wxMsg::Error(_S("Couldn't copy text to clipboard."));
	});

	return wxHBox(parent, 0) = but;
}

wxTextFormatter wxWText(wxWindow *parent, const wxString &text, int style)
{
	return new WrappedText(parent, text, style);
}

namespace {

#define DEBUG_NEST_ENABLED 0

struct debug_nest
{
	debug_nest(const char *name [[maybe_unused]])
	#if DEBUG_NEST_ENABLED
		: name(name)
	#endif
	{
	#if DEBUG_NEST_ENABLED
		indent(">>>", get_level()++) << name << ": " << this << "\n";
	#endif
	}

	~debug_nest()
	{
	#if DEBUG_NEST_ENABLED
		indent("<<<", --get_level()) << name << ": " << this << "\n";
	#endif
	}

	template <typename... Args>
	void operator()(const char *format [[maybe_unused]], const Args &... args [[maybe_unused]])
	{
	#if DEBUG_NEST_ENABLED
		indent("!!!", get_level()) << fz::sprintf(format, args...) << "\n";
	#endif
	}

private:
#if DEBUG_NEST_ENABLED
	const char *name;

	std::ostream &indent(const char *tag, std::size_t level)
	{
		return std::cerr << tag << " " << std::string(level*4, ' ');
	}

	static std::size_t &get_level()
	{
		static std::size_t level = 0;
		return level;
	}
#endif
};

}

bool wxCreator::apply_to_all_pages(wxBookCtrlBase *b, bool (wxWindow::*m)(), const char *name)
{
	debug_nest debug(name);

	auto faulty = [&](std::size_t f)
	{
		debug("FAULTY: %d", f);

		b->CallAfter([b, f]{b->ChangeSelection(f);});
		return false;
	};

	auto cp = b->GetCurrentPage();
	if (cp && !(cp->*m)())
		return faulty(std::size_t(b->GetSelection()));

	for (std::size_t  i = 0, n = b->GetPageCount(); i != n; ++i) {
		if (auto p = b->GetPage(i); p != cp)
			if (!(p->*m)())
				return faulty(i);
	}

	return true;
}

wxDialogQueue::wxDialogQueue()
	: use_top_window_(new wxWindow)
{
	timer_.Bind(wxEVT_TIMER, [this](wxTimerEvent &) {
		try_dequeue();
	});
}

wxDialogQueue::~wxDialogQueue()
{
	delete use_top_window_;
}

wxDialogQueue &wxDialogQueue::instance()
{
	static wxDialogQueue self;
	return self;
}

void wxDialogQueue::try_dequeue()
{
	while (!queue_.empty()) {
		if (auto &parent = queue_.front().parent; parent == use_top_window_) {
			if (!stack_.empty()) {
				parent = stack_.top().get();
			}
			// If no dialog is in the stack, use the the TopWindow as parent
			else {
				parent = wxTheApp->GetTopWindow();
			}
		}

		if (!queue_.front().parent) {
			// Parent doesn't exist, don't display the dialog and proceed with the next one in the queue.
			queue_.pop();
			continue;
		}

		if (!stack_.empty() && stack_.top().get() != queue_.front().parent && stack_.top().get() != wxGetTopLevelParent(queue_.front().parent)) {
			// A dialog that is already displayed must be the parent of the one that is wanted to be displayed.
			// If that's not the case, try to display it only when the currently open one is closed.
			return;
		}
		else
		if (must_be_delayed()) {
			// Not possible to display the dialog right now, try later.
			timer_.Start(100, true);
			return;
		}
		else {
			auto e = std::move(queue_.front());
			queue_.pop();

			auto parent = e.parent->IsTopLevel() ? e.parent.get() : wxGetTopLevelParent(e.parent);

			if (auto d = e.creator(parent)) {

				stack_.push(d);
				#ifdef __WXMAC__
					shown_dialogs_creation_events_.push(wxTheApp->MacGetCurrentEvent());
				#endif

				e.opener(d);

				if (stack_.top())
					// Destroy the dialog, if it wasn't destroyed already.
					stack_.top()->Destroy();

				#ifdef __WXMAC__
					shown_dialogs_creation_events_.pop();
				#endif
				stack_.pop();

				if (parent->IsShown())
					parent->Raise();
			}
		}
	}
}

bool wxDialogQueue::must_be_delayed()
{
	wxMouseState mouseState = wxGetMouseState();
	if (mouseState.LeftIsDown() || mouseState.MiddleIsDown() || mouseState.RightIsDown()) {
		// Displaying a dialog while the user is clicking is extremely confusing, don't do it.
		return true;
	}

#ifdef __WXMSW__
	// During a drag & drop we cannot show a dialog. Doing so can render the program unresponsive
	if (GetCapture()) {
		return true;
	}
#endif

#ifdef __WXMAC__
	void* ev = wxTheApp->MacGetCurrentEvent();
	if (ev && (shown_dialogs_creation_events_.empty() || ev != shown_dialogs_creation_events_.top())) {
		// We're inside an event handler for a native mac event, such as a popup menu
		return true;
	}
#endif

	return false;
}

wxString wxGetContainingPageTitle(wxWindow *w, bool full_path)
{
	wxString title;

	while (w && w->GetParent()) {
		wxString text;

		if (!dynamic_cast<wxSimplebook*>(w->GetParent()))
		if (auto *b = dynamic_cast<wxBookCtrlBase *>(w->GetParent())) {
			auto i = b->FindPage(w);

			while (i != wxNOT_FOUND) {
				text = b->GetPageText(std::size_t(i));

				if (!text.empty()) {
					if (title.empty())
						title = text;
					else
						title = text + wxT("/") + title;
				}

				if (full_path) {
					if (auto *t = dynamic_cast<wxTreebook *>(b)) {
						i = t->GetPageParent(std::size_t(i));
						continue;
					}
				}

				i = wxNOT_FOUND;
			}

			if (!full_path)
				break;
		}

		w = w->GetParent();
	}

	return title;
}

wxCheckBoxGroup::wxCheckBoxGroup(wxWindow *parent)
	: wxPanel(parent)
{
	auto s = new wxBoxSizer(wxVERTICAL);

	wxAddToSizer(s, this, {
		wxHBox(this, 0) = {
			select_all_ = new wxHyperlinkCtrl(this, wxID_ANY, _S("Select all"), _S("Select all")),
			deselect_all_ = new wxHyperlinkCtrl(this, wxID_ANY, _S("Deselect all"), _S("Deselect all"))
		}
	}, wxDefaultGap);

	select_all_->Bind(wxEVT_HYPERLINK, [&](wxHyperlinkEvent &) {
		SetValue(true);
	});
	select_all_->SetVisitedColour(select_all_->GetNormalColour());

	deselect_all_->Bind(wxEVT_HYPERLINK, [&](wxHyperlinkEvent &) {
		SetValue(false);
	});
	deselect_all_->SetVisitedColour(deselect_all_->GetNormalColour());

	SetSizer(s);
}

wxCheckBoxGroup::CB wxCheckBoxGroup::c(const wxString &label)
{
	auto s = static_cast<wxBoxSizer *>(GetSizer());
	auto c = new wxCheckBox(this, wxID_ANY, label);

	wxAddToSizer(s, this, { c }, wxDefaultGap);

	c->Bind(wxEVT_CHECKBOX, [&](wxCommandEvent &ev) {
		ev.Skip();
	});

	return { c };
}

bool wxCheckBoxGroup::IsAnyChecked() const
{
	for (auto &e: GetSizer()->GetChildren()) {
		if (auto *c = dynamic_cast<wxCheckBox *>(e->GetWindow()))
			if (c->IsChecked())
				return true;
	}

	return false;
}

void wxCheckBoxGroup::SetValue(bool v)
{
	for (auto &e: GetSizer()->GetChildren()) {
		if (auto *c = dynamic_cast<wxCheckBox *>(e->GetWindow()); c && c->IsEnabled()) {
			c->SetValue(v);
		}
	}
}

std::size_t wxCheckBoxGroup::GetCheckedNumber() const
{
	std::size_t res{};

	for (auto &e: GetSizer()->GetChildren()) {
		if (auto *c = dynamic_cast<wxCheckBox *>(e->GetWindow()))
			res += c->IsChecked();
	}

	return res;
}

std::size_t wxCheckBoxGroup::GetNumberOfEnabledCheckboxes() const
{
	std::size_t res{};

	for (auto &e: GetSizer()->GetChildren()) {
		if (auto *c = dynamic_cast<wxCheckBox *>(e->GetWindow()))
			res += c->IsEnabled();
	}

	return res;
}

wxCheckBoxGroup::CB::operator bool() const
{
	return cb_->IsChecked();
}

wxCheckBoxGroup::CB &wxCheckBoxGroup::CB::operator=(bool v)
{
	if (cb_->IsEnabled())
		cb_->SetValue(v);

	return *this;
}

void wxCheckBoxGroup::CB::Enable(bool enabled)
{
	cb_->Enable(enabled);
}

void wxCheckBoxGroup::CB::EnableAndSet(bool value)
{
	cb_->Enable(value);
	*this = value;
}

wxCheckBoxGroup::CB::CB(wxCheckBox *cb)
	: cb_(cb)
{}

wxSize wxTextExtent(std::size_t num_characters_per_line, std::size_t num_lines, wxWindow *win, wxFontFamily family, std::initializer_list<wxSystemMetric> x_metrics, std::initializer_list<wxSystemMetric> y_metrics)
{
	static wxString fake_text;
	if (fake_text.empty()) {
		for (std::size_t i = 0; i < num_lines; ++i)
			fake_text.append(wxString(L'F', num_characters_per_line)).append(L'\n');
	}

	if (auto s = fake_text.size(); s != 0)
		fake_text.resize(s-1);

	return wxTextExtent(fake_text, win, family, x_metrics, y_metrics);
}

wxSize wxTextExtent(const wxString &text, wxWindow *win, wxFontFamily family, std::initializer_list<wxSystemMetric> x_metrics, std::initializer_list<wxSystemMetric> y_metrics)
{
	wxClientDC dc(win);

	if (family != wxFONTFAMILY_UNKNOWN) {
		auto font = win->GetFont();
		font.SetFamily(family);
		dc.SetFont(font);
	}

	auto extent = dc.GetMultiLineTextExtent(text);

	for (auto m: x_metrics)
		extent.IncBy(wxSystemSettings::GetMetric(m), 0);

	for (auto m: y_metrics)
		extent.IncBy(0, wxSystemSettings::GetMetric(m));

	return extent;

}

wxSize wxMonospaceTextExtent(std::size_t num_characters_per_line, std::size_t num_lines, wxWindow *win, std::initializer_list<wxSystemMetric> x_metrics, std::initializer_list<wxSystemMetric> y_metrics)
{
	return wxTextExtent(num_characters_per_line, num_lines, win, wxFONTFAMILY_TELETYPE, x_metrics, y_metrics);
}

wxSize wxMonospaceTextExtent(const wxString &text, wxWindow *win, std::initializer_list<wxSystemMetric> x_metrics, std::initializer_list<wxSystemMetric> y_metrics)
{
	return wxTextExtent(text, win, wxFONTFAMILY_TELETYPE, x_metrics, y_metrics);
}

wxWindow *wxFindFirstPage(wxWindow *p, wxWindow *root)
{
	while (p && p != root && p->GetParent()) {
		if (wxDynamicCast(p->GetParent(), wxBookCtrlBase)) {
			return p;
		}

		p = p->GetParent();
	}

	return nullptr;
}

wxBookCtrlBase *wxSwitchBookTo(wxWindow *p, wxWindow *root)
{
	wxBookCtrlBase *ret{};

	p = wxFindFirstPage(p, root);

	if (p) {
		if (auto b = wxDynamicCast(p->GetParent(), wxBookCtrlBase)) {
			if (int i = b->FindPage(p); i >= 0) {
				ret = b;

				if (b->GetSelection() == i) {
					p->TransferDataToWindow();
				}
				else {
					b->SetSelection(std::size_t(i));
				}

				wxSwitchBookTo(b, root);
			}
		}
	}

	return ret;
}

bool wxIsSelected(wxWindow *p)
{
	p = wxFindFirstPage(p);

	if (p) {
		if (auto b = wxDynamicCast(p->GetParent(), wxBookCtrlBase)) {
			return b->GetSelection() == b->FindPage(p);
		}
	}

	return false;
}

InvalidPathExplanation::InvalidPathExplanation(const fz::tvfs::validation::result &res, fz::util::fs::path_format native_path_format, bool path_is_for_tvfs, const wxString &what)
{
	if (auto e = res.invalid_placeholder_values()) {
		wxString invalids;
		for (const auto &x: e->explanations) {
			if (!invalids.empty())
				invalids += wxT("\n");

			invalids += fz::to_wxString(x);
		}

		main = _F("Placeholders expansion for the %s has issues.", what);
		extra = invalids;
		return;
	}

	if (res.path_has_invalid_characters()) {
		main = _F("The %s contains invalid characters.", what);
	}
	else
	if (res.path_is_not_absolute()) {
		main = _F("The %s must be absolute.", what);
	}
	else
	if (res.path_is_empty()) {
		main = _F("The %s must not be empty.", what);
	}

	if (path_is_for_tvfs) {
		if (native_path_format == fz::util::fs::windows_format) {
			extra = _S(
				"Character '\\' is not allowed in the path.\n"
				"Moreover, file and directory names in the path must not include the ':' character and must not terminate with a space or a dot."
			);
		}
	}
	else
	if (native_path_format == fz::util::fs::windows_format) {
		extra = _S(
			"The path must be in the form\n"
			"    L:\\[...]\n"
			"or\n"
			"    \\\\server\\share[\\...]\n"
			"or\n"
			"    \\\\.\\UNC\\server\\share[\\...]\n\n"
			"Moreover, file and directory names in the path must not include the ':' character, must not terminate with a space or a dot and must not contain characters that are not convertible to UTF-8."
		);
	}
}

namespace fx {

namespace colors {

	wxColour error(255, 0, 0);
	wxColour command(0, 0, 128);
	wxColour reply(0, 128, 0);
	wxColour warning(0xFF, 0x77, 0x22);
	wxColour trace(128, 0, 128);
}

wxButton *RetrieveButton(wxWindow *parent, wxTextCtrl &dest, const wxString &label, const wxString &failure_msg, RetrieveFunc retrieve_func)
{
	wxButton *button = new wxButton(parent, nullID, label);

	button->Bind(wxEVT_BUTTON, [retrieve_func = std::move(retrieve_func), button, &dest, label, failure_msg](auto &) {
		button->SetLabel(_S("Retrieving..."));
		button->Disable();

		auto retrieved = retrieve_func ? retrieve_func() : fz::unexpected(_S("Retrieving function not set."));

		button->SetLabel(label);

		if (!retrieved) {
			wxMsg::Error(wxS("%s"), failure_msg).Ext(wxS("%s"), retrieved.error()).Wait();
		}
		else {
			dest.ChangeValue(*retrieved);
		}

		button->Enable();
	});

	return button;
}

fz::expected<wxString, wxString> RetrievePublicIpEx(const RetrievePublicIpFunc &func, ipv_type ipv)
{
	if (!func) {
		return fz::unexpected(_S("Invalid parameters"));
	}

	std::array<std::optional<fz::address_type>, 2> types{};

	if (ipv == ipv_type::any) {
		types[0] = fz::address_type::unknown;
	}
	else {
		if (ipv & ipv_type::ipv4) {
			types[0] = fz::address_type::ipv4;
		}

		if (ipv & ipv_type::ipv6) {
			types[1] = fz::address_type::ipv6;
		}
	}

	wxString ips;
	wxString errors;

	for (auto t: types) {
		if (t) {
			auto ip = func(*t);
			if (ip) {
				if (!ips.empty()) {
					ips += wxS(", ");
				}

				ips += fz::to_wxString(*ip);
			}
			else {
				if (!errors.empty()) {
					errors += wxS("\n");
				}

				if (*t == fz::address_type::ipv4) {
					errors += wxS("IPv4: ");
				}
				else
					if (*t == fz::address_type::ipv6) {
						errors += wxS("IPv6: ");
					}

				errors += ip.error();
			}
		}
	}

	if (!ips.empty()) {
		return ips;
	}

	return fz::unexpected(std::move(errors));
}

static RetrieveFunc AdaptRetrieveIpFunction(RetrievePublicIpFunc func, ipv_type ipv)
{
	if (!func) {
		return nullptr;
	}

	return [func = std::move(func), ipv]() -> fz::expected<wxString, wxString>
	{
		return RetrievePublicIpEx(func, ipv);
	};
}

wxButton *RetrievePublicIpButton(wxWindow *parent, wxTextCtrl &dest, ipv_type ipv, RetrievePublicIpFunc && func)
{
	return RetrieveButton(parent, dest, _S("Retrieve public IP"), _S("Couldn't retrieve server's public IP."), fx::AdaptRetrieveIpFunction(std::move(func), ipv));
}

wxButton *RetrievePublicIpButton(wxWindow *parent, wxTextCtrl &dest, ipv_type ipv, RetrievePublicIpFunc &func)
{
	return RetrievePublicIpButton(parent, dest, ipv, [&func](fz::address_type at) -> fz::expected<std::string, wxString>
	{
		if (!func) {
			return fz::unexpected(_S("Retrieving IP function not set."));
		}

		return func(at);
	});
}

fz::expected<std::vector<std::string>, wxString> ResolveHostname(fz::thread_pool &fz_pool, fz::event_loop &fz_loop, const wxString &host, fz::address_type family)
{
	wxGUIEventLoop wx_loop;
	std::vector<std::string> ret;
	wxString wx_error;

	auto dispatcher = fz::util::make_dispatcher<fz::hostname_lookup_event>(fz_loop,
		[&wx_loop, &ret, &wx_error](fz::hostname_lookup *, int error, std::vector<std::string> &ips)
	{
	   if (error) {
		   wx_error = fz::to_wxString(fz::socket_error_description(error));
	   }
	   else {
		   ret = std::move(ips);
	   }

	   wx_loop.Exit();
	});

	fz::hostname_lookup lookuper(fz_pool, dispatcher);
	lookuper.lookup(fz::to_native(host), family);

	wx_loop.Run();

	if (!ret.empty()) {
		return ret;
	}

	return fz::unexpected(std::move(wx_error));
}

fz::expected<std::vector<std::string>, wxString> ResolveHostname(const wxString &host, fz::address_type family)
{
	fz::thread_pool fz_pool;
	fz::event_loop fz_loop(fz_pool);

	return ResolveHostname(fz_pool, fz_loop, host, family);
}

wxString ValidateHostname(const wxString &h, bool at_least_2nd_level)
{
	if (h.size() > 253) {
		return _S("Maximum allowed number of characters is hostnames is 253.");
	}

	auto labels = fz::strtok(fz::to_native(h), fzT("."), false);

	if (at_least_2nd_level && labels.size() < 2) {
		return _S("You must input at least 2nd level domain names (i.e example.com, example.net, etc.)");
	}

	for (auto &l: labels) {
		if (l.size() == 0 || l.size() > 63) {
			return _S("Components of host names cannot be empty and cannot exceed 63 characters");
		}

		std::size_t number_count = 0;

		auto invalid = l.front() == '-' || l.back() == '-';
		if (!invalid) {
			fz::util::parseable_range r(l);

			for (;;) {
				if (lit(r, '0', '9')) {
					number_count += 1;
					continue;
				}

				if (lit(r, 'a', 'z') || lit(r, 'A', 'Z') || lit(r, '-')) {
					continue;
				}

				break;
			}

			invalid = !eol(r) ;
		}

		if (invalid) {
			return _S("Components of host names can only include digits (0-9), letters (a-z, A-Z), and hyphens (-)");
		}

		if (number_count == l.size()) {
			return _S("Components of host names cannot be all numbers");
		}
	}

	return wxEmptyString;
}

fz::address_type IdentifyAddressType(const wxString &host)
{
	auto check = [&host](auto &h) {
		fz::util::parseable_range r(host);
		return parse_ip(r, h) && eol(r);
	};

	if (fz::hostaddress::ipv4_host h; check(h)) {
		return fz::address_type::ipv4;
	}

	if (fz::hostaddress::ipv6_host h; check(h)) {
		return fz::address_type::ipv6;
	}

	return fz::address_type::unknown;
}

wxString ValidateHost(const wxString &h, bool at_least_2nd_level)
{
	if (IdentifyAddressType(h) == fz::address_type::unknown) {
		return ValidateHostname(h, at_least_2nd_level);
	}

	return wxEmptyString;
}

bool ValidatePassiveModeHostMsg(const wxString &val)
{
	if (val.empty()) {
		return true;
	}

	auto at = fx::IdentifyAddressType(val);

	if (at == fz::address_type::ipv6) {
		wxMsg::Error(_S("IPv6 address cannot be used for the passive mode host.")).Wait();
		return false;
	}

	if (at == fz::address_type::unknown) {
		auto err = fx::ValidateHostname(val, false);
		if (!err.empty()) {
			wxMsg::Error(err).Wait();
			return false;
		}

		auto ips = fx::ResolveHostname(val, fz::address_type::ipv4);
		wxString ext;
		if (!ips) {
			ext = ips.error();
		}
		else {
			ips->erase(std::remove_if(ips->begin(), ips->end(), [](auto &ip) {
				return fx::IdentifyAddressType(fz::to_wxString(ip)) != fz::address_type::ipv4;
			}), ips->end());

			if (ips->empty()) {
				ext = _S("Hostname did not resolve to an IPv4 address.");
			}
			else
			if (ips->size() > 1) {
				ext = _S("Hostname resolved to multiple IPv4 addresses, this can be problematic.");
			}
		}

		if (!ext.empty()) {
			int res = wxMsg::WarningConfirm(_S("Issues resolving PASV hostname.")).Ext(_F("%s.\n\nDo you wish to proceed with the current hostname choice?", ext));
			if (res != wxID_YES) {
				return false;
			}
		}
	}

	return true;
}

}
