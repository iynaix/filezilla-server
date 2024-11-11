#include <libfilezilla/glue/wx.hpp>

#include <wx/combo.h>
#include <wx/sizer.h>
#include <wx/rearrangectrl.h>
#include <wx/log.h>
#include <wx/statline.h>

#include <numeric>
#include <algorithm>

#include "rearrangeditemspickerctrl.hpp"
#include "helpers.hpp"

#if defined(__WXOSX__)
#	define FZ_REARRANGEDITEMPICKER_USE_DIALOG 1
#else
#   define FZ_REARRANGEDITEMPICKER_USE_DIALOG 0
#endif

class RearrangedItemsPickerCtrl::CheckListBox: public wxPanel {
public:
	CheckListBox(RearrangedItemsPickerCtrl *owner)
		: owner_(owner)
	{}

	bool Create(wxWindow *parent, wxComboCtrl *combo)
	{
		if (!wxPanel::Create(parent))
			return false;

		combo_ = combo;

		Rearrange();
		return true;
	}

	wxString GetStringValue() const
	{
		return owner_->GetStringValue();
	}

	void Rearrange()
	{
		// This whole creation/recreation thing works around a bug in wx which has been fixed in 3.1 but persists in 3.0.
		// BUG: https://github.com/wxWidgets/wxWidgets/issues/17836#issuecomment-1011164007
		// Unfortunately, it requires a "focus hack" on windows. See below.
		#if defined(FZ_WINDOWS) && FZ_WINDOWS
			wxAutoRefocuser refocus;
		#endif

		if (rearrange_) {
			rearrange_->Hide();
			rearrange_->Destroy();
		}

		if (GetParent()) {
			MakeOrder(owner_->active_);

			auto &available = owner_->available_;

			rearrange_ = new wxRearrangeCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, order_, available);
			rearrange_->SetExtraStyle(wxWS_EX_PROCESS_UI_UPDATES);

			SetSizer(new wxBoxSizer(wxVERTICAL));

			rearrange_->Bind(wxEVT_BUTTON, [&](wxCommandEvent& ev) {
				ev.Skip();

				if (ev.GetId() == wxID_UP || ev.GetId() == wxID_DOWN)
					CallAfter([&]{SetText();});
			});

			rearrange_->GetList()->Bind(wxEVT_CHECKLISTBOX, [&](wxCommandEvent &ev) {
				ev.Skip();
				CallAfter([&]{SetText();});
			});

			rearrange_->GetList()->Bind(wxEVT_LISTBOX, [&](wxCommandEvent &ev) {
				ev.Skip();
				wxWindow::UpdateWindowUI(wxUPDATE_UI_PROCESS_SPECIFIED);
			});

			GetSizer()->Add(rearrange_, 1, wxEXPAND);

			Fit();

			SetText();
		}
	}

private:
	void MakeOrder(const std::vector<std::string> &active)
	{
		auto find_in_available = [this](std::size_t start, const wxString &to_find) {
			auto it = std::find_if(owner_->available_.begin() + start, owner_->available_.end(), [&to_find](const wxString &s) { return s == to_find; });

			if (it == owner_->available_.end())
				return std::string::npos;

			return std::size_t(it-owner_->available_.begin());
		};

		order_.clear();
		order_.reserve(owner_->available_.size());

		std::size_t last_selected_idx = std::string::npos;

		for (std::size_t i = 0; i < owner_->available_.size(); ++i) {
			int idx = ~int(i);

			if (i < active.size()) {
				auto available_idx = find_in_available(i, fz::to_wxString(active[i]));
				if (available_idx != std::string::npos) {
					std::swap(owner_->available_[available_idx], owner_->available_[i]);

					idx = ~idx;
					last_selected_idx = i;
				}
			}

			order_.push_back(idx);
		}

		if (last_selected_idx != std::string::npos)
			std::sort(&owner_->available_[last_selected_idx]+1, &owner_->available_[0]+owner_->available_.size());
	}

	void SetText()
	{
		SetActive();
		if (combo_)
			combo_->SetText(GetStringValue());

	}

	void SetActive()
	{
		owner_->active_.clear();

		if (rearrange_) {
			const auto &current_order = rearrange_->GetList()->GetCurrentOrder();
			for (auto pos: current_order) {
				if (pos >= 0) {
					auto s = owner_->available_[(size_t)pos].utf8_str();
					owner_->active_.push_back(std::string(s.data(), s.length()));
				}
			}
		}
	}

private:
	RearrangedItemsPickerCtrl *owner_{};
	wxComboCtrl *combo_{};
	wxRearrangeCtrl *rearrange_{};
	wxArrayInt order_;
};

class RearrangedItemsPickerCtrl::Popup: public wxComboPopup {
public:
	Popup(CheckListBox *list)
		: list_(list)
	{}

	CheckListBox *GetList() const
	{
		return list_;
	}

	bool LazyCreate() override
	{
		return false;
	}

	bool Create(wxWindow *parent) override
	{
		return list_->Create(parent, GetComboCtrl());
	}

	wxWindow *GetControl() override
	{
		return list_;
	}

	wxString GetStringValue() const override
	{
		return list_->GetStringValue();
	}

	wxSize GetAdjustedSize(int minWidth, int, int) override
	{
		return {minWidth, list_->GetBestSize().GetHeight()};
	}

private:
	CheckListBox *list_;
};

RearrangedItemsPickerCtrl::RearrangedItemsPickerCtrl(wxWindow *parent)
{
	Create(parent);
}

bool RearrangedItemsPickerCtrl::Create(wxWindow *parent)
{
	if (!wxPanel::Create(parent))
		return false;

#if FZ_REARRANGEDITEMPICKER_USE_DIALOG
	struct DiagCombo: wxComboCtrl
	{
		DiagCombo(RearrangedItemsPickerCtrl *owner)
			: wxComboCtrl(owner, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxCC_STD_BUTTON | wxCB_READONLY)
		{
		}

	private:
		wxDialog *dlg_{};

		void OnMove(wxMoveEvent &ev)
		{
			dlg_->SetPosition(ClientToScreen(wxPoint(GetPosition().x, GetPosition().y + GetSize().GetHeight())));
			ev.Skip();
		}

		void OnSize(wxSizeEvent &ev)
		{
			dlg_->SetSize(wxSize(GetSize().GetWidth(), dlg_->GetSize().GetHeight()));
			ev.Skip();
		}

		void OnButtonClick() override
		{
			Disable();

			wxPushDialog(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSTAY_ON_TOP) | [this](wxDialog &dlg) {
				dlg_ = &dlg;

				auto owner = static_cast<RearrangedItemsPickerCtrl *>(GetParent());

				owner->list_ = new CheckListBox(owner);
				owner->list_->Create(&dlg, this);

				wxVBox(&dlg, 0) = {
					{ wxSizerFlags(1).Expand(), owner->list_ },
					new wxStaticLine(&dlg),
					{ wxSizerFlags(0).Expand(), dlg.CreateStdDialogButtonSizer(wxOK) },
				};

				dlg.SetPosition(ClientToScreen(wxPoint(GetPosition().x, GetPosition().y + GetSize().GetHeight())));
				dlg.SetMinSize(wxSize(GetSize().GetWidth(), -1));

				dlg.Fit();
				dlg.Layout();

				auto top_parent = wxGetTopLevelParent(this);

				if (top_parent)
					top_parent->Bind(wxEVT_MOVE, &DiagCombo::OnMove, this);

				Bind(wxEVT_SIZE, &DiagCombo::OnSize, this);

				Enable();
				dlg.ShowModal();

				Unbind(wxEVT_SIZE, &DiagCombo::OnSize, this);

				if (top_parent)
					top_parent->Unbind(wxEVT_MOVE, &DiagCombo::OnMove, this);

				owner->list_ = nullptr;
				dlg_ = nullptr;
			};
		}

		virtual void DoSetPopupControl(wxComboPopup*) override
		{
		}
	};

	combo_ = new DiagCombo(this);
#else
	combo_ = new wxComboCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxCC_STD_BUTTON | wxCB_READONLY);
	combo_->UseAltPopupWindow(true);

	list_ = new CheckListBox(this);
	combo_->SetPopupControl(new Popup(list_));
#endif

	SetSizer(new wxBoxSizer(wxVERTICAL));
	GetSizer()->Add(combo_, 1, wxEXPAND);

	Fit();

	return true;
}

void RearrangedItemsPickerCtrl::SetAvailableItems(wxArrayString available)
{
	available_ = std::move(available);
	if (list_)
		list_->Rearrange();
	else
	if (combo_)
		combo_->SetValue(GetStringValue());
}

void RearrangedItemsPickerCtrl::SetActiveItems(const std::vector<std::string> &active)
{
	active_ = active;
	if (list_)
		list_->Rearrange();
	else
	if (combo_)
		combo_->SetValue(GetStringValue());
}

std::size_t RearrangedItemsPickerCtrl::GetActiveItems(std::vector<std::string> &active)
{
	active = active_;
	return active_.size();
}

wxString RearrangedItemsPickerCtrl::GetStringValue() const
{
	wxString value;

	for (auto &a: active_) {
		if (!value.empty())
			value += wxS(" ＞ ");

		value += fz::to_wxString(a);
	}

	return value;
}

