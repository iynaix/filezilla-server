#include <libfilezilla/glue/wx.hpp>

#include <wx/splitter.h>
#include <wx/msgdlg.h>
#include <wx/thread.h>
#include <wx/sizer.h>
#include <wx/log.h>
#include <wx/statline.h>
#include <wx/app.h>
#include <wx/hyperlink.h>
#include <wx/statbmp.h>
#include <wx/choicebk.h>
#include <wx/filepicker.h>
#include <wx/evtloop.h>
#include <wx/checkbox.h>

#include "../serveradministrator.hpp"
#include "../certinfoeditor.hpp"
#include "../settingsdialog.hpp"
#include "../networkconfigwizard.hpp"
#include "../textvalidatorex.hpp"

#include "../locale.hpp"
#include "../glue.hpp"
#include "../helpers.hpp"

static wxString get_acme_error(const fz::native_string &s)
{
	if (auto json = fz::json::parse(fz::to_utf8(s)))
		return wxString::Format(wxT("Type: %s\nDetail: %s"), fz::to_wxString(json["type"]), fz::to_wxString(json["detail"]));

	return fz::to_wxString(s);
}

void ServerAdministrator::Dispatcher::operator()(administration::get_acme_options::response && v)
{
	invoke_on_admin([this, v = std::move(v)] {
		if (!v) {
			wxMsg::Error(_S("Error retrieving ACME options"));
			return;
		}

		if (!was_expecting_response())
			return;

		std::tie(server_admin_.acme_options_, server_admin_.acme_extra_account_info_) = std::move(*v).tuple();

		server_admin_.maybe_act_on_settings_received();
	});
}


void ServerAdministrator::Dispatcher::operator()(administration::generate_acme_certificate::response && v)
{
	invoke_on_admin([this, v = std::move(v)] {
		if (server_admin_.rmp_loop_timer_) {
			server_admin_.rmp_loop_timer_->Stop();
		}

		if (!server_admin_.rmp_loop_) {
			return;
		}

		// The user might have closed the settings dialog before the response arrived.
		if (server_admin_.settings_dialog_ == nullptr) {
			return;
		}

		if (v) {
			auto [info, extra] = v->tuple();
			if (!info || !extra || !info.omni() || !extra.omni() || !server_admin_.out_cert_info_ || !server_admin_.out_cert_info_extra_) {
				server_admin_.rmp_loop_error_ = _S("Internal inconsistency error.");
			}
			else {
				server_admin_.rmp_loop_error_.clear();
				*server_admin_.out_cert_info_ = *info.omni();
				*server_admin_.out_cert_info_extra_ = *extra.omni();
			}
		}
		else {
			server_admin_.rmp_loop_error_ = fz::to_wxString(std::get<0>(v.error().v_));
		}

		server_admin_.rmp_loop_->Exit();
	});
}

void ServerAdministrator::Dispatcher::operator()(administration::generate_acme_account::response && v)
{
	invoke_on_admin([this, v = std::move(v)] {
		// The user might have closed the settings dialog before the response arrived.
		if (server_admin_.settings_dialog_ == nullptr)
			return;

		if (v) {
			server_admin_.acme_options_ = server_admin_.settings_dialog_->GetAcmeOptions();
			std::tie(server_admin_.acme_options_.account_id, server_admin_.acme_extra_account_info_) = v->tuple();
			server_admin_.logger_.log_u(fz::logmsg::status, _S("Generated ACME account: %s"), server_admin_.acme_options_.account_id);
		}
		else
			wxMsg::Error(_S("Couldn't generate a new ACME account, please try again."))
				.Ext(get_acme_error(std::get<0>(v.error().v_)));

		server_admin_.settings_dialog_->SetAcmeAccountId(server_admin_.acme_options_.account_id, server_admin_.acme_extra_account_info_);
	});
}

void ServerAdministrator::Dispatcher::operator()(administration::get_acme_terms_of_service::response && v, administration::engine::session &)
{
	invoke_on_admin([this, v = std::move(v)] {
		// The user might have closed the settings dialog before the response arrived.
		if (server_admin_.settings_dialog_ == nullptr)
			return;

		if (!v) {
			wxMsg::Error(_S("Couldn't generate a new ACME account, please try again."))
				.Ext(get_acme_error(std::get<0>(v.error().v_)));

			server_admin_.settings_dialog_->SetAcmeOptions(server_admin_.acme_options_, server_admin_.acme_extra_account_info_);
		}
		else {
			auto accept_terms = [this](bool terms_accepted) {
				server_admin_.client_.send<administration::generate_acme_account>(
					server_admin_.working_acme_extra_account_info_.directory,
					server_admin_.working_acme_extra_account_info_.contacts,
					terms_accepted
				);
			};

			auto [terms] = std::move(*v);

			if (terms.empty())
				return accept_terms(true);

			wxPushDialog(server_admin_.settings_dialog_, wxID_ANY, _S("Terms of Service"), wxDefaultPosition, wxDefaultSize, wxCAPTION) | [terms = fz::to_wxString(terms), accept_terms] (wxDialog *p) {
				wxVBox(p) = {
					{0, wxLabel(p, _S("Do you accept the terms of service?")) },
					{0, new wxHyperlinkCtrl(p, wxID_ANY, terms, terms) },
					{1, p->CreateButtonSizer(wxYES_NO) }
				};

				p->SetEscapeId(wxID_NO);
				p->SetAffirmativeId(wxID_YES);

				return accept_terms(p->ShowModal() == wxID_YES);
			};
		}
	});
}

void ServerAdministrator::generate_acme_account()
{
	if (!settings_dialog_)
		return;

	working_acme_extra_account_info_ = acme_extra_account_info_;

	wxPushDialog(settings_dialog_, wxID_ANY, _S("ACME account generation"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxRESIZE_BORDER) |[this] (wxDialog *p) {
		static const std::string production = "https://acme-v02.api.letsencrypt.org/directory";
		static const std::string staging = "https://acme-staging-v02.api.letsencrypt.org/directory";

		wxVBox(p) = {
			{0, wxLabel(p, _S("Contact URIs, separated by blanks: (i.e. mailto:fred@example.com mailto:bob@example.com)")) },
			{1, new wxTextCtrl(p, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_RICH2) |[&](auto p) {
				p->SetValidator(TextValidatorEx(wxFILTER_NONE, &working_acme_extra_account_info_.contacts));
			}},

			{0, wxLabel(p, _S("ACME directory:")) },
			{1, new wxChoicebook(p, wxID_ANY) |[&](wxChoicebook *p) {
				wxPage(wxValidateOnlyIfCurrent)(p, _S("Let's Encrypt® production")) |[&](auto p) {
					wxTransferDataFromWindow(p, [&] {
						working_acme_extra_account_info_.directory = production;
						return true;
					});
				};

				wxPage(wxValidateOnlyIfCurrent)(p, _S("Let's Encrypt® staging (for testing only)")) |[&](auto p) {
					wxTransferDataFromWindow(p, [&] {
						working_acme_extra_account_info_.directory = staging;
						return true;
					});
				};

				wxPage(wxValidateOnlyIfCurrent)(p, _S("Custom")) |[&](auto p) {
					wxVBox(p, 0) = new wxTextCtrl(p, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_RICH2) |[&](auto p) {
						p->SetValidator(TextValidatorEx(wxFILTER_NONE, &working_acme_extra_account_info_.directory, absolute_uri_validation({"https"})));
					};
				};

				wxTransferDataToWindow(p, [this, p]{
					if (working_acme_extra_account_info_.directory.empty() || working_acme_extra_account_info_.directory == production)
						p->ChangeSelection(0);
					else
					if (working_acme_extra_account_info_.directory == staging)
						p->ChangeSelection(1);
					else
						p->ChangeSelection(2);

					return true;
				});
			}},

			{0, p->CreateButtonSizer(wxCANCEL | wxOK)}
		};

		p->SetEscapeId(wxID_CANCEL);
		p->SetAffirmativeId(wxID_OK);

		if (p->ShowModal() == wxID_OK) {
			logger_.log_u(fz::logmsg::status, _S("Generating new ACME account"));

			client_.send<administration::get_acme_terms_of_service>(working_acme_extra_account_info_.directory);
		}
		else {
			settings_dialog_->SetAcmeAccountId(acme_options_.account_id, acme_extra_account_info_);
		}
	};
}

FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, ServerAdministrator::Dispatcher, administration::get_acme_options::response);
FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, ServerAdministrator::Dispatcher, administration::generate_acme_certificate::response);
FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, ServerAdministrator::Dispatcher, administration::generate_acme_account::response);
FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, ServerAdministrator::Dispatcher, administration::get_acme_terms_of_service::response);

