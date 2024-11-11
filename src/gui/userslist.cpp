#include <libfilezilla/glue/wx.hpp>
#include <libfilezilla/format.hpp>

#include <wx/listctrl.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/timer.h>

#include "fluidcolumnlayoutmanager.hpp"
#include "helpers.hpp"
#include "../filezilla/util/parser.hpp"

#include "userslist.hpp"

#include "locale.hpp"
#include "glue.hpp"
#include "listctrlex.hpp"

class UsersList::List: public wxListCtrlEx
{
	static int constexpr timer_ms = 10;

public:
	List(UsersList *parent)
		: wxListCtrlEx(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
					 wxTAB_TRAVERSAL | wxLC_VIRTUAL | wxLC_REPORT | wxLC_EDIT_LABELS | wxLC_SINGLE_SEL)
	{
		auto cl = new FluidColumnLayoutManager(this);

		cl->SetColumnWeight(InsertColumn(0, _S("Available users")), 1);

		Bind(wxEVT_LIST_BEGIN_LABEL_EDIT, [&](wxListEvent &ev) {
			if (ev.GetIndex() == 0)
				ev.Veto();
			else
				ev.Skip();
		});

		Bind(wxEVT_LIST_END_LABEL_EDIT, [&](wxListEvent &ev) {
			ev.Skip();

			auto name = fz::to_utf8(ev.GetItem().GetText());
			fz::trim(name);

			if (name.empty() || name == users::system_user_name)
				return ev.Veto();

			if (name.find_first_of(users::invalid_chars_in_name) != std::string::npos)
				return ev.Veto();

			if (ev.GetIndex() < 0 || static_cast<size_t>(ev.GetIndex()) >= item2user_.size()) {
				return ev.Veto();
			}

			auto old_it = item2user_[(size_t)ev.GetIndex()];
			auto [new_it, inserted] = users_->try_emplace(name, std::move(old_it->second));

			// Tried to use an already existing name?
			if (!inserted) {
				ev.Veto();
				return;
			}

			auto old_name = old_it->first;

			if (!UsersList::Event::Changing.Process(this, GetParent(), old_name, name).IsAllowed()) {
				users_->erase(new_it);
				ev.Veto();
				return;
			}

			users_->erase(old_it);
			item2user_[(size_t)ev.GetIndex()] = new_it;

			SetItemState(ev.GetIndex(), wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
			UsersList::Event::SelectedUser.Process(this, GetParent());

			UsersList::Event::Changed.Process(this, GetParent(), old_name, new_it->first);
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

	void SetUsers(fz::authentication::file_based_authenticator::users &users)
	{
		users_ = &users;

		item2user_.clear();
		item2user_.reserve(users.size());
		for (auto it = users.begin(); it != users.end(); ++it)
			item2user_.push_back(it);

		std::sort(item2user_.begin(), item2user_.end(), [](const auto &lhs, const auto &rhs) {
			if (lhs->first == users::system_user_name) {
				return rhs->first != users::system_user_name;
			}
			if (rhs->first == users::system_user_name) {
				return false;
			}
			return lhs->first < rhs->first;
		});

		SetItemCount((long)users.size());

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
			UsersList::Event::SelectedUser.Process(this, GetParent());
	}

	users::value_type *GetSelectedUser() const {
		if (selected_item_ < 0 || static_cast<size_t>(selected_item_) >= item2user_.size()) {
			return nullptr;
		}

		return std::addressof(*item2user_[(size_t)selected_item_]);
	}

	bool Append(const users::value_type *copy = nullptr)
	{
		if (!CanDeselectItem())
			return false;

		if (!copy) {
			static const users::value_type default_value { "New User", {} };
			copy = &default_value;
		}

		std::string new_name = copy->first;

		if (users_->count(new_name) != 0) {
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
			} while (users_->count(new_name) != 0);
		}

		if (!UsersList::Event::Changing.Process(this, GetParent(), std::string(), new_name).IsAllowed())
			return false;

		auto [it, inserted] = users_->insert(users::value_type{std::move(new_name), copy ? copy->second : users::mapped_type{}});
		if (!inserted)
			return false;

		item2user_.push_back(it);

		SetItemCount((long)users_->size());

		SetItemState(GetItemCount()-1, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
		RefreshItem(GetItemCount()-1);

		UsersList::Event::Changed.Process(this, GetParent(), std::string(), new_name);

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
		if (selected_item_ < 0 || static_cast<size_t>(selected_item_) >= item2user_.size()) {
			return;
		}

		auto it = item2user_[(std::size_t)selected_item_];

		auto old_name = it->first;

		if (!UsersList::Event::Changing.Process(this, GetParent(), old_name, std::string()).IsAllowed())
			return;

		users_->erase(it);
		item2user_.erase(item2user_.begin() + selected_item_);

		SetItemCount((long)users_->size());
		Refresh();

		if (selected_item_ == GetItemCount())
			selected_item_ -= 1;

		if (selected_item_ >= 0)
			SetItemState(selected_item_, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);

		UsersList::Event::SelectedUser.Process(this, GetParent());
		UsersList::Event::Changed.Process(this, GetParent(), old_name, std::string());
	}

	bool CanDeselectItem()
	{
		if (selected_item_ < 0)
			return true;

		bool allowed = UsersList::Event::AboutToDeselectUser.Process(this, GetParent()).IsAllowed();

		return allowed;
	}

	size_t size() const
	{
		return users_->size();
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

		UsersList::Event::SelectedUser.Process(this, GetParent());
	}

	wxString OnGetItemText(long item, long column) const override
	{
		wxASSERT(users_ != nullptr);
		wxCHECK(0 <= item && item <= (long)users_->size(), wxEmptyString);
		wxCHECK(column == 0, wxEmptyString);

		return fz::to_wxString(item2user_[(size_t)item]->first);
	}

	wxListItemAttr *OnGetItemAttr(long item) const override
	{
		wxASSERT(users_ != nullptr);
		wxCHECK(0 <= item && item <= (long)users_->size(), nullptr);

		if (item == 0 && item2user_[0]->first == users::system_user_name) {
			static wxListItemAttr &attr = [](const wxWindow *p) -> wxListItemAttr & {
				static wxListItemAttr attr;

				attr.SetFont(p->GetFont().MakeItalic());

				return attr;
			}(this);

			return &attr;
		}

		return nullptr;
	}

	users *users_;
	std::vector<users::iterator> item2user_;
	long selected_item_ = -1;
};

UsersList::UsersList(wxWindow *parent)
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

	Bind(Event::SelectedUser, [&](Event &ev){
		ev.Skip();

		auto selected_user = list_->GetSelectedUser();
		auto enabled = selected_user && selected_user->first != users::system_user_name;

		remove_button_->Enable(enabled);
		duplicate_button_->Enable(enabled);
		rename_button_->Enable(enabled);
	});

	list_->Bind(wxEVT_LIST_BEGIN_LABEL_EDIT, [&](wxListEvent &ev) {
		if (ev.GetIndex() == 0)
			return ev.Veto();

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
		if (!list_->Append(list_->GetSelectedUser()))
			return;

		remove_button_->Enable();
		duplicate_button_->Disable();
		rename_button_->Disable();

	});

	rename_button_->Bind(wxEVT_BUTTON, [&](auto){
		list_->RenameSelected();
	});
}

void UsersList::SetUsers(users &users)
{
	list_->SetUsers(users);
}

UsersList::users::value_type *UsersList::GetSelectedUser() const
{
	return list_->GetSelectedUser();
}

size_t UsersList::size() const
{
	return list_->size();
}
