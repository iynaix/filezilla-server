#include "../server/administrator.hpp"

#include "../filezilla/debug.hpp"

#include "../filezilla/hostaddress.hpp"
#include "../filezilla/logger/splitter.hpp"

#include "../filezilla/impersonator/util.hpp"
#include "../filezilla/util/io.hpp"
#include "../filezilla/util/proof_of_work.hpp"

#include "administrator/notifier.hpp"
#include "administrator/update_checker.hpp"
#include "administrator/ftp_test_creator.hpp"

struct administrator::session_data
{
	bool is_in_overflow{};
};

administrator::~administrator()
{
	shared_self_.stop_sharing();

	ftp_server_.set_notifier_factory(fz::tcp::session::notifier::factory::none);

	ftp_server_.iterate_over_sessions({}, [](fz::ftp::session &s) mutable {
		auto notifier = static_cast<administrator::notifier *>(s.get_notifier());
		if (notifier)
			notifier->detach_from_administrator();

		return true;
	});

	splitter_logger_.remove_logger(*log_forwarder_);
}

auto administrator::operator()(administration::ban_ip &&v)
{
	auto &&[ip, family] = std::move(v).tuple();

	if (disallowed_ips_.add(ip, family)) {
		kick_disallowed_ips();
		return v.success();
	}

	return v.failure();
}

void administrator::connection(administration::engine::session *session, int err)
{
	if (session) {
		if (err)
			logger_.log_u(fz::logmsg::error, L"Administration client with ID %d attempted to connect from from %s, but failed with error %s.", session->get_id(), fz::join_host_and_port(session->peer_ip(), unsigned(session->peer_port())), fz::socket_error_description(err));
		else {
			logger_.log_u(fz::logmsg::status, L"Administration client with ID %d connected from %s", session->get_id(), fz::join_host_and_port(session->peer_ip(), unsigned(session->peer_port())));

			session->set_user_data(session_data{});

			session->enable_dispatching<administration::admin_login>(true);
			session->enable_sending<administration::admin_login::response>(true);
			session->set_max_buffer_size(administration::buffer_size_before_login);
		}
	}
}

void administrator::disconnection(administration::engine::session &s, int err)
{
	if (err)
		logger_.log_u(fz::logmsg::error, L"Administration client with ID %d disconnected with error %s", s.get_id(), fz::socket_error_description(err));
	else
		logger_.log_u(fz::logmsg::status, L"Administration client with ID %d disconnected without error", s.get_id());

	if (update_checker_)
		update_checker_->on_disconnection(s.get_id());
}

bool administrator::have_some_certificates_expired()
{
	auto s = server_settings_.lock();

	if (s->ftp_server.sessions().tls.cert.load_extra().expired())
		return true;

	if (s->admin.tls.cert.load_extra().expired())
		return true;

	return false;
}

void administrator::kick_disallowed_ips()
{
	std::vector<fz::tcp::session::id> ids;

	auto generate_ids = [&](const fz::tcp::session &s) {
		const auto &[addr, type] = s.get_peer_info();
		if (disallowed_ips_.contains(addr, type))
			ids.push_back(s.get_id());

		return true;
	};

	ftp_server_.iterate_over_sessions({}, std::cref(generate_ids));
	if (!ids.empty())
		ftp_server_.end_sessions(ids);
}

auto administrator::operator()(administration::end_sessions &&v)
{
	auto && [sessions] = std::move(v).tuple();

	auto num = ftp_server_.end_sessions(sessions);

	if(sessions.empty() || num > 0)
		return v.success(num);

	return v.failure();
}

void administrator::send_buffer_is_in_overflow(administration::engine::session &session)
{
	if (auto &sd = session.get_user_data<session_data>(); !sd.is_in_overflow) {
		sd.is_in_overflow = true;

		session.enable_sending<
			administration::session::user_name,
			administration::session::entry_open,
			administration::session::entry_close,
			administration::session::entry_written,
			administration::session::entry_read,
			administration::log,
			administration::listener_status
		>(false);

		session.send<administration::acknowledge_queue_full>();

		engine_logger_.log_u(fz::logmsg::debug_warning, L"Administrator: upload buffer has overflown! Silencing notifications until the client informs us it has exausted the queue.");
	}
}

std::unique_ptr<fz::tcp::session::notifier> administrator::make_notifier(fz::ftp::session::id id, const fz::datetime &start, const std::string &peer_ip, fz::address_type peer_address_type, fz::logger_interface &logger)
{
	return std::make_unique<notifier>(*this, id, start, peer_ip, peer_address_type, logger);
}

void administrator::listener_status(const fz::tcp::listener &listener)
{
	if (admin_server_.get_number_of_sessions() < 1)
		return;

	admin_server_.broadcast<administration::listener_status>(fz::datetime::now(), listener.get_address_info(), listener.get_status());
}

bool administrator::handle_new_admin_settings()
{
	auto server_settings = server_settings_.lock();

	auto &admin = server_settings->admin;

	bool enable_local_ipv4 = admin.local_port != 0;
	bool enable_local_ipv6 = admin.local_port != 0 && admin.enable_local_ipv6;

	std::vector<fz::rmp::address_info> address_info_list;

	if (admin.password) {
		static constexpr fz::hostaddress::ipv4_host local_ipv4 = *fz::hostaddress("127.0.0.1", fz::hostaddress::format::ipv4).ipv4();
		static constexpr fz::hostaddress::ipv6_host local_ipv6 = *fz::hostaddress("::1", fz::hostaddress::format::ipv6).ipv6();

		for (auto &i: admin.additional_address_info_list) {
			if (i.port != admin.local_port)
				continue;

			if (fz::hostaddress r(i.address, fz::hostaddress::format::ipv4); r.is_valid()) {
				if (*r.ipv4() == local_ipv4 || *r.ipv4() == fz::hostaddress::ipv4_host{})
					enable_local_ipv4 = false;
			}
			else
			if (fz::hostaddress r(i.address, fz::hostaddress::format::ipv6); r.is_valid()) {
				if (*r.ipv6() == local_ipv6 || *r.ipv6() == fz::hostaddress::ipv6_host{})
					enable_local_ipv6 = false;
			}
		}

		address_info_list.reserve(admin.additional_address_info_list.size() + enable_local_ipv4 + enable_local_ipv6);

		std::copy(admin.additional_address_info_list.begin(), admin.additional_address_info_list.end(), std::back_inserter(address_info_list));
	}
	else {
		logger_.log_u(fz::logmsg::warning, L"No valid password is set.");

		if (!admin.additional_address_info_list.empty()) {
			logger_.log_u(fz::logmsg::warning, L"A list of listener is specified, but no valid password is set: this is not supported. Ignoring the provided listeners.");
		}
	}

	if (enable_local_ipv4)
		address_info_list.push_back({{"127.0.0.1", admin.local_port}, true});

	if (enable_local_ipv6)
		address_info_list.push_back({{"::1", admin.local_port}, true});

	admin_server_.set_listen_address_infos(address_info_list);

	if (admin.tls.cert)
		admin.tls.cert.set_root_path(config_paths_.certificates());

	admin_server_.set_security_info(admin.tls);

	if (address_info_list.empty()) {
		logger_.log_u(fz::logmsg::debug_warning, L"No listeners were enabled. Will not serve!");
		return false;
	}

	return true;
}

std::pair<std::string, fz::securable_socket::cert_info &>administrator::get_admin_cert(server_settings &ss)
{
	return {"Administration", ss.admin.tls.cert};
}

std::pair<std::string, fz::securable_socket::cert_info &>administrator::get_ftp_cert(server_settings &ss)
{
	return {"FTP", ss.ftp_server.sessions().tls.cert};
}

std::pair<std::string, fz::securable_socket::cert_info &>administrator::get_webui_cert(server_settings &ss)
{
	return {"WebUI", ss.webui.tls.cert};
}

void administrator::set_acme_certificate_for_renewal(std::function<std::pair<std::string, fz::securable_socket::cert_info &> (server_settings &)> info_retriever, bool do_renew)
{
	if (do_renew) {
		acme_.set_certificate(info_retriever(*server_settings_.lock()), [this, info_retriever](fz::securable_socket::cert_info ci) {
			assert(ci.omni()->acme());

			info_retriever(*server_settings_.lock()).second = std::move(ci);
			server_settings_.save_later();
		});

	}
	else {
		acme_.set_certificate(info_retriever(*server_settings_.lock()), nullptr);
	}
}


auto administrator::operator()(administration::session::solicit_info &&v, administration::engine::session &session)
{
	auto && [session_ids] = std::move(v).tuple();

	// We cap the number of sessions to be retrieved to a given maximum, to avoid stalling the server.
	constexpr size_t max_num_info = 10'000;

	ftp_server_.iterate_over_sessions(session_ids, [&session, sent_so_far = std::size_t(0)](fz::ftp::session &s) mutable {
		if (sent_so_far == max_num_info)
			return false;

		auto notifier = static_cast<administrator::notifier *>(s.get_notifier());
		if (notifier)
			notifier->send_session_info(session);

		return true;
	});
}

auto administrator::operator()(administration::admin_login &&v, administration::engine::session &session)
{
	auto [password] = std::move(v).tuple();

	if (server_settings_.lock()->admin.password.verify(password)) {
		bool unsafe_ptrace_scope = false;
		#ifdef __linux__
			unsafe_ptrace_scope = fz::trimmed(fz::util::io::read(fzT("/proc/sys/kernel/yama/ptrace_scope")).to_view()) == "0";
		#endif
		session.send(v.success(
			fz::util::fs::native_format,
			unsafe_ptrace_scope,
			fz::impersonator::can_impersonate(),
			fz::current_username(),
			fz::get_network_interfaces(),
			have_some_certificates_expired(),
			fz::hostaddress::any_is_equivalent,
			!server_settings_.lock()->ftp_server.sessions().pasv.host_override.empty(),
			fz::build_info::version,
			fz::build_info::host,
			instance_id_
		));

		// Enable all messages
		session.enable_dispatching(true);
		session.enable_sending(true);

		// Except for the login ones, sice we're now logged in.
		session.enable_dispatching<administration::admin_login>(false);
		session.enable_sending<administration::admin_login::response>(false);

		session.set_max_buffer_size(administration::buffer_size_after_login);

		// Send ftp sessions info to the admin session
		operator()(administration::session::solicit_info{}, session);

		if (update_checker_) {
			// Send the last available update info
			session.send<administration::update_info>(update_checker_->get_last_checked_info(), update_checker_->get_last_check_dt(), update_checker_->get_next_check_dt());
		}

		return;
	}

	session.send(v.failure());
}


auto administrator::operator ()(administration::acknowledge_queue_full::response &&, administration::engine::session &session)
{
	if (auto &sd = session.get_user_data<session_data>(); sd.is_in_overflow) {
		sd.is_in_overflow = false;

		engine_logger_.log_u(fz::logmsg::debug_warning, L"Administrator: upload buffer has been emptied by the client! Enabling notifications again.");

		session.enable_sending<
			administration::session::user_name,
			administration::session::entry_open,
			administration::session::entry_close,
			administration::session::entry_written,
			administration::session::entry_read,
			administration::log,
			administration::listener_status
		>(true);

		// Send ftp sessions info to the admin session
		operator()(administration::session::solicit_info{}, session);
	}
}

auto administrator::operator()(administration::get_public_ip &&v, administration::engine::session &session)
{
	// Using a separate thread because the proof of work can take some time to run and we don't wanna stall the server.
	server_context_.pool().spawn([sa = shared_self_, id = session.get_id(), address_type = std::get<0>(v.tuple())]() mutable {
		auto a = sa.lock();
		if (!a) {
			return;
		}

		fz::http::headers h;
		h["Content-Type"] = "application/x-www-form-urlencoded";

		auto qs = fz::util::proof_of_work("resolve", 16, {{"", a->http_.get_options().user_agent()}});

		auto service = fz::uri("http://ip.filezilla-project.org/resolve.php");

		a->http_.perform("POST", std::move(service), std::move(h), qs.to_string(false)).with_address_type(address_type).and_then([sa = std::move(sa), id](fz::http::response::status status, fz::http::response &r) mutable {
			auto a = sa.lock();
			if (!a) {
				return ECANCELED;
			}

			auto s = a->admin_server_.get_session(id);
			if (!s) {
				return ECANCELED;
			}

			if (r.code_type() != r.successful) {
				s->send<administration::get_public_ip::response>(fz::unexpected(fz::sprintf(fzT("%s - %s"), r.code_string(), r.reason)));
				return ECANCELED;
			}

			if (status == fz::http::response::got_end) {
				s->send<administration::get_public_ip::response>(std::string(r.body.to_view()));
			}

			return 0;
		});
	}).detach();
}

/**********************************************************************/

void administrator::reload_config()
{
	invoke_later_([this] {
		fz::authentication::file_based_authenticator::groups groups;
		fz::authentication::file_based_authenticator::users users;
		fz::tcp::binary_address_list disallowed_ips, allowed_ips;
		server_settings server_settings;

		fz::serialization::xml_input_archive::error_t err;

		if (!err)
			err = authenticator_.load_into(groups, users);

		if (!err)
			err = server_settings_.load_into(server_settings);

		if (!err)
			err = disallowed_ips_.load_into(disallowed_ips);

		if (!err)
			err = allowed_ips_.load_into(allowed_ips);

		if (err) {
			logger_.log_u(fz::logmsg::error, L"Failed reloading configuration. Reason: %s.", err.description());
			return;
		}

		set_groups_and_users(std::move(groups), std::move(users));
		set_logger_options(std::move(server_settings.logger));
		set_ftp_options(std::move(server_settings.ftp_server));
		set_protocols_options(std::move(server_settings.protocols));
		set_admin_options(std::move(server_settings.admin));
		set_acme_options(std::move(server_settings.acme));
		set_pkcs11_options(std::move(server_settings.pkcs11));
		set_ip_filters(std::move(disallowed_ips), std::move(allowed_ips), false);
		set_updates_options(std::move(server_settings.update_checker));
		set_webui_options(std::move(server_settings.webui));

		logger_.log_u(fz::logmsg::status, L"Successfully reloaded configuration.");
	});
}

administrator::administrator(fz::tcp::server::context &context,
							 fz::event_loop_pool &loop_pool,
							 fz::logger::file &file_logger,
							 fz::logger::splitter &splitter_logger,
							 fz::ftp::server &ftp_server,
	#ifdef ENABLE_FZ_WEBUI
							 fz::webui::server &webui_server,
	#endif
							 fz::tcp::automatically_serializable_binary_address_list &disallowed_ips,
							 fz::tcp::automatically_serializable_binary_address_list &allowed_ips,
							 fz::authentication::autobanner &autobanner,
							 fz::authentication::file_based_authenticator &authenticator,
							 fz::util::xml_archiver<server_settings> &server_settings,
							 fz::acme::daemon &acme,
							 const server_config_paths &config_paths,
							 fz::tls_system_trust_store &trust_store)
	: visitor(*this)
	, server_context_(context)
	, file_logger_(file_logger)
	, splitter_logger_(splitter_logger)
	, engine_logger_(file_logger, "Administration Server")
	, logger_(splitter_logger, "Administration Server")
	, loop_pool_(loop_pool)
	, ftp_server_(ftp_server)
	#ifdef ENABLE_FZ_WEBUI
		, webui_server_(webui_server)
	#endif
	, disallowed_ips_(disallowed_ips)
	, allowed_ips_(allowed_ips)
	, autobanner_(autobanner)
	, authenticator_(authenticator)
	, server_settings_(server_settings)
	, acme_(acme)
	, config_paths_(config_paths)
	, trust_store_(trust_store)
	, log_forwarder_(new log_forwarder(*this, 0))
	#if !defined(WITHOUT_FZ_UPDATE_CHECKER)
		, update_checker_(new update_checker(*this, config_paths.update() / fz::build_info::toString<fz::native_string>(fz::build_info::flavour) / fzT("cache"), server_settings_.lock()->update_checker))
	#endif
	, ftp_test_creator_(new ftp_test_creator(*this))
	, http_(server_context_.pool(), server_context_.loop(), logger_, fz::http::client::options()
		.follow_redirects(true)
		.trust_store(&trust_store_)
		.default_timeout(fz::duration::from_seconds(10))
	)
	, invoke_later_(context.loop())
	, admin_server_(context, *this, engine_logger_)
	, instance_id_(fz::random_bytes(32))
{
	log_forwarder_->set_all(fz::logmsg::type(~0));

	splitter_logger_.add_logger(*log_forwarder_);

	if (handle_new_admin_settings()) {
		ftp_server_.set_notifier_factory(*this);

		admin_server_.start();
	}

	if (update_checker_)
		update_checker_->start();

	if (auto s = server_settings_.lock()) {
		acme_.set_root_path(config_paths_.certificates());
		acme_.set_how_to_serve_challenges(s->acme.how_to_serve_challenges);
		set_acme_certificate_for_renewal(get_ftp_cert, true);
		set_acme_certificate_for_renewal(get_admin_cert, true);
		set_acme_certificate_for_renewal(get_webui_cert, true);
	}
}

