#include <libfilezilla/glue/wx.hpp>
#include <libfilezilla/format.hpp>

#include <wx/listctrl.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/timer.h>

#include "fluidcolumnlayoutmanager.hpp"
#include "helpers.hpp"
#include "../filezilla/util/parser.hpp"

#include "groupslist.hpp"

#include "locale.hpp"
#include "glue.hpp"
#include "listctrlex.hpp"

class GroupsList::List: public wxListCtrlEx
{
	static int constexpr timer_ms = 10;

public:
	List(GroupsList *parent)
		: wxListCtrlEx(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
					 wxTAB_TRAVERSAL | wxLC_VIRTUAL | wxLC_REPORT | wxLC_EDIT_LABELS | wxLC_SINGLE_SEL)
	{
		auto cl = new FluidColumnLayoutManager(this);

		cl->SetColumnWeight(InsertColumn(0, _S("Available groups")), 1);

		Bind(wxEVT_LIST_END_LABEL_EDIT, [&](wxListEvent &ev) {
			ev.Skip();

			auto name = fz::to_utf8(ev.GetItem().GetText());
			fz::trim(name);

			if (name.empty())
				return ev.Veto();

			if (name.find_first_of(groups::invalid_chars_in_name) != std::string::npos)
				return ev.Veto();

			if (ev.GetIndex() < 0 || static_cast<size_t>(ev.GetIndex()) >= item2group_.size()) {
				return ev.Veto();
			}

			auto old_it = item2group_[(size_t)ev.GetIndex()];
			auto [new_it, inserted] = groups_->try_emplace(name, std::move(old_it->second));

			// Tried to use an already existing name?
			if (!inserted) {
				ev.Veto();
				return;
			}

			auto old_name = old_it->first;

			if (!GroupsList::Event::Changing.Process(this, GetParent(), old_name, name).IsAllowed()) {
				groups_->erase(new_it);
				ev.Veto();
				return;
			}

			groups_->erase(old_it);
			item2group_[(size_t)ev.GetIndex()] = new_it;

			SetItemState(ev.GetIndex(), wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
			GroupsList::Event::SelectedGroup.Process(this, GetParent());

			GroupsList::Event::Changed.Process(this, GetParent(), old_name, new_it->first);
		});

		Bind(Event::ItemSelecting, [&](Event &ev) {
			if (ev.GetItem() != selected_item_ && !CanDeselectItem())
				ev.Veto();
		});

		Bind(wxEVT_LIST_ITEM_SELECTED, [&](wxListEvent &ev) {
			ev.Skip();
			OnItemSelected(ev.GetIndex());
		});
	}

	void SetGroups(fz::authentication::file_based_authenticator::groups &groups)
	{
		groups_ = &groups;

		item2group_.clear();
		item2group_.reserve(groups.size());
		for (auto it = groups.begin(); it != groups.end(); ++it)
			item2group_.push_back(it);

		std::sort(item2group_.begin(), item2group_.end(), [](const auto &lhs, const auto &rhs) {
			return lhs->first < rhs->first;
		});

		SetItemCount((long)groups.size());

		if (GetItemCount() > 0) {
			SetItemState(0, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
			selected_item_ = 0;
		}
		else
			selected_item_ = -1;

		Refresh();
		Layout();

		// Give the client a chance to react to the fact that the list might have been completely emptied
		if (selected_item_ == -1)
			GroupsList::Event::SelectedGroup.Process(this, GetParent());
	}

	groups::value_type *GetSelectedGroup() const {
		if (selected_item_ < 0 || static_cast<size_t>(selected_item_) >= item2group_.size()) {
			return nullptr;
		}

		return std::addressof(*item2group_[(size_t)selected_item_]);
	}

	bool Append(const groups::value_type *copy = nullptr)
	{
		if (!CanDeselectItem())
			return false;

		if (!copy) {
			static const groups::value_type default_value { "New Group", {} };
			copy = &default_value;
		}

		std::string new_name = copy->first;

		if (groups_->count(new_name) != 0) {
			static const auto get_name_and_index = [](std::string_view name) {
				std::size_t idx = 0;

				// Try to parse the index number between parens
				fz::util::parseable_range r(name.crbegin(), name.crend());
				if (lit(r, ')') && until_lit(r, '(')) {
					auto open_paren_it = (++r.it).base();

					// This should encompass the number we're looking for
					fz::util::parseable_range n(std::next(open_paren_it), std::prev(name.cend()));
					if (parse_int(n, idx) && eol(n)) {
						auto new_name = name.substr(0, std::size_t(open_paren_it-name.cbegin()));
						if (!new_name.empty()) {
							if (new_name.back() == ' ')
								new_name.remove_suffix(1);

							name = new_name;
						}
						else
							idx = 0;
					}
				}

				return std::pair{name, idx};
			};

			auto [name, idx] = get_name_and_index(copy->first);

			do {
				new_name = fz::sprintf("%s (%zu)", name, ++idx);
			} while (groups_->count(new_name) != 0);
		}

		if (!GroupsList::Event::Changing.Process(this, GetParent(), std::string(), new_name).IsAllowed())
			return false;

		auto [it, inserted] = groups_->insert(groups::value_type{std::move(new_name), copy ? copy->second : groups::mapped_type{}});
		if (!inserted)
			return false;

		item2group_.push_back(it);

		SetItemCount((long)groups_->size());

		SetItemState(GetItemCount()-1, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
		RefreshItem(GetItemCount()-1);

		GroupsList::Event::Changed.Process(this, GetParent(), std::string(), new_name);

		EditLabel(GetItemCount()-1);

		return true;
	}

	void RenameSelected()
	{
		if (selected_item_ < 0)
			return;

		EditLabel(selected_item_);
	}

	void RemoveSelected()
	{
		if (selected_item_ < 0 || static_cast<size_t>(selected_item_) >= item2group_.size()) {
			return;
		}

		auto it = item2group_[(std::size_t)selected_item_];

		auto old_name = it->first;

		if (!GroupsList::Event::Changing.Process(this, GetParent(), old_name, std::string()).IsAllowed())
			return;

		groups_->erase(it);
		item2group_.erase(item2group_.begin() + selected_item_);

		SetItemCount((long)groups_->size());
		Refresh();

		if (selected_item_ == GetItemCount())
			selected_item_ -= 1;

		if (selected_item_ >= 0)
			SetItemState(selected_item_, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);

		GroupsList::Event::SelectedGroup.Process(this, GetParent());
		GroupsList::Event::Changed.Process(this, GetParent(), old_name, std::string());
	}

	bool CanDeselectItem()
	{
		if (selected_item_ < 0)
			return true;

		bool allowed = GroupsList::Event::AboutToDeselectGroup.Process(this, GetParent()).IsAllowed();

		return allowed;
	}

	size_t size() const
	{
		return groups_->size();
	}

private:
	void OnItemSelected(long item)
	{
		if (item == selected_item_)
			return;

		if (!CanDeselectItem()) {
			SetItemState(selected_item_, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
			return;
		}

		selected_item_ = item;

		GroupsList::Event::SelectedGroup.Process(this, GetParent());
	}

	wxString OnGetItemText(long item, long column) const override
	{
		wxASSERT(groups_ != nullptr);
		wxCHECK(0 <= item && item <= (long)groups_->size(), wxEmptyString);
		wxCHECK(column == 0, wxEmptyString);

		return fz::to_wxString(item2group_[(size_t)item]->first);
	}

	wxListItemAttr *OnGetItemAttr(long item) const override
	{
		wxASSERT(groups_ != nullptr);
		wxCHECK(0 <= item && item <= (long)groups_->size(), nullptr);

		return nullptr;
	}

	groups *groups_;
	std::vector<groups::iterator> item2group_;
	long selected_item_ = -1;
};

GroupsList::GroupsList(wxWindow *parent)
	: wxPanel(parent)
{
	wxVBox(this, 0) = {
		{ 1, list_ = new List(this) },

		{ 0, wxGBox(this, 2, {0, 1}) = {
			{ 0, add_button_ = new wxButton(this, wxID_ANY, _S("A&dd")) },
			{ 0, remove_button_ = new wxButton(this, wxID_ANY, _S("Re&move")) },

			{ 0, duplicate_button_ = new wxButton(this, wxID_ANY, _S("D&uplicate")) },
			{ 0, rename_button_ = new wxButton(this, wxID_ANY, _S("Re&name")) },
		}}
	};

	Bind(Event::SelectedGroup, [&](Event &ev){
		ev.Skip();

		auto selected_group = list_->GetSelectedGroup();
		auto enabled = selected_group;

		remove_button_->Enable(enabled);
		duplicate_button_->Enable(enabled);
		rename_button_->Enable(enabled);
	});

	list_->Bind(wxEVT_LIST_BEGIN_LABEL_EDIT, [&](wxListEvent &ev) {
		ev.Skip();
		add_button_->Disable();
		duplicate_button_->Disable();
		rename_button_->Disable();
	});

	list_->Bind(wxEVT_LIST_END_LABEL_EDIT, [&](wxListEvent &ev) {
		ev.Skip();
		if (list_->GetItemCount() > 0) {
			add_button_->Enable();
			rename_button_->Enable();
			duplicate_button_->Enable();
		}
	});

	add_button_->Bind(wxEVT_BUTTON, [&](auto) {
		if (!list_->Append())
			return;

		remove_button_->Enable();
		duplicate_button_->Disable();
		rename_button_->Disable();

	});

	remove_button_->Bind(wxEVT_BUTTON, [&](auto) {
		list_->EndEditLabel(true);
		list_->RemoveSelected();

		if (list_->GetItemCount() == 0) {
			remove_button_->Disable();
			rename_button_->Disable();
			duplicate_button_->Disable();
		}
	});

	duplicate_button_->Bind(wxEVT_BUTTON, [&](auto){
		if (!list_->Append(list_->GetSelectedGroup()))
			return;

		remove_button_->Enable();
		duplicate_button_->Disable();
		rename_button_->Disable();

	});

	rename_button_->Bind(wxEVT_BUTTON, [&](auto){
		list_->RenameSelected();
	});
}

void GroupsList::SetGroups(groups &groups)
{
	list_->SetGroups(groups);
}

GroupsList::groups::value_type *GroupsList::GetSelectedGroup() const
{
	return list_->GetSelectedGroup();
}

size_t GroupsList::size() const
{
	return list_->size();
}
