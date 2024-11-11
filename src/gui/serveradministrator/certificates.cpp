#include <wx/evtloop.h>

#include "serveradministrator.hpp"

#include "locale.hpp"

void ServerAdministrator::Dispatcher::operator()(administration::generate_selfsigned_certificate::response && v)
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

void ServerAdministrator::Dispatcher::operator()(administration::get_extra_certs_info::response &&v)
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
			auto [extra] = v->tuple();
			if (!extra.omni() || !server_admin_.out_cert_info_extra_) {
				server_admin_.rmp_loop_error_ = _S("Internal inconsistency error.");
			}
			else {
				server_admin_.rmp_loop_error_.clear();
				*server_admin_.out_cert_info_extra_ = *extra.omni();
			}
		}
		else {
			server_admin_.rmp_loop_error_ = fz::to_wxString(std::get<0>(v.error().v_));
		}

		server_admin_.rmp_loop_->Exit();
	});
}

FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, ServerAdministrator::Dispatcher, administration::generate_selfsigned_certificate::response);
FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, ServerAdministrator::Dispatcher, administration::get_extra_certs_info::response);
