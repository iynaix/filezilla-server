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

#include "../filezilla/logger/type.hpp"

#include "serveradministrator.hpp"
#include "settingsdialog.hpp"
#include "serverlogger.hpp"
#include "sessionlist.hpp"
#include "settings.hpp"

#include "locale.hpp"
#include "glue.hpp"
#include "helpers.hpp"

class ServerAdministrator::Splitter: public wxSplitterWindow
{
public:
	Splitter(wxWindow *parent)
		: wxSplitterWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D | wxSP_LIVE_UPDATE)
		, wanted_position_(Settings()->sashes().try_emplace(L"ServerAdministrator", INT_MIN).first->second)
	{
		SetSashGravity(0.8);
		SetMinimumPaneSize(100);

		// We want to avoid to resize during the time wx does its own internal things and before anything is displayed on screen,
		// so to avoid any spurious drift of the sash. Hence, we allow resizing only if the window is already displayed.
		// This is true if IsShownOnScreen() returns true, or if a PAINT event is received.
		Bind(wxEVT_SIZE, [this](wxSizeEvent &ev) {
			if (!can_resize_)
				can_resize_ = IsShownOnScreen();

			if (can_resize_)
				OnSize(ev);
			else
				ev.Skip();
		});

		Bind(wxEVT_PAINT, [this](wxPaintEvent &ev) {
			ev.Skip();

			if (!can_resize_) {
				can_resize_ = true;
				SendSizeEvent();
			}
		});
	}

private:
	int &wanted_position_;
	bool can_resize_{};

	int OnSashPositionChanging(int position) override
	{
		position = wxSplitterWindow::OnSashPositionChanging(position);

		if (position != -1) {
			wanted_position_ = position;
		}

		return position;
	}

	void OnDoubleClickSash(int, int) override {}

	void OnSize(wxSizeEvent &event)
	{
		// Code copied from wxWidgets and adjusted for better gravity handling

		// only process this message if we're not iconized - otherwise iconizing
		// and restoring a window containing the splitter has a funny side effect
		// of changing the splitter position!
		bool iconized;

		if (auto winTop = dynamic_cast<wxTopLevelWindow*>(wxGetTopLevelParent(this))) {
			iconized = winTop->IsIconized();
		}
		else {
			wxFAIL_MSG(wxT("should have a top level parent!"));

			iconized = false;
		}

		if (iconized) {
			m_lastSize = wxSize(0, 0);

			event.Skip();

			return;
		}

		if (m_windowTwo) {
			int w, h;
			GetClientSize(&w, &h);

			int new_size = m_splitMode == wxSPLIT_VERTICAL ? w : h;
			const int old_size = m_splitMode == wxSPLIT_VERTICAL ? m_lastSize.x : m_lastSize.y;
			int size_to_distribute = new_size - old_size;

			if (wanted_position_ == INT_MIN)
				wanted_position_ = m_sashPosition;

			wanted_position_ += (int)std::round((double)size_to_distribute * m_sashGravity);

			auto new_position = AdjustSashPosition(wanted_position_);

			if (new_position != m_sashPosition)
				SetSashPositionAndNotify(new_position);

			m_lastSize = wxSize(w,h);
		}

		SizeWindows();
	}
};

ServerAdministrator::~ServerAdministrator()
{
	wxSetWindowLogger(wxGetTopLevelParent(this), nullptr);

	is_being_deleted_ = true;

	if (server_logger_)
		logger_.remove_logger(*server_logger_);
}


ServerAdministrator::ServerAdministrator(wxWindow *parent, fz::logger_interface &logger)
	: wxPanel(parent)
	, logger_(logger, "Admin UI")
	, trust_store_(pool_)
	, dispatcher_(*this)
	, client_(pool_, loop_, dispatcher_, logger_)
{
	SetName(wxT("ServerAdministrator"));

	logger_.set_all(fz::logmsg::type(~0));
	wxSetWindowLogger(wxGetTopLevelParent(this), &logger_);

	wxVBox(this, 0) = new Splitter(this) |[&](auto p) {
		server_logger_ = wxCreate<ServerLogger>(p);
		session_list_ = wxCreate<SessionList>(p);

		p->SplitHorizontally(server_logger_, session_list_, 0);
	};

	logger_.add_logger(*server_logger_);

	server_logger_->Bind(ServerLogger::Event::IPsNeedToBeBanned, [&] (ServerLogger::Event &ev){
		for (auto &s: ev.sessions) {
			client_.send<administration::ban_ip>(fz::to_string(s.host), fz::address_type(s.address_family));
		}
	});

	session_list_->Bind(SessionList::Event::SessionNeedsToBeKilled, [&] (SessionList::Event &ev){
		client_.send<administration::end_sessions>(std::vector{ev.session_info.id});
	});

	session_list_->Bind(SessionList::Event::IpNeedsToBeBanned, [&] (SessionList::Event &ev){
		client_.send<administration::ban_ip>(fz::to_string(ev.session_info.host), fz::address_type(ev.session_info.address_family));
	});

	session_list_->Bind(SessionList::Event::SessionNotAvailable, [&] (SessionList::Event &ev){
		client_.send<administration::session::solicit_info>(std::vector<uint64_t>{ev.session_info.id});
	});

	reconnect_timer_.Bind(wxEVT_TIMER, [&](auto &) {
		if (state_ != state::disconnecting)
			do_connect();
	});

	server_status_changed_timer_.Bind(wxEVT_TIMER, [&](auto &) {
		maybe_send_server_status_changed(true);
	});
}

void ServerAdministrator::ConnectTo(const server_info &info, int reconnect_timeout_secs)
{
	server_info_ = info;
	state_ = state::connecting;
	reconnect_timeout_secs_ = reconnect_timeout_secs;
	certificate_not_trusted_ = false;

	do_connect();
}

void ServerAdministrator::Disconnect()
{
	if (state_ == state::reconnecting) {
		state_ = state::disconnecting;
		do_disconnect(0);
	}
	else {
		state_ = state::disconnecting;
		client_.disconnect();
	}
}

void ServerAdministrator::do_connect()
{
#if defined(ENABLE_ADMIN_DEBUG) && ENABLE_ADMIN_DEBUG
	client_.connect({server_info_.host, server_info_.port, server_info_.use_tls});
#else
	client_.connect({server_info_.host, server_info_.port}, true);
#endif
}

void ServerAdministrator::do_disconnect(int err)
{
	CallAfter([this, err]{
		session_list_->Clear();
		public_ipv4_cache_.reset();
		public_ipv6_cache_.reset();
		public_ipvx_cache_.reset();
		responses_to_wait_for_ = 0;

		server_status_.num_of_active_sessions = -1;
		maybe_send_server_status_changed();

		if (certificate_not_trusted_)
			state_ = state::disconnecting;

		if (settings_dialog_ && state_ == state::disconnecting)
			settings_dialog_->EndModal(wxID_ABORT);

		if (certificate_not_trusted_)
			server_logger_->log_u(fz::logmsg::status, _S("Disconnected from server %s because TLS certificate is not trusted.").ToStdWstring(), server_info_.name);
		else
		if (!err)
			server_logger_->log_u(fz::logmsg::status, _S("Disconnected from server %s without any errors.").ToStdWstring(), server_info_.name);
		else
			server_logger_->log_u(fz::logmsg::error, _S("Disconnected from server %s with error %d (%s).").ToStdWstring(), server_info_.name, err, fz::socket_error_description(err));

		if (state_ != state::disconnecting && reconnect_timeout_secs_ > 0) {
			do_reconnect(err);
		}
		else {
			reconnect_timer_.Stop();
			Event::Disconnection.Process(this, this, certificate_not_trusted_ ? 0 : err, state_ != state::disconnecting);
			state_ = state::disconnected;
		}
	});
}

void ServerAdministrator::do_reconnect(int err)
{
	CallAfter([this, err]{
		if (state_ == state::disconnecting || state_ == state::disconnected)
			return;

		state_ = state::reconnecting;

		logger_.log_u(fz::logmsg::status, _S("Attempting reconnection to server %s in %d seconds...").ToStdWstring(), server_info_.name, reconnect_timeout_secs_);
		reconnect_timer_.Start(reconnect_timeout_secs_*1000, true);

		Event::Reconnection.Queue(this, this, err);
	});
}

const ServerAdministrator::server_info &ServerAdministrator::GetServerInfo() const
{
	return server_info_;
}

const ServerAdministrator::server_status &ServerAdministrator::GetServerStatus() const
{
	return server_status_;
}

void ServerAdministrator::maybe_send_server_status_changed(bool timer_stopped)
{
	fz::scoped_lock lock(mutex_);

	if (timer_stopped)
		server_status_timer_running_ = false;

	auto now = fz::monotonic_clock::now();
	if (!last_server_status_changed_time_ || (now - last_server_status_changed_time_).get_milliseconds() > 100) {
		last_server_status_changed_time_ = now;
		CallAfter([&] {
			Event::ServerStatusChanged.Process(this, this);
		});
	}
	else
	if (!server_status_timer_running_) {
		CallAfter([&] {
			server_status_timer_running_ = true;
			server_status_changed_timer_.StartOnce(100);
		});
	}
}

wxSizer *ServerAdministrator::create_session_info_sizer(wxWindow *p, const SessionList::Session::Info &, const administration::session::any_protocol_info &a)
{
	if (a.get_status() == administration::session::any_protocol_info::insecure)
		return wxVBox(p) = wxLabel(p, _S("Connection is insecure"));

	if (auto ftp = a.ftp()) {
		wxASSERT(ftp->security);

		auto &s = *ftp->security;
		auto w = ftp->security->algorithm_warnings;
		using W = decltype(w);

		auto field = [&](const auto &v, auto flag) {
			bool insecure = w & flag;
			auto text = fz::to_wxString(v);

			if (insecure) {
				text += wxT(" - ");
				text += _S("Insecure!");
			}

			auto ret = wxEscapedLabel(p, text);

			if (insecure)
				ret->SetForegroundColour(wxColour(255, 0, 0));

			return ret;
		};

		return wxGBox(p, 2, {1}) = {
			wxLabel(p, _S("TLS version:")),
			field(s.tls_version, W::tlsver),

			wxLabel(p, _S("Cipher:")),
			field(s.cipher, W::cipher),

			wxLabel(p, _S("Key exchange:")),
			field(s.key_exchange, W::kex),

			wxLabel(p, _S("MAC:")),
			field(s.mac, W::mac)
		};
	}

	return wxVBox(p) = wxLabel(p, _S("Connection is secure"));
}

void ServerAdministrator::maybe_act_on_settings_received()
{
	if (responses_to_wait_for_ > 0)
		return;

	if (on_settings_received_func_)
		CallAfter(on_settings_received_func_);
}

bool ServerAdministrator::IsConnected() const
{
	return client_.is_connected();
}

ServerAdministrator::Event::Event(Tag eventType, int err, bool server_initiated_disconnection)
	: wxEventEx(eventType)
	, error(err)
	, server_initiated_disconnection(server_initiated_disconnection)
{}

ServerAdministrator::Event::Event(wxEventEx::Tag eventType)
	: wxEventEx(eventType)
{}

/********************************************/

void ServerAdministrator::Dispatcher::operator()(administration::session::start &&v) {
	auto && [session_id, start_time, peer_ip, family] = std::move(v).tuple();

	if (server_admin_.session_list_->Add(session_id, start_time, fz::to_wxString(peer_ip), (int)family)) {
		server_admin_.server_status_.num_of_active_sessions += 1;
		server_admin_.maybe_send_server_status_changed();
	}
}

bool ServerAdministrator::Dispatcher::was_expecting_response()
{
	if (server_admin_.responses_to_wait_for_ > 0) {
		--server_admin_.responses_to_wait_for_;
		return true;
	}

	server_admin_.logger_.log(fz::logmsg::error, L"Got a response to a command that was not expected. Program will continue, but the state might be compromised.");
	return false;
}

void ServerAdministrator::Dispatcher::operator()(administration::session::stop &&v) {
	auto && [session_id, since_start] = std::move(v).tuple();

	if (server_admin_.session_list_->Remove(session_id)) {
		server_admin_.server_status_.num_of_active_sessions -= 1;
		server_admin_.maybe_send_server_status_changed();
	}
}

void ServerAdministrator::Dispatcher::operator()(administration::session::user_name &&v) {
	auto && [session_id, since_start, username] = std::move(v).tuple();

	server_admin_.session_list_->SetUserName(session_id, fz::to_wxString(username));
}

void ServerAdministrator::Dispatcher::operator()(administration::log &&v) {
	auto && [dt, session_id, type, module, msg] = std::move(v).tuple();


	uint64_t id{};
	wxString host;
	wxString username;
	int address_family{};

	if (auto &&i = server_admin_.session_list_->GetInfo(session_id)) {
		id = i->id;
		host = i->host;
		username = i->username;
		address_family = i->address_family;
	}

	server_admin_.server_logger_->Log({std::move(dt), type, fz::to_wxString(module), std::move(msg), { id, std::move(host), address_family, std::move(username) }});
}


void ServerAdministrator::Dispatcher::operator()(administration::end_sessions::response &&v)
{
	if (!v)
		server_admin_.CallAfter([] { wxMsg::Error(_S("Failed killing the session.")); });
}

void ServerAdministrator::Dispatcher::operator()(administration::listener_status &&v)
{
	auto && [dt, address_info, state] = std::move(v).tuple();

	wxString msg;
	wxString host_port = fz::to_wxString(fz::join_host_and_port(address_info.address, address_info.port));

	switch (state) {
		case fz::tcp::listener::status::started:
			msg = _F("Server started listening on %s.", host_port);
			break;

		case fz::tcp::listener::status::stopped:
			msg = _F("Server stopped listening on %s.", host_port);
			break;

		case fz::tcp::listener::status::retrying_to_start:
			msg = _F("Server couldn't listen on %s. Will keep trying.", host_port);
			break;
	}

	server_admin_.server_logger_->Log({std::move(dt), fz::logmsg::status, {}, msg, {}});
}

void ServerAdministrator::Dispatcher::operator()(administration::session::entry_close &&v) {
	auto && [session_id, since_start, entry_id, error] = std::move(v).tuple();

	server_admin_.session_list_->EntryClose(session_id, entry_id);
}

void ServerAdministrator::Dispatcher::operator()(administration::session::entry_open &&v) {
	auto && [session_id, since_start, entry_id, path, size] = std::move(v).tuple();

	server_admin_.session_list_->EntryOpen(session_id, entry_id, since_start, fz::to_wxString(path), size);
}

void ServerAdministrator::Dispatcher::operator()(administration::session::entry_written &&v)
{
	auto && [session_id, since_start, entry_id, amount, size] = std::move(v).tuple();

	server_admin_.session_list_->SetEntryWritten(session_id, entry_id, since_start, amount, size);
}

void ServerAdministrator::Dispatcher::operator()(administration::session::entry_read &&v)
{
	auto && [session_id, since_start, entry_id, amount] = std::move(v).tuple();

	server_admin_.session_list_->SetEntryRead(session_id, entry_id, since_start, amount);
}

void ServerAdministrator::Dispatcher::operator()(administration::session::protocol_info && v)
{
	auto && [session_id, since_start, any] = v.tuple();

	static constexpr auto convert_status = [](administration::session::any_protocol_info::status s) {
		using f = administration::session::any_protocol_info::status;
		using t = SessionList::SecureState;

		switch (s) {
			case f::insecure: return t::Insecure;
			case f::not_completely_secure: return t::QuasiSecure;
			case f::secure: return t::Secure;
		}

		return t::Insecure;
	};

	server_admin_.session_list_->SetProtocolInfo(session_id, fz::to_wxString(any.get_name()), convert_status(any.get_status()), [any = std::move(any)](wxWindow *p, const SessionList::Session::Info &i) mutable {
		return create_session_info_sizer(p, i, any);
	});
}

void ServerAdministrator::Dispatcher::operator()(const administration::exception::generic &ex)
{
	server_admin_.CallAfter([&admin = server_admin_, desc = fz::to_wxString(ex.description())] {
		wxMsg::Error(_S("Server error: %s"), desc);

		admin.Disconnect();
	});
}

void ServerAdministrator::Dispatcher::operator()(administration::admin_login::response && v, administration::engine::session &session)
{
	int err = 0;

	if (v) {
		session.enable_sending(true);
		session.enable_dispatching(true);
		session.enable_sending<administration::admin_login>(false);
		session.enable_dispatching<administration::admin_login::response>(false);

		session.set_max_buffer_size(administration::buffer_size_after_login);

		bool unsafe_ptrace_scope = false;
		bool have_some_certificates_expired = false;
		bool ftp_has_host_override = false;

		std::tie(
			server_admin_.server_path_format_,
			unsafe_ptrace_scope,
			server_admin_.server_can_impersonate_,
			server_admin_.server_username_,
			server_admin_.network_interfaces_,
			have_some_certificates_expired,
			server_admin_.any_is_equivalent_,
			ftp_has_host_override,
			server_admin_.server_version_,
			server_admin_.server_host_,
			server_admin_.server_instance_id_
		) = std::move(*v).tuple();

		server_admin_.logger_.log_u(fz::logmsg::status, _S("Successfully connected to server %s. Server's version is %s, built for %s.").ToStdWstring(), server_admin_.server_info_.name, server_admin_.server_version_, server_admin_.server_host_);

		if (unsafe_ptrace_scope && server_admin_.state_ != state::reconnecting)
			server_admin_.logger_.log_u(fz::logmsg::status, fz::to_wstring(_S("WARNING! Server's sysctl kernel.yama.ptrace_scope is set to an unsafe value.")));

		if (have_some_certificates_expired)
			server_admin_.logger_.log_u(fz::logmsg::error, fz::to_wstring(_S("Some of the TLS certificates in use on the server have expired.")));

		if (!server_admin_.network_interfaces_)
			server_admin_.logger_.log_raw(fz::logmsg::debug_warning, L"Couldn't get info from server about network interfaces.");
		else
		if (!ftp_has_host_override) {
			bool has_routable_address = false;

			for (const auto &i: *server_admin_.network_interfaces_) {
				for (const auto &a: i.addresses) {
					has_routable_address = fz::is_routable_address(std::string_view(a).substr(0, a.find_first_of('/')));
					if (has_routable_address)
						break;
				}
			}

			if (!has_routable_address)
				server_admin_.logger_.log_raw(fz::logmsg::warning, L"In order to access the server from the internet first you need to configure the passive mode settings from the Administration interface.\n"
																   L"You will also need to forward the same range of ports in your router.\n"
																   L"The Network Configuration Wizard might help you with that, you find it in the Administration interface Server menu.");
		}

		server_admin_.state_ = state::connected;
		server_admin_.server_status_.num_of_active_sessions = 0;
		server_admin_.maybe_send_server_status_changed();
	}
	else
		err = EACCES;

	Event::Connection.Queue(&server_admin_, &server_admin_, err);
}

/*
void ServerAdministrator::Dispatcher::operator()(administration::engine::session &session, administration::engine::any_message &&any)
{
#if defined(ENABLE_FZ_RMP_DEBUG) && ENABLE_FZ_RMP_DEBUG
	static fz::monotonic_clock start = fz::monotonic_clock::now();
	static std::size_t count;
	static size_t last_delta = 0;

	++count;

	auto delta = (fz::monotonic_clock::now() - start).get_milliseconds();
	auto diff = std::size_t(delta) - last_delta;

	if (delta > 0 && diff >= 10000) {
		auto msg_per_sec = (count * 1000 / diff);
		last_delta = std::size_t(delta);
		count = 0;

		FZ_RMP_DEBUG_LOG("MSG/SEC: %d", msg_per_sec);
	}
#endif

	forwarder::operator()(session, std::move(any));
}
*/

void ServerAdministrator::Dispatcher::connection(administration::engine::session *session, int err)
{
	wxASSERT(!wxThread::IsMain());

	if (server_admin_.is_being_deleted_) {
		return;
	}

	if (err || !session) {
		server_admin_.logger_.log_u(fz::logmsg::error, _S("Failed connection to server %s. Reason: %s.").ToStdWstring(), server_admin_.server_info_.name, fz::socket_error_description(err));


		if (server_admin_.reconnect_timeout_secs_ > 0)
			server_admin_.do_reconnect(err);
		else
			Event::Connection.Queue(&server_admin_, &server_admin_, err);

		return;
	}

	async_handler_ = std::make_unique<fz::async_handler>(server_admin_.loop_);

	session->enable_sending<administration::admin_login>(true);

	session->send<administration::admin_login>(server_admin_.server_info_.password);

	session->enable_dispatching<administration::admin_login::response>(true);
	session->set_max_buffer_size(administration::buffer_size_before_login);
}

void ServerAdministrator::Dispatcher::disconnection(administration::engine::session &, int err)
{
	if (server_admin_.is_being_deleted_) {
		return;
	}

	// We erase the async handler on disconnection, so that any pending async op doesn't get sent in the void.
	async_handler_.reset();

	server_admin_.do_disconnect(err);
}

void ServerAdministrator::Dispatcher::certificate_verification(administration::engine::session &, fz::tls_session_info &&i)
{
	wxASSERT(!wxThread::IsMain());

	if (server_admin_.is_being_deleted_) {
		return;
	}

	invoke_on_admin([i = std::move(i), this] {
		auto set_trusted = [this](bool trusted) {
			server_admin_.certificate_not_trusted_ = !trusted;
			server_admin_.client_.set_certificate_verification_result(trusted);
		};

		auto && certs = i.get_certificates();

		if (certs.empty()) {
			wxMsg::Error(_S("The server didn't send any certificates. We don't trust that."));
			return set_trusted(false);
		}


		auto fingerprint_is_new = server_admin_.server_info_.fingerprint.empty();

		if (!fingerprint_is_new && fz::equal_insensitive_ascii(certs[0].get_fingerprint_sha256(), server_admin_.server_info_.fingerprint))
			return set_trusted(true);

		wxPushDialog(server_admin_.GetParent(), wxID_ANY, _S("Do you trust this server?"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) |[this, set_trusted, fingerprint_is_new, cert = std::move(certs[0])](auto *p) {
			auto extent = wxMonospaceTextExtent(97, 2, p, {wxSYS_BORDER_X}, {wxSYS_BORDER_Y});
			static auto make_monospace = [](wxTextCtrl *p) {
				auto font = p->GetFont();
				font.SetFamily(wxFONTFAMILY_TELETYPE);
				p->SetFont(font);
				p->SetInsertionPoint(0);
			};

			if (fingerprint_is_new) {
				wxVBox(p) = {
					wxLabel(p, _S("Server certificate fingerprint is not known.")),

					wxStaticVBox(p, _S("Fingerprint provided by the server")) = {
						new wxTextCtrl(p, wxID_ANY, fz::to_wxString(cert.get_fingerprint_sha256()), wxDefaultPosition, extent, wxTE_READONLY) | make_monospace
					},

					wxLabel(p, _S("Do you recognize this fingerprint?")),

					wxEmptySpace,
					p->CreateStdDialogButtonSizer(wxYES_NO | wxNO_DEFAULT)
				};
			}
			else {
				wxVBox(p) = {
					wxLabel(p, _S("Server certificate fingerprint does not match with the one saved.")),

					wxStaticVBox(p, _S("Fingerprint provided by the server")) = {
						new wxTextCtrl(p, wxID_ANY, fz::to_wxString(cert.get_fingerprint_sha256()), wxDefaultPosition, extent, wxTE_READONLY) | make_monospace
					},

					wxStaticVBox(p, _S("Saved fingerprint")) = {
						new wxTextCtrl(p, wxID_ANY, fz::to_wxString(server_admin_.server_info_.fingerprint), wxDefaultPosition, extent, wxTE_READONLY) | make_monospace
					},

					wxLabel(p, _S("Do you recognize this fingerprint?")),

					wxEmptySpace,
					p->CreateStdDialogButtonSizer(wxYES_NO | wxNO_DEFAULT)
				};
			}

			p->SetEscapeId(wxID_NO);
			p->SetAffirmativeId(wxID_YES);
			p->Layout();
			p->Fit();

			bool trusted = (p->ShowModal() == wxID_YES);

			if (trusted) {
				server_admin_.server_info_.fingerprint = cert.get_fingerprint_sha256();
				Event::ServerInfoUpdated.Process(&server_admin_, &server_admin_);
			}

			set_trusted(trusted);
		};
	});
}

void ServerAdministrator::Dispatcher::operator()(administration::get_ip_filters::response && v)
{
	invoke_on_admin([this, v = std::move(v)] {
		if (!v) {
			wxMsg::Error(_S("Error retrieving the IPV4 black list"));
			return;
		}

		if (!was_expecting_response())
			return;

		std::tie(server_admin_.disallowed_ips_, server_admin_.allowed_ips_) = std::move(*v).tuple();

		server_admin_.maybe_act_on_settings_received();
	});
}

void ServerAdministrator::Dispatcher::operator()(administration::get_protocols_options::response && v)
{
	invoke_on_admin([this, v = std::move(v)] {
		if (!v) {
			wxMsg::Error(_S("Error retrieving protocols options"));
			return;
		}

		if (!was_expecting_response())
			return;

		std::tie(server_admin_.protocols_options_) = std::move(*v).tuple();

		server_admin_.maybe_act_on_settings_received();
	});
}

void ServerAdministrator::Dispatcher::operator()(administration::get_ftp_options::response && v)
{
	invoke_on_admin([this, v = std::move(v)] {
		if (!v) {
			wxMsg::Error(_S("Error retrieving ftp options"));
			return;
		}

		if (!was_expecting_response())
			return;

		std::tie(server_admin_.ftp_options_, server_admin_.ftp_tls_extra_info_) = std::move(*v).tuple();

		server_admin_.maybe_act_on_settings_received();
	});
}

void ServerAdministrator::Dispatcher::operator()(administration::get_webui_options::response && v)
{
	invoke_on_admin([this, v = std::move(v)] {
		if (!v) {
			wxMsg::Error(_S("Error retrieving WebUI options"));
			return;
		}

		if (!was_expecting_response())
			return;

		std::tie(server_admin_.webui_options_, server_admin_.webui_tls_extra_info_) = std::move(*v).tuple();

		server_admin_.maybe_act_on_settings_received();
	});
}

void ServerAdministrator::Dispatcher::operator()(administration::get_admin_options::response && v)
{
	invoke_on_admin([this, v = std::move(v)] {
		if (!v) {
			wxMsg::Error(_S("Error retrieving admin options"));
			return;
		}

		if (!was_expecting_response())
			return;

		std::tie(server_admin_.admin_options_, server_admin_.admin_tls_extra_info_) = std::move(*v).tuple();

		server_admin_.maybe_act_on_settings_received();
	});
}

void ServerAdministrator::Dispatcher::operator()(administration::get_logger_options::response && v)
{
	invoke_on_admin([this, v = std::move(v)] {
		if (!v) {
			wxMsg::Error(_S("Error retrieving logger options"));
			return;
		}

		if (!was_expecting_response())
			return;

		std::tie(server_admin_.logger_options_) = std::move(*v).tuple();

		server_admin_.maybe_act_on_settings_received();
	});
}

void ServerAdministrator::Dispatcher::operator()(administration::get_pkcs11_options::response && v)
{
	invoke_on_admin([this, v = std::move(v)] {
		if (!v) {
			wxMsg::Error(_S("Error retrieving PKCS#11 options"));
			return;
		}

		if (!was_expecting_response())
			return;

		std::tie(server_admin_.pkcs11_options_) = std::move(*v).tuple();

		server_admin_.maybe_act_on_settings_received();
	});
}

void ServerAdministrator::Dispatcher::operator()(administration::get_deobfuscated_blob::response &&v)
{
	invoke_on_admin([this, v = std::move(v)]() mutable {
		if (server_admin_.rmp_loop_timer_) {
			server_admin_.rmp_loop_timer_->Stop();
		}

		if (!server_admin_.rmp_loop_) {
			return;
		}

		bool success = v.is_success();

		if (success) {
			std::tie(server_admin_.rmp_loop_response_string_) = v->tuple();
		}
		else {
			server_admin_.rmp_loop_response_string_.clear();
		}

		server_admin_.rmp_loop_->Exit();
	});
}

void ServerAdministrator::Dispatcher::operator()(administration::get_public_ip::response &&v)
{
	invoke_on_admin([this, v = std::move(v)]() mutable {
		if (server_admin_.rmp_loop_timer_) {
			server_admin_.rmp_loop_timer_->Stop();
		}

		if (!server_admin_.rmp_loop_) {
			return;
		}

		if (v) {
			std::tie(server_admin_.rmp_loop_response_string_) = std::move(v->tuple());
		}
		else {
			auto && [error] = std::move(v.error().tuple());

			server_admin_.rmp_loop_error_ = fz::to_wxString(error);
			server_admin_.rmp_loop_response_string_.clear();
		}

		server_admin_.rmp_loop_->Exit();
	});
}

fz::expected<std::string, wxString> ServerAdministrator::get_public_ip(fz::address_type at)
{
	wxCHECK_MSG(rmp_loop_ == nullptr, fz::unexpected(wxEmptyString), wxT("Internal error: rmp_loop_ is not NULL"));

	if (!client_.is_connected()) {
		return fz::unexpected(_S("Got disconnected from the server, try again later."));
	}

	public_ip_cache *cache = [at, this]() -> public_ip_cache* {
		if (at == fz::address_type::unknown) {
			return &public_ipvx_cache_;
		}

		if (at == fz::address_type::ipv4) {
			return &public_ipv4_cache_;
		}

		if (at == fz::address_type::ipv6) {
			return &public_ipv6_cache_;
		}

		return nullptr;
	}();

	if (!cache) {
		return fz::unexpected(_S("Invalid IP version"));
	}

	if (cache->expired()) {
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

		client_.send<administration::get_public_ip>(at);
		timer.Start(20000, true);

		loop.Run();

		rmp_loop_  = nullptr;
		rmp_loop_timer_ = nullptr;

		if (!rmp_loop_error_.empty() || rmp_loop_response_string_.empty()) {
			return fz::unexpected(std::move(rmp_loop_error_));
		}

		cache->ip = std::move(rmp_loop_response_string_);
		cache->reset(fz::monotonic_clock::now());
	}

	return cache->ip;
}

void ServerAdministrator::Dispatcher::operator()(administration::destroy_webui_tokens::response && v)
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
			server_admin_.rmp_loop_error_.clear();
		}
		else {
			server_admin_.rmp_loop_error_ = wxT("Unknown error");
		}

		server_admin_.rmp_loop_->Exit();
	});
}

void ServerAdministrator::Dispatcher::operator()(administration::acknowledge_queue_full &&v, administration::engine::session &session)
{
	session.send(v.success());
}

ServerAdministrator::Dispatcher::Dispatcher(ServerAdministrator &server_admin)
	: visitor(*this)
	, server_admin_(server_admin)
	, http_update_retriever_(server_admin.loop_, server_admin_.pool_, server_admin_.logger_)
{
}


void ServerAdministrator::public_ip_cache::reset(fz::monotonic_clock at)
{
	time = std::move(at);
}

bool ServerAdministrator::public_ip_cache::expired()
{
	static const auto expiration = fz::duration::from_seconds(30);

	return expiration < fz::monotonic_clock::now() - time;
}
