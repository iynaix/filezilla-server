#ifndef WXFZFTPBINARYADDRESSLISTEDITOR_HPP
#define WXFZFTPBINARYADDRESSLISTEDITOR_HPP

#include <wx/panel.h>

#include "../filezilla/tcp/binary_address_list.hpp"

class wxTextCtrl;

class BinaryAddressListEditor: public wxPanel
{
public:
	BinaryAddressListEditor() = default;

	bool Create(wxWindow *parent,
				wxWindowID winid = wxID_ANY,
				const wxPoint& pos = wxDefaultPosition,
				const wxSize& size = wxDefaultSize,
				long style = wxTAB_TRAVERSAL | wxNO_BORDER,
				const wxString& name = wxS("BinaryAddressListEditor"));

	//! Sets the list the editor will have to display and let the user edit.
	//! It *doesn't* take ownership of the list.
	void SetIps(fz::tcp::binary_address_list *address_list);

private:
	bool TransferDataFromWindow() override;

	wxTextCtrl *text_{};
	fz::tcp::binary_address_list *list_{};
};

#endif // WXFZFTPBINARYADDRESSLISTEDITOR_HPP
