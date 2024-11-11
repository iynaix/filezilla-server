#ifndef GENERATECERTIFICATEDIALOG_HPP
#define GENERATECERTIFICATEDIALOG_HPP


#include <string>
#include <vector>

#include <libfilezilla/tls_params.hpp>

#include <wx/propdlg.h>
#include "dialogex.hpp"
#include "tls_param_editor.hpp"

class wxTextCtrl;
class TlsParamEditor;

class GenerateCertificateDialog: public wxDialogEx<wxPropertySheetDialog>
{
public:
	GenerateCertificateDialog();

	bool Create(wxWindow *parent,
				const wxString &title,
				wxWindowID winid = wxID_ANY,
				const wxPoint& pos = wxDefaultPosition,
				const wxSize& size = wxDefaultSize,
				long style = wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER,
				const wxString& name = wxS("GenerateCertificateDialog"));

	void SetKey(fz::tls_param *key, fz::native_string *password, TlsParamEditor::RetrieveDeobfuscatedBlobFunc func, fz::util::fs::path_format server_path_format);
	void SetDistinguishedName(std::string *dn);
	void SetHostnames(std::vector<std::string> *hostnames, std::size_t minimum_number_of_hostnames, bool at_least_2dn_level);

private:
	wxBookCtrlBase* CreateBookCtrl() override;
	bool TransferDataToWindow() override;

	fz::tls_param *key_{};
	fz::native_string *password_{};
	std::string *dn_{};
	std::vector<std::string> *hostnames_{};
	std::size_t minimum_number_of_hostnames_{};
	bool at_least_2nd_level_{};

	wxWindow *key_book_ctrl_{};
	TlsParamEditor *key_ctrl_{};
	wxTextCtrl *key_pass_ctrl_{};
	wxTextCtrl *dn_ctrl_{};
	wxTextCtrl *hostnames_ctrl_{};
};

#endif // GENERATECERTIFICATEDIALOG_HPP
