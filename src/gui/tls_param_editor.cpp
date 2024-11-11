#include "tls_param_editor.hpp"

#include <wx/choicebk.h>
#include <wx/simplebook.h>
#include <wx/textctrl.h>
#include <wx/statline.h>

#include "locale.hpp"
#include "helpers.hpp"

#include "../filezilla/securable_socket.hpp"

struct TlsParamEditor::Useful {
	wxWindow *page{};
	wxTextCtrl *text{};

	operator bool() const {
		return page != nullptr && text != nullptr;
	}
};

TlsParamEditor::Useful TlsParamEditor::first_useful()
{
	Useful ret{};

	if (!(style_ & TLS_PE_READONLY) || selected_) {
		auto selected = selected_
			? selected_
			: TLS_PE_BLOB | TLS_PE_FILEPATH | TLS_PE_PKCS11URL;

		if (style_ & selected) {
			ret.page = blob_page_;
			ret.text = blob_ctrl_;
		}
		else
		if (style_ & selected) {
			ret.page = filepath_page_;
			ret.text = filepath_ctrl_;
		}
		else
		if (style_ & selected) {
			ret.page = pkcs11url_page_;
			ret.text = pkcs11url_ctrl_;
		}
	}

	if (!ret) {
		ret.page = unsupported_page_;
		ret.text = unsupported_ctrl_;
	}

	return ret;
}

wxChoice *TlsParamEditor::GetChoiceCtrl()
{
	return main_book_->GetChoiceCtrl();
}

void TlsParamEditor::SetRetrieveDeobfuscatedBlobFunction(RetrieveDeobfuscatedBlobFunc func)
{
	retrieve_deobfuscated_func_ = std::move(func);
}

bool TlsParamEditor::IsModified()
{
	return blob_modified_ || pkcs11url_modified_ || filepath_modified_;
}

bool TlsParamEditor::IsCurrentlyObfuscatedAndNotModified()
{
	if (!blob_modified_) {
		return fz::blob_obfuscator::is_obfuscated(blob_data_);
	}

	return {};
}

bool TlsParamEditor::SetObfuscated(const fz::tls_param &p)
{
	if (auto their_blob = p.blob(); their_blob && !blob_modified_) {
		auto our_id = fz::blob_obfuscator::get_obfuscated_blob_id(blob_data_);
		auto their_id = fz::blob_obfuscator::get_obfuscated_blob_id(their_blob->value);

		if (our_id == their_id) {
			blob_data_ = their_blob->value;

			if ((style_ & TLS_PE_READONLY) && param_) {
				// We need to update also the outer param, not just the internal representation,
				// since it won't be done by TransferDataFromWindow();
				*param_ = p;
			}

			return true;
		}
	}

	return {};
}

void TlsParamEditor::SetModified(bool modified)
{
	blob_modified_ = pkcs11url_modified_ = filepath_modified_ = modified;
}

TlsParamEditor::TlsParamEditor(wxWindow *parent, unsigned style, const wxString &name, unsigned selected)
	: wxPanel(parent)
	, style_(style)
	, selected_(selected)
{
	SetName(name);

	wxVBox(this, 0) = outer_book_ = wxCreate<wxNavigationEnabled<wxSimplebook>>(this) | [&](auto p) {
		main_page_ = wxPage(p, wxT("Main:")) | [&](auto p) {
			wxVBox(p, 0) = create_main_editors(p);
		};

		unsupported_page_ = wxPage(p, wxT("Unsupported:")) | [&](auto p) {
			wxHBox(p, 0) = {
				wxSizerFlags(1) >>= unsupported_ctrl_ = new wxTextCtrl(p, nullID, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY)
			};
		};
	};

	SetValue(nullptr, {});
}

wxSizer *TlsParamEditor::create_main_editors(wxWindow *p)
{
	auto read_only = style_ & TLS_PE_READONLY ? wxTE_READONLY : 0;

	auto box = wxVBox(p, 0) = main_book_ = wxCreate<wxChoicebook>(p, nullID, wxDefaultPosition, wxDefaultSize, wxCHB_LEFT) | [&](auto p) {
		if (style_ & TLS_PE_BLOB) {
			blob_page_ = wxPage(wxValidateOnlyIfCurrent)(p, _S("Raw data:"), selected_ == TLS_PE_BLOB) | [&](auto p) {
				wxHBox(p, 0) = {
					wxSizerFlags(1) >>= blob_ctrl_ = new wxTextCtrl(p, nullID, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY),
					edit_blob_ctrl_ = new wxButton(p, nullID, read_only ? _S("&View") : _S("&Edit"))
				};

				blob_ctrl_->SetFont(blob_ctrl_->GetFont().MakeItalic());

				wxTransferDataToWindow(p, [&] {
					wxString text;

					if (!blob_data_.empty()) {
						edit_blob_ctrl_->Enable();

						text = _F(
							"%s hidden. Click on '%s' to %s.",
							GetName(),
							edit_blob_ctrl_->GetLabelText(),
							style_ & TLS_PE_READONLY ? _S("display") : _S("modify")
						);
					}
					else {
						text = _F("%s is absent.", GetName());

						if ((style_ & TLS_PE_READONLY)) {
							edit_blob_ctrl_->Disable();
						}
						else {
							text += _F(" Click on '%s' to modify.", edit_blob_ctrl_->GetLabelText());
						}
					}

					blob_ctrl_->ChangeValue(text);

					return true;
				});
			};

			edit_blob_ctrl_->Bind(wxEVT_BUTTON, [&](auto) {
				edit_blob();
			});

			if (!read_only) {
				blob_ctrl_->Bind(wxEVT_TEXT, [&](auto &) {
					check_modified();
				});
			}
		}

		if (style_ & TLS_PE_PKCS11URL) {
			pkcs11url_page_ = wxPage(p, _S("PKCS#11 URL:"), selected_ == TLS_PE_PKCS11URL) | [&](auto p) {
				wxHBox(p, 0) = {
					wxSizerFlags(1) >>= pkcs11url_ctrl_ = new wxTextCtrl(p, nullID, wxEmptyString, wxDefaultPosition, wxDefaultSize, read_only)
				};

				if (!read_only) {
					pkcs11url_ctrl_->SetHint(_S("Input a pkcs11: URL."));

					pkcs11url_ctrl_->Bind(wxEVT_TEXT, [&](auto &) {
						check_modified();
					});
				}
			};
		}

		if (style_ & TLS_PE_FILEPATH) {
			filepath_page_ = wxPage(p, _S("Path to file:"), selected_ == TLS_PE_FILEPATH) | [&](auto p) {
				wxHBox(p, 0) = {
					wxSizerFlags(1) >>= filepath_ctrl_ = new wxTextCtrl(p, nullID, wxEmptyString, wxDefaultPosition, wxDefaultSize, read_only)
				};

				if (!read_only) {
					filepath_ctrl_->SetHint(_S("Input the path to a file on the server's filesystem."));

					filepath_ctrl_->Bind(wxEVT_TEXT, [&](auto &) {
						check_modified();
					});
				}
			};
		}

		if (read_only) {
			p->GetChoiceCtrl()->Disable();
		}
	};

	if (!read_only) {
		main_book_->Bind(wxEVT_CHOICEBOOK_PAGE_CHANGED, [this](auto &ev) {
			ev.Skip();
			check_modified();
		});
	}

	return std::move(box);
}

void TlsParamEditor::check_modified()
{
	if (!param_ || (style_ & TLS_PE_READONLY)) {
		return;
	}

	if (outer_book_->GetCurrentPage() == main_page_) {
		if (auto p = main_book_->GetCurrentPage()) {
			if (p == blob_page_) {
				blob_modified_ = !param_->blob() || param_->blob()->value != blob_data_;
			}
			else
			if (p == pkcs11url_page_) {
				pkcs11url_modified_ = !param_->pkcs11url() || param_->pkcs11url()->value != fz::to_utf8(pkcs11url_ctrl_->GetValue());
			}
			else
			if (p == filepath_page_) {
				filepath_modified_ = !param_->filepath() || param_->filepath()->value != fz::to_native(filepath_ctrl_->GetValue());
			}
		}
	}

	Event::Modified.Process(this, this, IsModified());
}

void TlsParamEditor::edit_blob()
{
	auto title = _F("%s the %s", edit_blob_ctrl_->GetLabelText(), GetName().MakeLower());

	edit_blob_ctrl_->Disable();

	wxPushDialog(this, nullID, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) | [this](wxDialog *p) {
		wxTextCtrl *data{}, *nodata{};
		wxWindow *data_page{}, *nodata_page{};
		wxButton *clear{};
		std::string blob_data;

		wxSize extent = wxMonospaceTextExtent(66, 12, p, {wxSYS_VSCROLL_X});

		auto act_on_data_provided = [&](std::string_view in) {
			blob_data = std::string(in);

			if (in.empty() || fz::is_pem(in)) {
				data->ChangeValue(fz::to_wxString(in));
				wxSwitchBookTo(data_page, p);
			}
			else {
				wxSwitchBookTo(nodata_page, p);
				nodata->ChangeValue(_F("The %s is in a format that cannot be displayed.", GetName().MakeLower()));
			}
		};

		{
			int read_only = style_ & TLS_PE_READONLY
				? wxTE_READONLY
				: 0;

			auto sizer = wxVBox(p) = {
				wxSizerFlags(1).Expand() >>= wxCreate<wxNavigationEnabled<wxSimplebook>>(p) | [&](auto p) {
					nodata_page = wxPage(wxValidateOnlyIfCurrent)(p, wxT("**unable to view/edit**")) | [&](auto p) {
						wxVBox(p, 0) = {
							wxLabel(p, _F("%s:", GetName())),
							wxSizerFlags(1).Expand() >>= nodata = new wxTextCtrl(p, nullID, wxEmptyString, wxDefaultPosition, extent, wxTE_MULTILINE | wxTE_READONLY),
						};

						wxTransferDataFromWindow(p, [&] {
							if (blob_data != blob_data_) {
								wxCommandEvent event(wxEVT_TEXT, nullID);
								blob_ctrl_->GetEventHandler()->ProcessEvent(event);
								blob_data_ = std::move(blob_data);
							}
							return true;
						});
					};

					data_page = wxPage(wxValidateOnlyIfCurrent)(p, wxT("**ok to view/edit**")) | [&](auto p) {
						wxVBox(p, 0) = {
							wxLabel(p, _F("%s in &PEM format:", GetName())),
							wxSizerFlags(1).Expand() >>= data = new wxTextCtrl(p, nullID, wxEmptyString, wxDefaultPosition, extent, wxTE_MULTILINE | read_only),
						};

						wxTransferDataFromWindow(p, [&] {
							if (data->GetValue().empty()) {
								wxMsg::Error(_S("A %s must be provided."), GetName().MakeLower());
								return false;
							}

							auto text_data = fz::to_utf8(data->GetValue());

							if (blob_data != text_data) {
								blob_data_ = std::move(text_data);
								wxCommandEvent event(wxEVT_TEXT, nullID);
								blob_ctrl_->GetEventHandler()->ProcessEvent(event);
							}

							return true;
						});
					};
				},
			};

			auto save_file = wxSaveFile(p, [&] {
				if (wxIsSelected(nodata_page)) {
					return blob_data;
				}
				else {
					return fz::to_utf8(data->GetValue());
				}
			}, _F("Sa&ve %s to file...", GetName().MakeLower()), _F("Save %s to file", GetName().MakeLower()), wxEmptyString, wxEmptyString);


			if (!read_only) {
				sizer += {
					clear = new wxButton(p, nullID, _S("&Clear")),

					wxHBox(p, 0) = {
						wxSizerFlags(1) >>= save_file,
						wxSizerFlags(1) >>= wxLoadFile(p, act_on_data_provided, _F("&Load %s from file...", GetName().MakeLower()), _F("Load %s from file", GetName().MakeLower()), wxEmptyString, wxEmptyString),
					}
				};
			}
			else {
				sizer += {
					save_file
				};
			}

			sizer += {
				new wxStaticLine(p),
				p->CreateButtonSizer(wxOK | (read_only ? 0 : wxCANCEL))
			};
		}

		auto font = p->GetFont();
		font.SetFamily(wxFONTFAMILY_TELETYPE);

		data->SetFont(font);
		nodata->SetFont(font);

		if (clear) {
			clear->Bind(wxEVT_BUTTON, [&](auto) {
				data->Clear();
				wxSwitchBookTo(data_page, p);
			});
		}

		wxTransferDataToWindow(p, [&] {
			if (fz::blob_obfuscator::is_obfuscated(blob_data_) && retrieve_deobfuscated_func_) {
				wxSwitchBookTo(nodata_page);
				nodata->ChangeValue(_F("Retrieving %s from server...", GetName().MakeLower()));
				auto deobfuscated = retrieve_deobfuscated_func_(blob_data_);

				if (!deobfuscated) {
					wxMsg::Error(_F("Couldn't retrieve %s from server.", GetName().MakeLower())).Ext(deobfuscated.error()).Wait();
					p->CallAfter([&]{p->EndModal(wxID_CANCEL);});
					return false;
				}
				act_on_data_provided(*deobfuscated);
			}
			else {
				act_on_data_provided(blob_data_);
			}

			return true;
		});

		p->ShowModal();

		edit_blob_ctrl_->Enable();
		blob_page_->TransferDataToWindow();
	};
}

void TlsParamEditor::SetValue(fz::tls_param *param, fz::util::fs::path_format server_path_format, bool check_modified)
{
	param_ = param;
	server_path_format_ = server_path_format;

	if (!param_) {
		auto useful = first_useful();
		useful.text->Clear();
		if (auto b = wxSwitchBookTo(useful.page, this)) {
			b->Disable();
		}
		return;
	}

	outer_book_->Enable();
	main_book_->Enable();

	bool supported = false;

	if (auto p = param_->blob()) {
		supported = blob_page_ != nullptr;

		if (supported) {
			blob_modified_ = check_modified && blob_data_ != p->value;
			blob_data_ = p->value;

			wxSwitchBookTo(blob_page_, this);
		}
	}
	else
	if (auto p = param_->filepath()) {
		supported = filepath_page_ != nullptr;

		if (supported) {
			filepath_modified_ = check_modified && filepath_ctrl_->GetValue() != fz::to_wxString(p->value);
			filepath_ctrl_->ChangeValue(fz::to_wxString(p->value));

			wxSwitchBookTo(filepath_page_, this);
		}
	}
	else
	if (auto p = param_->pkcs11url()) {
		supported = pkcs11url_page_ != nullptr;

		if (supported) {
			static const auto get_value = [](auto *p) {
				if (p->is_valid()) {
					return fz::to_wxString(p->value);
				}
				else {
					return wxString();
				}
			};

			auto p_value = get_value(p);

			pkcs11url_modified_ =  check_modified && pkcs11url_ctrl_->GetValue() != p_value;
			pkcs11url_ctrl_->ChangeValue(p_value);

			wxSwitchBookTo(pkcs11url_page_, this);
		}
	}

	if (!supported) {
		SetModified(false);

		if (wxSwitchBookTo(unsupported_page_, this)) {
			unsupported_ctrl_->SetValue(fz::to_wxString(param_->url()));
		}
		else {
			SetValue(nullptr, {});
		}
	}
}

static bool is_empty(wxTextCtrl *t) {
	return t->GetValue().IsEmpty();
}

static bool is_empty(const std::string &s) {
	return s.empty();
}

bool TlsParamEditor::TransferDataFromWindow()
{
	if (!wxPanel::TransferDataFromWindow()) {
		return false;
	}

	if (style_ & TLS_PE_READONLY) {
		return true;
	}

	if (!wxIsSelected(main_page_)) {
		return true;
	}

	if (!IsEnabled()) {
		return true;
	}

	auto is_empty = [this](auto t) {
		if (::is_empty(t)) {
			wxMsg::Error(_F("%s cannot be empty.", GetName())).Wait();
			return true;
		}

		return false;
	};

	if (wxIsSelected(blob_page_)) {
		if (is_empty(blob_data_)) {
			return false;
		}

		auto blob = fz::tls_blob(blob_data_);

		if (!blob) {
			wxMsg::Error(_F("%s is not valid.", GetName())).Wait();
			return false;
		}

		*param_ = blob;

		return true;
	}

	if (wxIsSelected(pkcs11url_page_)) {
		if (is_empty(pkcs11url_ctrl_)) {
			return false;
		}

		auto url = fz::tls_pkcs11url(fz::to_utf8(pkcs11url_ctrl_->GetValue()));

		if (!url) {
			wxMsg::Error(_F("%s is not a valid PKCS#11 URL.", GetName())).Wait();
			return false;
		}

		*param_ = std::move(url);

		return true;
	}

	if (wxIsSelected(filepath_page_)) {
		auto path = fz::to_native(filepath_ctrl_->GetValue());

		if (auto res = fz::tvfs::validation::validate_native_path(path, server_path_format_); !res) {
			InvalidPathExplanation exp(res, server_path_format_, false, _F("path to the %s", GetName().MakeLower()));
			wxMsg::Error(exp.main).Ext(exp.extra).Wait();
			return false;
		}

		*param_ = fz::tls_filepath(std::move(path));

		return true;
	}

	return true;
}
