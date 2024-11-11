#include <libfilezilla/glue/wx.hpp>
#include <libfilezilla/string.hpp>


#include <wx/textctrl.h>
#include <wx/log.h>
#include <wx/choicebk.h>
#include <wx/button.h>
#include <wx/statline.h>
#include <wx/simplebook.h>
#include <wx/checkbox.h>
#include <wx/dcclient.h>
#include <wx/settings.h>

#include "certinfoeditor.hpp"
#include "generatecertificatedialog.hpp"

#include "helpers.hpp"
#include "locale.hpp"
#include "glue.hpp"
#include "tls_param_editor.hpp"
#include "textvalidatorex.hpp"

#include "../filezilla/string.hpp"

CertInfoEditor::cert_details::cert_details(wxWindow *parent)
	: wxPanel(parent)
{
	wxStaticVBox(this, _S("Information about the certificate")) = wxGBox(this, 2, {1}) = {
		wxLabel(this, _S("Fingerprint (SHA-256):")),
		fingerprint_ctrl_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY),

		wxLabel(this, _S("Activation date:")),
		activation_date_ctrl_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY),

		wxLabel(this, _S("Expiration date:")),
		expiration_date_ctrl_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY),

		wxLabel(this, _S("Distinguished name:")),
		distinguished_name_ctrl_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY),

		wxLabel(this, _S("Applicable hostnames:")),
		hostnames_ctrl_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY)
	};

	Disable();
}

void CertInfoEditor::cert_details::SetWaiting()
{
	Disable();
	Clear();

	fingerprint_ctrl_->SetValue(_S("Waiting for new fingerprint..."));
	activation_date_ctrl_->Clear();
	expiration_date_ctrl_->Clear();
	distinguished_name_ctrl_->Clear();
	hostnames_ctrl_->Clear();
}

void CertInfoEditor::cert_details::Clear()
{
	fingerprint_ctrl_->Clear();
	activation_date_ctrl_->Clear();
	expiration_date_ctrl_->Clear();
	distinguished_name_ctrl_->Clear();
	hostnames_ctrl_->Clear();
}

void CertInfoEditor::cert_details::SetValue(const fz::securable_socket::omni_cert_info::extra *e)
{
	if (!e || !e->activation_time || !e->expiration_time) {
		Disable();
		Clear();
		return;
	}

	Enable();

	fingerprint_ctrl_->SetValue(fz::to_wxString(e->fingerprint));
	activation_date_ctrl_->SetValue(fz::to_wxString(e->activation_time));
	expiration_date_ctrl_->SetValue(fz::to_wxString(e->expiration_time));
	distinguished_name_ctrl_->SetValue(fz::to_wxString(e->distinguished_name));
	hostnames_ctrl_->SetValue(fz::join<wxString>(e->hostnames));
}

static void make_equal_fitting_sizes(wxWindow *one, wxWindow *two)
{
	auto one_size = one->GetSize();
	auto two_size = two->GetSize();
	one_size.IncTo(two_size);
	two_size.IncTo(one_size);
	one->SetMinSize(two_size);
	two->SetMinSize(one_size);
}

CertInfoEditor::cert_form::cert_form(wxWindow *parent, bool generate, std::size_t source_id)
	: wxPanel(parent)
	, source_id_(source_id)
{
	auto p = this;

	auto read_only = generate
		? TLS_PE_READONLY
		: TLS_PE_NONE;

	auto box = wxVBox(p, 0) = {
		wxLabel(p, _S("&Certificate:")),
		certs_ctrl_ = new TlsParamEditor(p, TLS_PE_BLOB | TLS_PE_FILEPATH | read_only, _S("Certificate")),

		wxLabel(p, _S("Private &key:")),
		key_ctrl_ = new TlsParamEditor(p, TLS_PE_BLOB | TLS_PE_FILEPATH | TLS_PE_PKCS11URL | read_only, _S("Private key")),
	};

	if (generate) {
		box += generate_ctrl_ = new wxButton(p, wxID_ANY, _S("&Generate new"));
	}
	else {
		box += {
			wxLabel(p, _S("Private key &password (stored in plaintext):")),
			key_pass_ctrl_ = new wxTextCtrl(p, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD | (read_only ? wxTE_READONLY : 0)),
		};
	}

	make_equal_fitting_sizes(certs_ctrl_->GetChoiceCtrl(), key_ctrl_->GetChoiceCtrl());

	wxTransferDataFromWindow(p, [this] {
		if (generate_ctrl_) {
			if (generate_ctrl_->GetLabel() == _S("Generating...")) {
				wxMsg::Error(_S("Still waiting for the new fingerprint.\nIn case of troubles, hit the CANCEL button and enter the settings dialog again.")).Wait();
				return false;
			}

			if (!my_omni_.certs) {
				wxMsg::Error(_S("You must either generate a certificate, or provide your own")).Wait();
				return false;
			}
		}

		if (test_func_) {
			if (details_) {
				details_->SetWaiting();
			}

			wxString error = test_func_(my_omni_, my_extra_);

			if (!error.empty()) {
				wxMsg::Error(_F("Error in %s.", GetName())).Ext(error).Wait();

				if (details_) {
					details_->SetValue(nullptr);
				}

				details_->Clear();
				return false;
			}
			else {
				details_->SetValue(&my_extra_);
			}
		}

		if (their_omni_) {
			*their_omni_ = my_omni_;
		}

		if (their_extra_) {
			*their_extra_ = my_extra_;
		}

		certs_ctrl_->SetModified(false);
		key_ctrl_->SetModified(false);
		key_pass_modified_ = false;
		return true;
	});

	if (!read_only) {
		auto act_on_modified = [this] {
			if (details_) {
				if (IsModified()) {
					details_->Clear();
				}
				else {
					details_->SetValue(&my_extra_);
				}
			}
		};

		certs_ctrl_->Bind(TlsParamEditor::Event::Modified, [act_on_modified](auto &) {
			act_on_modified();
		});

		key_ctrl_->Bind(TlsParamEditor::Event::Modified, [act_on_modified](auto &) {
			act_on_modified();
		});

		key_pass_ctrl_->Bind(wxEVT_TEXT, [this, act_on_modified](auto &) {
			key_pass_modified_ = fz::to_native(key_pass_ctrl_->GetValue()) != my_omni_.key_password;
			act_on_modified();
		});
	}
}

void CertInfoEditor::cert_form::SetDetails(CertInfoEditor::cert_details *details)
{
	details_ = details;
}

void CertInfoEditor::cert_form::SetTestCertificateFunction(TestCertificateFunc func)
{
	test_func_ = std::move(func);
}

void CertInfoEditor::cert_form::SetRetrieveDeobfuscatedBlobFunction(RetrieveDeobfuscatedBlobFunc func)
{
	retrieve_deobfuscated_func_ = func;

	certs_ctrl_->SetRetrieveDeobfuscatedBlobFunction(func);
	key_ctrl_->SetRetrieveDeobfuscatedBlobFunction(std::move(func));
}

bool CertInfoEditor::cert_form::SetGenerating(bool generating)
{
	if (generate_ctrl_) {
		generate_ctrl_->SetLabel(generating ? _S("Generating...") : _S("&Generate new"));
		generate_ctrl_->Enable(!generating);

		if (generating) {
			if (details_) {
				details_->SetWaiting();
			}
		}

		if (!generating) {
			certs_ctrl_->SetValue(&my_omni_.certs, server_path_format_, true);
			key_ctrl_->SetValue(&my_omni_.key, server_path_format_, true);

			if (details_) {
				details_->SetValue(&my_extra_);
			}
		}

		return generating;
	}

	return false;
}

bool CertInfoEditor::cert_form::IsModified()
{
	return certs_ctrl_->IsModified() || key_ctrl_->IsModified() || key_pass_modified_;
}

bool CertInfoEditor::cert_form::SetObfuscatedCert(const fz::securable_socket::omni_cert_info &o)
{
	return key_ctrl_->SetObfuscated(o.key);
}

bool CertInfoEditor::cert_form::IsCurrentlyObfuscatedAndNotModified()
{
	return key_ctrl_->IsCurrentlyObfuscatedAndNotModified();
}

bool CertInfoEditor::IsModified()
{
	return provided_generation_->IsModified() || autogenerated_generation_->IsModified() || (acme_generation_ && acme_generation_->IsModified());
}

bool CertInfoEditor::IsCurrentlyObfuscatedAndNotModified()
{
	if (cert_info_ && cert_info_->omni()) {
		auto o = cert_info_->omni();

		if (o->provided()) {
			return provided_generation_->IsCurrentlyObfuscatedAndNotModified();
		}

		if (o->autogenerated()) {
			return autogenerated_generation_->IsCurrentlyObfuscatedAndNotModified();
		}

		if (o->acme()) {
			return acme_generation_->IsCurrentlyObfuscatedAndNotModified();
		}
	}

	return {};
}

bool CertInfoEditor::SetObfuscatedCert(const fz::securable_socket::cert_info &their_cert_info)
{
	if (cert_info_ && cert_info_->omni() && their_cert_info.omni()) {
		auto o = cert_info_->omni();
		auto to = their_cert_info.omni();

		if (o->provided() && to->provided()) {
			return provided_generation_->SetObfuscatedCert(*to);
		}

		if (o->autogenerated() && to->autogenerated()) {
			return autogenerated_generation_->SetObfuscatedCert(*to);
		}

		if (o->acme() && to->autogenerated()) {
			return acme_generation_->SetObfuscatedCert(*to);
		}
	}

	return {};
}

bool CertInfoEditor::cert_form::SetValue(fz::securable_socket::omni_cert_info *omni, fz::securable_socket::omni_cert_info::extra *extra, fz::util::fs::path_format server_path_format)
{
	their_omni_ = omni;
	their_extra_ = extra;

	my_extra_ = {};
	server_path_format_ = server_path_format;

	bool its_me = false;

	if (their_omni_) {
		if (their_omni_->source.index() == source_id_) {
			its_me = true;

			my_omni_ = *their_omni_;
			if (extra) {
				my_extra_ = *extra;
			}

			if (details_) {
				details_->SetValue(&my_extra_);
			}
		}
		else {
			my_omni_ = { {}, {}, {}, fz::securable_socket::omni_cert_info::sources::from_id(source_id_) };
			if (details_) {
				details_->SetValue(nullptr);
			}
		}

		certs_ctrl_->SetValue(&my_omni_.certs, server_path_format_);
		key_ctrl_->SetValue(&my_omni_.key, server_path_format_);

		if (key_pass_ctrl_) {
			key_pass_ctrl_->SetValidator(TextValidatorEx(wxFILTER_NONE, &my_omni_.key_password));
			key_pass_ctrl_->TransferDataToWindow();
			key_pass_ctrl_->Enable();
		}

		if (generate_ctrl_) {
			generate_ctrl_->Enable();
		}
	}
	else {
		certs_ctrl_->SetValue(nullptr, {});
		key_ctrl_->SetValue(nullptr, {});

		if (key_pass_ctrl_) {
			key_pass_ctrl_->SetValidator(wxValidator());
			key_pass_ctrl_->Disable();
		}

		if (generate_ctrl_) {
			generate_ctrl_->Disable();
		}

		if (details_) {
			details_->Clear();
		}
	}

	return its_me;
}

bool CertInfoEditor::Create(wxWindow *parent, wxWindowID winid, const wxPoint &pos, const wxSize &size, long style, const wxString &name)
{
	if (!wxPanel::Create(parent, winid, pos, size, style, name))
		return false;

	using sources = fz::securable_socket::omni_cert_info::sources;

	wxVBox(this, 0) = {
		wxLabel(this, _S("&TLS credentials:")),
		book_ = new wxChoicebook(this, wxID_ANY) | [&](auto p) {
			provided_generation_ = wxPage<cert_form>(wxValidateOnlyIfCurrent)(p, _S("Provide a X.509 certificate and private key"), true, false, sources::id_of<sources::provided>()) | [&](auto p) {
				p->SetName(_S("X.509 certificate and private key"));
			};

			autogenerated_generation_ = wxPage<cert_form>(wxValidateOnlyIfCurrent)(p, _S("Use a self-signed X.509 certificate"), false, true, sources::id_of<sources::autogenerated>()) | [&](auto p) {
				p->SetName(_S("self-signed X.509 certificate"));

				p->generate_ctrl_->Bind(wxEVT_BUTTON, [this, p](auto) {
					if (!selfsigned_func_) {
						return;
					}

					if (!p->SetGenerating(true)) {
						return;
					}

					wxPushDialog<GenerateCertificateDialog>(this, _F("Data for %s", p->GetName())) | [this, p](GenerateCertificateDialog &diag) {
						std::string dn;
						auto hostnames = p->my_extra_.hostnames;
						auto key = p->my_omni_.key;
						auto key_password = p->my_omni_.key_password;

						diag.SetKey(&key, &key_password, p->retrieve_deobfuscated_func_, p->server_path_format_);
						diag.SetDistinguishedName(&dn);
						diag.SetHostnames(&hostnames, 0, false);

						wxTransferDataFromWindow(&diag, [&] {
							wxString error = selfsigned_func_(dn, hostnames, key, key_password,
								p->my_omni_, p->my_extra_
							);

							if (!error.empty()) {
								wxMsg::Error(_F("Error while generating %s.", p->GetName())).Ext(error).Wait();
								return false;
							}

							return true;
						});

						diag.ShowModal();

						p->SetGenerating(false);
					};
				});
			};
		},

		details_book_ = new wxSimplebook(this) |[&](auto p) {
			wxPage(p) |[&](auto p) {
				wxVBox(p, 0) = provided_details_ctrl_ = wxCreate<cert_details>(p);
			};

			wxPage(p) |[&](auto p) {
				wxVBox(p, 0) = autogenerated_details_ctrl_ = wxCreate<cert_details>(p);
			};

			provided_generation_->SetDetails(provided_details_ctrl_);
			autogenerated_generation_->SetDetails(autogenerated_details_ctrl_);
		}
	};

	book_->Bind(wxEVT_CHOICEBOOK_PAGE_CHANGED, [&](auto &ev) {
		if (ev.GetEventObject() != book_) {
			ev.Skip();
			return;
		}

		auto sel = book_->GetSelection();
		if (sel < 0)
			return;

		details_book_->ChangeSelection(std::size_t(sel));
	});

	SetValue(nullptr, {}, {});

	return true;
}

bool CertInfoEditor::create_acme_editor()
{
	if (acme_ctrl_) {
		return false;
	}

	if (!book_) {
		return false;
	}

	using sources = fz::securable_socket::omni_cert_info::sources;

	acme_ctrl_ = wxPage<wxNavigationEnabled<wxSimplebook>>(wxValidateOnlyIfCurrent)(book_, _S("Use a Let's Encrypt® certificate")) | [&](auto p) {
		acme_setup_page_ = wxPage(wxValidateOnlyIfCurrent)(p) | [&](auto p) {
			wxButton *setup;

			wxVBox(p, 0) = {
				setup = new wxButton(p, wxID_ANY, _S("Set up Let's &Encrypt® options first"))
			};

			setup->Bind(wxEVT_BUTTON, [this](auto) {
				if (!acme_opts_)
					return;

				if (!*acme_opts_ && switch_to_acme_opts_) {
					SetValue(cert_info_, extra_info_, server_path_format_);
					switch_to_acme_opts_();
				}
			});
		};

		acme_generation_page_ = wxPage(wxValidateOnlyIfCurrent)(p) | [&](auto p) {
			wxVBox(p, 0) = {
				acme_generation_ = wxCreate<cert_form>(p, true, sources::id_of<sources::acme>()),
				autorenew_acme_ctrl_ = new wxCheckBox(p, wxID_ANY, _S("Automatically try to renew the certificate in due time."))
			};

			acme_generation_->SetName(_S("Let's Encrypt® certificate"));

			acme_generation_->generate_ctrl_->Bind(wxEVT_BUTTON, [this, p = acme_generation_](auto) {
				if (!acme_func_) {
					return;
				}

				if (!p->SetGenerating(true)) {
					return;
				}

				wxPushDialog<GenerateCertificateDialog>(this, _F("Data for %s", p->GetName())) |[this, p](GenerateCertificateDialog &diag) {
					auto hostnames = p->my_extra_.hostnames;
					auto key = p->my_omni_.key;
					auto key_password = p->my_omni_.key_password;

					diag.SetKey(&key, &key_password, p->retrieve_deobfuscated_func_, p->server_path_format_);
					diag.SetHostnames(&hostnames, 1, true);

					wxTransferDataFromWindow(&diag, [&] {
						wxString error = acme_func_(hostnames, key, key_password,
							p->my_omni_, p->my_extra_
						);

						if (!error.empty()) {
							wxMsg::Error(_F("Error while generating %s.", p->GetName())).Ext(error).Wait();
							return false;
						}

						return true;
					});

					diag.ShowModal();

					p->SetGenerating(false);
				};
			});

			autorenew_acme_ctrl_->SetValue(true);
			autorenew_acme_ctrl_->Disable();

			wxTransferDataFromWindow(p, [&] {
				auto omni = cert_info_->omni();
				wxASSERT(omni != nullptr);

				if (omni) {
					auto acme = omni->acme();
					wxASSERT(acme != nullptr);

					if (acme) {
						acme->autorenew = autorenew_acme_ctrl_->GetValue();
					}
				}

				return true;
			});
		};

		wxTransferDataToWindow(p, [&] {
			if (acme_opts_ && *acme_opts_) {
				wxSwitchBookTo(acme_generation_page_, acme_ctrl_);
			}
			else {
				wxSwitchBookTo(acme_setup_page_, acme_ctrl_);
			}

			return true;
		});
	};

	wxPage(details_book_) |[&](auto p) {
		wxVBox(p, 0) = acme_details_ctrl_ = wxCreate<cert_details>(p);

		acme_generation_->SetDetails(acme_details_ctrl_);
	};

	book_->GetParent()->Layout();

	return true;
}

void CertInfoEditor::SetValue(fz::securable_socket::cert_info *cert_info, fz::securable_socket::cert_info::extra *extra, fz::util::fs::path_format server_path_format)
{
	fz::securable_socket::omni_cert_info *omni{};
	fz::securable_socket::omni_cert_info::extra *omni_extra{};

	server_path_format_ = server_path_format;

	if (cert_info) {
		if (auto o = cert_info->omni()) {
			cert_info_ = cert_info;
			omni = o;
		}

		extra_info_ = {};

		if (extra) {
			if (auto o = extra->omni()) {
				extra_info_ = extra;
				omni_extra = o;
			}
		}
	}

	if (provided_generation_->SetValue(omni, omni_extra, server_path_format)) {
		wxSwitchBookTo(provided_generation_, this);
	}

	if (autogenerated_generation_->SetValue(omni, omni_extra, server_path_format)) {
		wxSwitchBookTo(autogenerated_generation_, this);
	}

	if (acme_generation_) {
		if (acme_generation_->SetValue(omni, omni_extra, server_path_format)) {
			wxSwitchBookTo(acme_generation_, this);

			autorenew_acme_ctrl_->SetValue(omni->acme()->autorenew);
			autorenew_acme_ctrl_->Enable();
		}
	}

	if (!cert_info_) {
		Disable();
		book_->SetSelection(0);
		return;
	}

	Enable();
}

void CertInfoEditor::SetGenerateSelfsignedCertificateFunction(GenerateSelfsignedFunc func)
{
	selfsigned_func_ = std::move(func);
}

void CertInfoEditor::SetGenerateAcmeCertificateFunction(GenerateAcmeFunc func)
{
	acme_func_ = std::move(func);
}

void CertInfoEditor::SetTestCertificateFunction(TestCertificateFunc func)
{
	provided_generation_->SetTestCertificateFunction(func);

	autogenerated_generation_->SetTestCertificateFunction(func);

	if (acme_generation_) {
		acme_generation_->SetTestCertificateFunction(std::move(func));
	}
}

void CertInfoEditor::SetRetrieveDeobfuscatedBlobFunction(RetrieveDeobfuscatedBlobFunc func)
{
	provided_generation_->SetRetrieveDeobfuscatedBlobFunction(func);
	autogenerated_generation_->SetRetrieveDeobfuscatedBlobFunction(func);

	if (acme_generation_) {
		acme_generation_->SetRetrieveDeobfuscatedBlobFunction(std::move(func));
	}
}

void CertInfoEditor::SetAcmeOptions(const server_settings::acme_options &acme_opts)
{
	acme_opts_ = &acme_opts;
	if (create_acme_editor()) {
		SetValue(cert_info_, extra_info_, server_path_format_);
	}
}

void CertInfoEditor::SetSwitchToAcmeOptsFunc(CertInfoEditor::SwitchToAcmeOptsFunc func)
{
	switch_to_acme_opts_ = std::move(func);
}

// BUG: Clang requires these: clang's bug or something I don't understand about C++?
template struct wxValidateOnlyIfCurrentPage<CertInfoEditor::cert_form>;
template struct wxCreator::CtrlFix<wxValidateOnlyIfCurrentPage<CertInfoEditor::cert_form>>;
