#ifndef WXREARRANGEDITEMSPICKERCTRL_HPP
#define WXREARRANGEDITEMSPICKERCTRL_HPP

#include <vector>
#include <string>

#include <wx/panel.h>

class wxComboCtrl;

class RearrangedItemsPickerCtrl: public wxPanel
{
public:
	RearrangedItemsPickerCtrl() = default;
	RearrangedItemsPickerCtrl(wxWindow *parent);
	bool Create(wxWindow *parent);

	void SetAvailableItems(wxArrayString available);
	void SetActiveItems(const std::vector<std::string> &active);
	std::size_t GetActiveItems(std::vector<std::string> &active);

	bool IsModified() const;
	void DiscardEdits();

private:
	class CheckListBox;
	class Popup;

	wxComboCtrl *combo_;
	CheckListBox *list_;

	wxArrayString available_;
	std::vector<std::string> active_;

	wxString GetStringValue() const;
};

#endif // WXREARRANGEDITEMSPICKERCTRL_HPP
