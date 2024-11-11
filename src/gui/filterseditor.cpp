#include <libfilezilla/glue/wx.hpp>

#include <wx/sizer.h>
#include <wx/stattext.h>

#include "filterseditor.hpp"
#include "locale.hpp"
#include "helpers.hpp"

bool FiltersEditor::Create(wxWindow *parent, wxWindowID winid, const wxPoint &pos, const wxSize &size, long style, const wxString &name)
{
	if (!wxPanel::Create(parent, winid, pos, size, style, name))
		return false;

	wxVBox(this, 0) = {
		{ 0, wxLabel(this, _S("D&isallowed IP ranges:")) },
		{ 1, disallowed_editor_ = wxCreate<BinaryAddressListEditor>(this) },

		{ 0, wxLabel(this, _S("A&llowed IP ranges:")) },
		{ 1, allowed_editor_ = wxCreate<BinaryAddressListEditor>(this) }
	};

	return true;
}

void FiltersEditor::SetIps(fz::tcp::binary_address_list *disallowed_ips, fz::tcp::binary_address_list *allowed_ips)
{
	disallowed_editor_->SetIps(disallowed_ips);
	allowed_editor_->SetIps(allowed_ips);
}
