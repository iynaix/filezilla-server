#include <wx/evtloop.h>

#include "serveradministrator.hpp"

#include "helpers.hpp"

void ServerAdministrator::Dispatcher::operator()(administration::get_groups_and_users::response &&v)
{
	invoke_on_admin([this, v = std::move(v)] {
		if (!v) {
			wxMsg::Error(_S("Error retrieving groups and users info"));
			return;
		}

		if (!was_expecting_response())
			return;

		std::tie(server_admin_.groups_, server_admin_.users_) = std::move(*v).tuple();

		server_admin_.maybe_act_on_settings_received();
	});
}

void ServerAdministrator::Dispatcher::operator()(administration::create_ftp_test_environment::response &&v)
{
	invoke_on_admin([this, v = std::move(v)]() mutable {
		if (server_admin_.rmp_loop_timer_) {
			server_admin_.rmp_loop_timer_->Stop();
		}

		if (!server_admin_.rmp_loop_) {
			return;
		}

		if (v) {
			auto &&[name, pass] = std::move(v->tuple());
			server_admin_.rmp_temp_user_and_pass_ = { std::move(name), std::move(pass) };
		}
		else {
			server_admin_.rmp_temp_user_and_pass_ = {};
		}

		server_admin_.rmp_loop_->Exit();
	});
}

fz::expected<std::pair<std::string, std::string>, wxString> ServerAdministrator::create_ftp_test_environment(const fz::ftp::server::options &opts, fz::duration timeout)
{
	wxCHECK_MSG(rmp_loop_ == nullptr, fz::unexpected(wxT("Internal error: rmp_loop_ is not NULL")), wxT("Internal error: rmp_loop_ is not NULL"));

	if (!client_.is_connected()) {
		return fz::unexpected(_S("Got disconnected from the server, try again later."));
	}

	wxGUIEventLoop loop;
	wxTimer timer;
	wxWindowDisabler disabler;

	timer.Bind(wxEVT_TIMER, [&loop, this](wxTimerEvent &) {
		rmp_loop_error_ = _S("Timed out waiting for response from server.");
		loop.Exit();
	});

	rmp_loop_ = &loop;
	rmp_loop_timer_ = &timer;

	rmp_loop_error_.clear();

	client_.send<administration::create_ftp_test_environment>(opts, timeout);
	timer.Start(10000, true);

	loop.Run();

	rmp_loop_  = nullptr;
	rmp_loop_timer_ = nullptr;

	if (!rmp_loop_error_.empty() || rmp_temp_user_and_pass_.first.empty()) {
		return fz::unexpected(std::move(rmp_loop_error_));
	}

	return rmp_temp_user_and_pass_;
}

FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, ServerAdministrator::Dispatcher, administration::get_groups_and_users::response);
FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, ServerAdministrator::Dispatcher, administration::create_ftp_test_environment::response);
