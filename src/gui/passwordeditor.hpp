#ifndef PASSWORDEDITOR_HPP
#define PASSWORDEDITOR_HPP

#include "../filezilla/authentication/password.hpp"

#include <wx/panel.h>
#include "eventex.hpp"

class wxTextCtrl;
class wxCheckBox;

class PasswordEditor: public wxPanel
{
public:
	PasswordEditor() = default;

	bool Create(wxWindow *parent,
				bool allow_no_password = true,
				wxWindowID winid = wxID_ANY,
				const wxPoint& pos = wxDefaultPosition,
				const wxSize& size = wxDefaultSize,
				long style = wxTAB_TRAVERSAL | wxNO_BORDER,
				const wxString& name = wxS("passwordeditor"));


	/// \brief Sets the password to be edited.
	/// *Doesn't* take ownership of the pointer.
	void SetPassword(fz::authentication::any_password *password);

	struct Event: wxEventEx<Event>
	{
		inline const static Tag Changed;

		using wxEventEx::wxEventEx;
	};

	bool HasPassword() const;
	bool CheckIsStrongEnough();

	bool TransferDataToWindow() override;
	bool TransferDataFromWindow() override;

private:
	wxTextCtrl *password_text_{};
	wxCheckBox *password_enabler_{};
	fz::authentication::any_password *password_{};
};

#include "helpers.hpp"

// BUG: Clang requires these: clang's bug or something I don't understand about C++?
extern template struct wxValidateOnlyIfCurrentPage<PasswordEditor>;
extern template struct wxCreator::CtrlFix<wxValidateOnlyIfCurrentPage<PasswordEditor>>;

#endif // PASSWORDEDITOR_HPP
