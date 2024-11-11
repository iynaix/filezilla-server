#ifndef WXFZFTPUSERSLIST_HPP
#define WXFZFTPUSERSLIST_HPP

#include <wx/panel.h>

#include "../filezilla/authentication/file_based_authenticator.hpp"
#include "eventex.hpp"

class wxButton;

class UsersList: public wxPanel
{
	using users = fz::authentication::file_based_authenticator::users;

public:
	struct Event;

	UsersList(wxWindow *parent);

	void SetUsers(users &users);
	users::value_type *GetSelectedUser() const;
	size_t size() const;

private:
	class List;

	List *list_;
	wxButton *add_button_;
	wxButton *remove_button_;
	wxButton *duplicate_button_;
	wxButton *rename_button_;
};

struct UsersList::Event: wxEventEx<Event>
{
	bool IsAllowed() const {
		return allowed_;
	}

	void Veto() {
		allowed_ = false;
	}

	const std::string &GetOldName() const {
		return old_name_;
	}

	const std::string &GetNewName() const {
		return new_name_;
	}

	inline const static Tag SelectedUser;
	inline const static Tag AboutToDeselectUser;
	inline const static Tag Changing;
	inline const static Tag Changed;

private:
	friend Tag;

	using wxEventEx::wxEventEx;

	Event(const Tag &tag, const std::string &old_name, const std::string new_name)
		: wxEventEx(tag)
		, old_name_(old_name)
		, new_name_(new_name)
	{}

	bool allowed_ = true;

	std::string old_name_;
	std::string new_name_;
};

#endif // WXFZFTPUSERSLIST_HPP
