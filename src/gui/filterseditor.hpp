#ifndef FILTERSEDITOR_HPP
#define FILTERSEDITOR_HPP

#include <wx/panel.h>

#include "binaryaddresslisteditor.hpp"

class FiltersEditor: public wxPanel
{
public:
	FiltersEditor() = default;

	bool Create(wxWindow *parent,
				wxWindowID winid = wxID_ANY,
				const wxPoint& pos = wxDefaultPosition,
				const wxSize& size = wxDefaultSize,
				long style = wxTAB_TRAVERSAL | wxNO_BORDER,
				const wxString& name = wxS("filterseditor"));

	//! Sets the ips the editor will have to display and let the user edit.
	//! It *doesn't* take ownership of the objects.
	void SetIps(fz::tcp::binary_address_list *disallowed_ips, fz::tcp::binary_address_list *allowed_ips);

private:
	BinaryAddressListEditor *allowed_editor_;
	BinaryAddressListEditor *disallowed_editor_;
};

#endif // FILTERSEDITOR_HPP
