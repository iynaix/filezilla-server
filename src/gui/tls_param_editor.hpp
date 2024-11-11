#ifndef TLS_PARAM_EDITOR_HPP
#define TLS_PARAM_EDITOR_HPP

#include <wx/panel.h>

#include <libfilezilla/tls_params.hpp>

#include "../filezilla/util/filesystem.hpp"
#include "../filezilla/expected.hpp"

#include "eventex.hpp"

class wxSimplebook;
class wxChoicebook;
class wxTextCtrl;
class wxButton;
class wxChoice;

enum : unsigned {
	TLS_PE_NONE      = 0b00000,
	TLS_PE_BLOB      = 0b00001,
	TLS_PE_FILEPATH  = 0b00010,
	TLS_PE_PKCS11URL = 0b00100,
	TLS_PE_READONLY  = 0b01000,
};

class TlsParamEditor: public wxPanel
{
public:
	using RetrieveDeobfuscatedBlobFunc = std::function<fz::expected<std::string, wxString> (std::string_view obfuscated)>;

	TlsParamEditor(wxWindow *parent, unsigned style, const wxString &name = wxT("TlsParamEditor"), unsigned selected = 0);

	void SetValue(fz::tls_param *param, fz::util::fs::path_format server_path_format, bool check_modified = false);
	wxChoice *GetChoiceCtrl();
	void SetRetrieveDeobfuscatedBlobFunction(RetrieveDeobfuscatedBlobFunc func);
	bool IsModified();
	void SetModified(bool);

	bool IsCurrentlyObfuscatedAndNotModified();
	bool SetObfuscated(const fz::tls_param &);

	struct Event: wxEventEx<Event>
	{
		inline const static Tag Modified;

		Event(const Tag tag, bool modified)
			: wxEventEx(tag)
			, modified(modified)
		{}

		bool modified{};
	};

private:
	struct Useful;

	bool TransferDataFromWindow() override;

	wxSizer *create_main_editors(wxWindow *p);
	void edit_blob();
	bool act_on_data_provided(std::string_view blob);
	void check_modified();

	Useful first_useful();

	fz::tls_param *param_{};

	wxChoicebook *main_book_{};
	wxSimplebook *outer_book_{};

	wxTextCtrl *blob_ctrl_{};
	wxButton *edit_blob_ctrl_{};
	wxTextCtrl *filepath_ctrl_{};
	wxTextCtrl *pkcs11url_ctrl_{};
	wxTextCtrl *unsupported_ctrl_{};

	wxWindow *blob_page_{};
	wxWindow *filepath_page_{};
	wxWindow *pkcs11url_page_{};

	bool blob_modified_{};
	bool pkcs11url_modified_{};
	bool filepath_modified_{};

	wxWindow *main_page_{};
	wxWindow *unsupported_page_{};

	unsigned style_;
	unsigned selected_;
	std::string blob_data_;
	RetrieveDeobfuscatedBlobFunc retrieve_deobfuscated_func_;
	fz::util::fs::path_format server_path_format_;
};

#endif // TLS_PARAM_EDITOR_HPP

