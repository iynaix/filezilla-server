#ifndef WXFZFTPGROUPSLIST_HPP
#define WXFZFTPGROUPSLIST_HPP

#include <wx/panel.h>

#include "../filezilla/authentication/file_based_authenticator.hpp"
#include "eventex.hpp"

class wxButton;

class GroupsList: public wxPanel
{
	using groups = fz::authentication::file_based_authenticator::groups;

public:
	struct Event;

	GroupsList(wxWindow *parent);

	void SetGroups(groups &groups);
	groups::value_type *GetSelectedGroup() const;
	size_t size() const;

private:
	class List;

	List *list_;
	wxButton *add_button_;
	wxButton *remove_button_;
	wxButton *duplicate_button_;
	wxButton *rename_button_;
};

struct GroupsList::Event: wxEventEx<Event>
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

	inline const static Tag SelectedGroup;
	inline const static Tag AboutToDeselectGroup;
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

#endif // WXFZFTPGROUPSLIST_HPP
