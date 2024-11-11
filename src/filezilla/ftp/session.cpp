#include <libfilezilla/util.hpp>
#include <libfilezilla/buffer.hpp>
#include <libfilezilla/local_filesys.hpp>
#include <libfilezilla/rate_limited_layer.hpp>
#include <libfilezilla/hostname_lookup.hpp>

#include "../authentication/authenticator.hpp"
#include "../authentication/error.hpp"

#include "../remove_event.hpp"

#include <string_view>
#include <cinttypes>
#include <cassert>

#include "../ftp/session.hpp"
#include "../ftp/ascii_layer.hpp"
#include "../util/thread_id.hpp"

namespace fz::ftp {

using namespace std::string_literals;
using namespace std::string_view_literals;

session::protocol_info::status session::protocol_info::get_status() const
{
	if (!security)
		return protocol_info::insecure;

	if (security->algorithm_warnings)
		return protocol_info::not_completely_secure;

	return protocol_info::secure;
}

std::string session::protocol_info::get_name() const
{
	return security ? "FTPS"s : "FTP"s;
}

session::session(fz::thread_pool &pool, event_loop &loop, event_handler &target_event_handler,
				 rate_limit_manager &rate_limit_manager,
				 std::unique_ptr<notifier> notifier,
				 id id,
				 datetime start,
				 std::unique_ptr<socket> control_socket,
				 session::tls_mode tls_mode,
				 authentication::autobanner &autobanner,
				 authentication::authenticator &authenticator,
				 port_manager &port_manager,
				 const commander::welcome_message &welcome_message, const std::string &refuse_message,
				 options opts)
	: tcp::session(target_event_handler, id, {control_socket->peer_ip(), control_socket->address_family()})
	, event_handler(loop)
	, session_limiter_(&rate_limit_manager)
	, pool_(pool)
	, notifier_{std::move(notifier)}
	, logger_(notifier_->logger(), "FTP Session", {{"id", std::to_string(id)}, {"host", control_socket->peer_ip()}}, logger_info_to_string)
	, start_datetime_{start}
	, control_socket_(loop, this, std::move(control_socket), logger_)
	, port_manager_(port_manager)
	, opts_(std::move(opts))
	, tvfs_(logger_)
	, autobanner_(autobanner)
	, authenticator_(authenticator)
	, commander_buffer_operators_(loop, logger_)
	, commander_(loop, *this, tvfs_, *notifier_, last_activity_, tls_mode == require_tls, welcome_message, refuse_message, commander_buffer_operators_, logger_)
	, invoke_later_(loop)
{
	control_socket_.set_unexpected_eof_cb([this] { return !must_downgrade_log_level();} );

	logger_.log_u(logmsg::debug_info, L"Session %p with ID %zu created.", this, id_);

	control_limiter_ = &control_socket_.emplace<compound_rate_limited_layer>(nullptr, control_socket_.top());

	invoke_later_([this, tls_mode] {
		FZ_UTIL_THREAD_CHECK

		if (tls_mode == implicit_tls) {
			if (!control_socket_.make_secure_server(opts_.tls.min_tls_ver, opts_.tls.cert, {}, {}, {"x-filezilla-ftp", "ftp"}))
				quit(EPROTO);
		}
		else {
			commander_.set_socket(&control_socket_);
			notifier_->notify_protocol_info(get_protocol_info());
		}
	});
}

session::~session() {
	remove_handler();

	logger_.log_u(logmsg::debug_info, L"Session %p with ID %zu destroyed.", this, id_);
	authenticator_.stop_ongoing_authentications(*this);
	unsubscribe(user_, *this);
}

session::notifier *session::get_notifier()
{
	return notifier_.get();
}

bool session::is_alive() const
{
	FZ_UTIL_THREAD_CHECK

	return control_socket_.get_state() == socket_state::connected;
}

void session::shutdown(int err)
{
	commander_.shutdown(err);
}

void session::on_shared_user_changed_event(const authentication::weak_user &su)
{
	FZ_UTIL_THREAD_CHECK

	if (!user_ || su.lock() != user_)
		return;

	auto user = user_->lock();

	if (user->id.empty()) {
		quit();
		return;
	}

	tvfs_.set_mount_tree(user->mount_tree);
	tvfs_.set_backend(user->impersonator);
	tvfs_.set_open_limits(user->session_open_limits);

	session_limiter_.set_limits(user->session_inbound_limit, user->session_outbound_limit);
	user_limiter_ = user->limiter;

	update_limits(control_limiter_, &user->extra_limiters);
	update_limits(data_limiter_, &user->extra_limiters);

	extra_limiters_ = user->extra_limiters;
}

void session::on_timer_event(timer_id id)
{
	if (id == check_if_control_is_secured_id_) {
		if (is_secure()) {
			stop_timer(id);
			notifier_->notify_protocol_info(get_protocol_info());
		}
	}
}

void session::set_options(options opts)
{
	invoke_later_([this, opts = std::move(opts)] () mutable {
		opts_ = std::move(opts);
	});
}

void session::set_data_buffer_sizes(int32_t receive, int32_t send)
{
	invoke_later_([this, receive, send] {
		receive_buffer_size_ = receive;
		send_buffer_size_    = send;

		do_set_buffer_sizes();
	});
}

void session::set_timeouts(const duration &login_timeout, const duration &activity_timeout)
{
	invoke_later_([this, login_timeout, activity_timeout] {
		commander_.set_timeouts(login_timeout, activity_timeout);
	});
}

session::protocol_info session::get_protocol_info() const
{
	return { control_socket_.get_session_info() };
}

void session::do_set_buffer_sizes()
{
	static_assert(
		std::numeric_limits<decltype (receive_buffer_size_)>::max() <= std::numeric_limits<int>::max(),
		"The type of the buffer size options can hold more values than the parameters of the socket_base::set_buffer_sizes() method"
	);

	if (data_listen_socket_)
		data_listen_socket_->set_buffer_sizes(receive_buffer_size_, send_buffer_size_);

	if (data_socket_)
		data_socket_->set_buffer_sizes(receive_buffer_size_, send_buffer_size_);
}

address_type session::get_control_socket_address_family() const
{
	return control_socket_.address_family();
}

void session::authenticate_user(std::string_view user, const authentication::methods_list &methods, controller::authenticate_user_response_handler *response_handler)
{
	FZ_UTIL_THREAD_CHECK

	authenticate_user_response_handler_ = response_handler;
	user_.reset();

	authenticator_.authenticate(user, methods, control_socket_.address_family(), control_socket_.peer_ip(), *this, { { "FTP Session", fz::to_string(id_) } });
}

void session::stop_ongoing_user_authentication()
{
	authenticator_.stop_ongoing_authentications(*this);
}

bool session::is_authenticated() const
{
	FZ_UTIL_THREAD_CHECK

	return user_.get() != nullptr;
}

void session::make_secure(std::string_view preamble, controller::make_secure_response_handler *response_handler)
{
	FZ_UTIL_THREAD_CHECK

	auto && securer = control_socket_.make_secure_server(opts_.tls.min_tls_ver, opts_.tls.cert, {}, preamble, {"x-filezilla-ftp", "ftp"});

	if (response_handler)
		response_handler->handle_make_secure_response(securer);

	// To know whether the control socket has been secured we need to poll, because the connection event is delivered to the socket adapter,
	// deep into the guts of the channel embedded into the commander.
	check_if_control_is_secured_id_ = add_timer(fz::duration::from_milliseconds(100), false);
}

controller::secure_state session::get_secure_state() const
{
	FZ_UTIL_THREAD_CHECK

	if (auto s = control_socket_.get_securable_state(); s == securable_socket_state::securing)
		return secure_state::securing;
	else
	if (s == securable_socket_state::secured)
		return secure_state::secured;

	return secure_state::insecure;
}

std::string session::get_alpn() const
{
	return control_socket_.get_alpn();
}

void session::quit(int err)
{
	FZ_UTIL_THREAD_CHECK

	logger_.erase_meta("user");

	target_handler_.send_event<ended_event>(id_, channel::error_type(err, channel::error_source::socket));
}

void session::get_data_local_info(address_type family, bool do_resolve_hostname, data_local_info_handler &handler)
{
	FZ_UTIL_THREAD_CHECK

	if (family == address_type::unknown)
		family = control_socket_.address_family();

	if (control_socket_.is_secure()) {
		if (int err = control_socket_.new_session_ticket(); err != 0) {
			logger_.log_u(logmsg::error, L"Failed new_session_ticket() on the control socket. Reason: %s. Closing the session now.", fz::socket_error_description(err));
			return quit(err);
		}
	}

	previous_data_transfer_error_ = {};
	data_socket_.reset();
	data_listen_socket_ = std::make_unique<listen_socket>(pool_, static_cast<event_handler *>(this));
	do_set_buffer_sizes();

	if (!data_listen_socket_->bind(control_socket_.local_ip())) {
		logger_.log_u(logmsg::error, "Failed data_listen_socket_->bind()");
		return handler.handle_data_local_info(std::nullopt);
	}

	auto listen = [this, family](int port){
		logger_.log_u(logmsg::debug_verbose, "Trying listen(%d, %d) for data connection.", family, port);
		int error = data_listen_socket_->listen(family, port);
		if (error) {
			logger_.log_u(logmsg::error, "Failed listen(%d, %d) for data connection. Reason: %s.", family, port, socket_error_description(error));
			return false;
		}
		return true;
	};

	if (opts_.pasv.port_range) {
		port_randomizer randomizer(port_manager_, control_socket_.peer_ip(), opts_.pasv.port_range->min, opts_.pasv.port_range->max);

		int num_tries_left = 15;

		while (num_tries_left-- > 0) {
			data_port_lease_ = randomizer.get_port();
			int port = data_port_lease_.get_port();

			if (port == 0) {
				logger_.log_u(logmsg::debug_verbose, "Leaser's get_port() returned 0.");
				num_tries_left = 0;
				break;
			}

			if (listen(port))
				break;

			data_port_lease_ = {};
		}

		if (num_tries_left <= 0) {
			logger_.log_u(logmsg::error, "Could not open listen port for passive data connection, check your passive mode port range.");
			return handler.handle_data_local_info(std::nullopt);
		}
	}
	else
	if (!listen(0))
		return handler.handle_data_local_info(std::nullopt);

	if (!do_resolve_hostname || opts_.pasv.host_override.empty() || (opts_.pasv.do_not_override_host_if_peer_is_local && !fz::is_routable_address(control_socket_.peer_ip()))) {
		int error;
		int port = data_listen_socket_->local_port(error);
		if (port < 0) {
			logger_.log_u(logmsg::error, "data_listen_socket_->local_port() failed. Reason: %s.", socket_error_description(error));
			return handler.handle_data_local_info(std::nullopt);
		}

		return handler.handle_data_local_info(std::pair{ data_listen_socket_->local_ip(), std::uint16_t(port) });
	}

	if (!hostname_lookup_)
		hostname_lookup_ = std::make_unique<hostname_lookup>(pool_, *static_cast<event_handler*>(this));

	hostname_lookup_->reset();

	data_local_info_handler_ = &handler;
	logger_.log(logmsg::debug_info, L"Looking up host '%s' for PASV mode.", opts_.pasv.host_override);
	if (!hostname_lookup_->lookup(opts_.pasv.host_override, family)) {
		logger_.log(logmsg::error, L"Host '%s' lookup failed.", opts_.pasv.host_override);

		data_local_info_handler_ = nullptr;
		return handler.handle_data_local_info(std::nullopt);
	}
}

bool session::set_data_peer_hostaddress(hostaddress h)
{
	FZ_UTIL_THREAD_CHECK

	if (control_socket_.is_secure()) {
		if (int err = control_socket_.new_session_ticket(); err != 0) {
			logger_.log_u(logmsg::error, L"Failed control_socket_.new_session_ticket(). Reason: %s. Closing the session now.", fz::socket_error_description(err));
			quit(err);
			return false;
		}
	}

	data_peer_hostaddress_ = std::move(h);

	previous_data_transfer_error_ = {};
	data_listen_socket_.reset();
	data_socket_ = std::make_unique<securable_socket>(event_loop_, static_cast<event_handler*>(this), std::make_unique<socket>(pool_, static_cast<event_handler *>(this)), logger_);
	do_set_buffer_sizes();

	if (!data_socket_->bind(control_socket_.local_ip())) {
		logger_.log_u(logmsg::error, "Failed data_socket_->bind()");
		data_socket_.reset();
		return false;
	}

	data_limiter_ = &data_socket_->emplace<compound_rate_limited_layer>(static_cast<event_handler*>(this), data_socket_->top());
	return true;
}

bool session::is_data_connection_setup()
{
	FZ_UTIL_THREAD_CHECK

	return 	data_socket_ || data_listen_socket_;
}

controller::set_mode_result session::set_data_mode(data_mode mode)
{
	FZ_UTIL_THREAD_CHECK

	switch (mode) {
		case data_mode::S: return set_mode_result::enabled;
		case data_mode::Z: return set_mode_result::not_enabled; //TODO: implement Z MODE
		case data_mode::B: return set_mode_result::not_implemented;
		case data_mode::C: return set_mode_result::not_implemented;

		case data_mode::unknown: break;
	}

	return controller::set_mode_result::unknown;
}

controller::set_mode_result session::set_data_protection_mode(data_protection_mode mode)
{
	FZ_UTIL_THREAD_CHECK

	if (!control_socket_.is_secure())
		return set_mode_result::not_enabled;

	bool allow_unsecure_data_with_secure_control = false; //TODO: or true. Depending on some configuration parameters.

	set_mode_result result = set_mode_result::unknown;
	switch (mode) {
		case data_protection_mode::C: result = (allow_unsecure_data_with_secure_control ? set_mode_result::enabled : set_mode_result::not_allowed); break;
		case data_protection_mode::E: result = set_mode_result::not_implemented; break;
		case data_protection_mode::S: result = set_mode_result::not_implemented; break;
		case data_protection_mode::P: result = set_mode_result::enabled; break;

		case data_protection_mode::unknown: break;
	}

	if (result == set_mode_result::enabled)
		data_protection_mode_ = mode;

	return result;
}

void session::start_data_transfer(buffer_operator::adder_interface &adder, controller::data_transfer_handler *handler, bool is_binary)
{
	FZ_UTIL_THREAD_CHECK

	data_adder_ = &adder;
	data_is_binary_ = is_binary;

	data_transfer_handler_ = handler;

	if (setup_data_channel())
		handle_data_transfer(controller::data_transfer_handler::started, {}, data_socket_ ?  "Starting data transfer." : "About to start data transfer.");
}

void session::start_data_transfer(buffer_operator::consumer_interface &consumer, controller::data_transfer_handler *handler, bool is_binary)
{
	FZ_UTIL_THREAD_CHECK

	data_consumer_ = &consumer;
	data_is_binary_ = is_binary;

	data_transfer_handler_ = handler;

	if (setup_data_channel())
		handle_data_transfer(controller::data_transfer_handler::started, {}, data_socket_ ? "Starting data transfer." : "About to start data transfer.");
}

void session::notify_channel_socket_read_amount(const monotonic_clock &time_point, int64_t amount)
{
	last_activity_ = time_point;

	notifier_->notify_entry_write(1, amount - data_previous_read_amount_, -1);
	data_previous_read_amount_ = amount;
}

void session::notify_channel_socket_written_amount(const monotonic_clock &time_point, int64_t amount)
{
	last_activity_ = time_point;
	notifier_->notify_entry_read(1, amount - data_previous_written_amount_, -1);
	data_previous_written_amount_ = amount;
}

bool session::setup_data_channel()
{
	FZ_UTIL_THREAD_CHECK

	if (data_socket_) {
		if (data_socket_->get_state() == socket_state::none) {
			int error = data_socket_->connect(data_peer_hostaddress_.host(), data_peer_hostaddress_.port());
			if (error)
				return handle_data_transfer(controller::data_transfer_handler::connecting, {error, channel::error_source::socket});
			return true;
		}

		if (data_protection_mode_ == data_protection_mode::C && control_socket_.is_secure()) {
			std::string_view error_msg = "PROT C is not allowed when the control connection is secure. Use PROT P.";

			logger_.log_u(logmsg::error, L"%s", error_msg);
			return handle_data_transfer(controller::data_transfer_handler::started, {EPROTO, channel::error_source::socket}, error_msg);
		}

		if (data_protection_mode_ == data_protection_mode::P) {
			logger_.log_u(logmsg::debug_debug, L"Client wants a secure data connection.");

			std::string_view error_msg = "Unknown securer state.";

			std::vector<std::string> alpns;
			if (control_socket_.get_alpn() == "x-filezilla-ftp"sv)
				alpns.emplace_back("ftp-data");

			if (auto securer = data_socket_->make_secure_server(opts_.tls.min_tls_ver, opts_.tls.cert, &control_socket_, {}, alpns, !alpns.empty()); true)
			switch (securer.get_state()) {
				case securable_socket_state::about_to_secure:
					logger_.log_u(logmsg::debug_debug, L"Making the data connection secure.");

					data_socket_->set_flags(socket::flag_keepalive | socket::flag_nodelay);
					data_socket_->set_keepalive_interval(duration::from_seconds(30));
					return true;

				case securable_socket_state::securing:
					logger_.log_u(logmsg::debug_debug, L"The data connection is not secure yet. Waiting for the related connection event.");
					return true;

				case securable_socket_state::secured:
					logger_.log_u(logmsg::debug_debug, L"The data connection is now secure.");
					data_socket_->set_flags(socket::flag_nodelay, false);
					error_msg = {};
					break;

				/*** Error conditions ***/

				case securable_socket_state::insecure:
					error_msg = "Failed making the data connection secure!";
					break;

				case securable_socket_state::session_not_resumed:
					error_msg = "TLS session of data connection not resumed.";
					break;

				case securable_socket_state::wrong_alpn:
					error_msg = "No or wrong ALPN on data connection";
					break;

				case securable_socket_state::failed_setting_certificate_file:
					error_msg = "Failed setting certificate file.";
					break;

				case securable_socket_state::session_socket_not_secure:
					error_msg = "Control socket is not secure.";
					break;

				case securable_socket_state::invalid_socket_state:
					error_msg = "Invalid socket state.";
					break;
			}

			if (!error_msg.empty()) {
				logger_.log_u(logmsg::error, L"%s", error_msg);
				return handle_data_transfer(controller::data_transfer_handler::connecting, {EPROTO, channel::error_source::socket}, error_msg);
			}
		}
		else {
			data_socket_->set_flags(socket::flag_keepalive | socket::flag_nodelay);
			data_socket_->set_keepalive_interval(duration::from_seconds(30));
		}

		if (data_adder_ || data_consumer_) {
			#if !(defined(FZ_WINDOWS) && FZ_WINDOWS)
				if (!data_is_binary_)
					data_socket_->emplace<ascii_layer>(static_cast<event_handler*>(this), data_socket_->top());
			#endif

			update_limits(data_limiter_);

			if (logger_.should_log(logmsg::debug_debug))
				data_channel_.dump_state(logger_);

			data_channel_.set_buffer_adder(data_adder_);
			data_channel_.set_buffer_consumer(data_consumer_);
			data_channel_.set_socket(data_socket_.get());
		}
	}
	else
	// The data socket doesn't even exist yet. Are we actually passively waiting for somebody to connect to us?
	if (!data_listen_socket_) {
		// Nope. Therefore the USER didn't issue a PASV neither a PORT command *AND* the server was not set up to connect to a default address/port.

		assert(data_transfer_handler_ != nullptr);

		return handle_data_transfer(controller::data_transfer_handler::connecting, {ENOTSOCK, channel::error_source::socket});
	}

	return true;
}

void session::data_socket_shutdown(channel::error_type error)
{
	FZ_UTIL_THREAD_CHECK

	assert(data_socket_);

	if (!data_shutting_down_) {
		data_shutting_down_ = true;
		data_socket_->set_event_handler(this);
	}

	int ret = data_socket_->shutdown();

	logger_.log_u(logmsg::debug_verbose, L"data_socket_->shutdown() = %d", ret);

	if (ret == EAGAIN) {
		data_socket_shutdown_error_ = error;
		return;
	}

	handle_data_transfer(controller::data_transfer_handler::stopped, error);
}

bool session::handle_data_transfer(controller::data_transfer_handler::status status, channel::error_type error, std::string_view msg)
{
	FZ_UTIL_THREAD_CHECK

	if (data_transfer_handler_) {
		if (previous_data_transfer_error_) {
			status = previous_data_transfer_status_;
			error = previous_data_transfer_error_;
			msg = previous_data_transfer_msg_;
		}

		data_transfer_handler_->handle_data_transfer(status, error, msg);

		previous_data_transfer_error_ = {};
	}
	else {
		previous_data_transfer_error_ = error;

		previous_data_transfer_status_ = status;
		previous_data_transfer_msg_ = msg;
		previous_data_transfer_is_consuming_ = data_consumer_ != nullptr;
	}

	if (error || status == controller::data_transfer_handler::stopped) {
		close_data_connection();
		return false;
	}

	return true;
}

std::string session::logger_info_to_string(const logger::modularized::info &i, const logger::modularized::info_list &)
{
	std::string ret = i.name;

	ret
		.append(" ").append(*i.find_meta("id"))
		.append(" ").append(*i.find_meta("host"));

	if (auto u = i.find_meta("user"))
		ret.append(" ").append(*u);

	return ret;
}

controller::data_connection_status session::close_data_connection()
{
	FZ_UTIL_THREAD_CHECK

	data_connection_status prev_status = data_connection_status::not_started;
	if (data_socket_ && data_transfer_handler_)
		prev_status = data_connection_status::started;
	else
	if (data_transfer_handler_)
		prev_status = data_connection_status::waiting;

	logger_.log_u(logmsg::debug_debug, L"session::close_data_connection(): prev data_connection_status = %d", prev_status);

	auto removed = remove_events<channel::done_event>(this, data_channel_);
	logger_.log_u(logmsg::debug_debug, L"Removed done events: %d", removed);

	data_channel_.clear();

	data_socket_.reset();
	data_listen_socket_.reset();
	data_port_lease_ = {};

	data_local_info_handler_ = nullptr;

	data_adder_ = nullptr;
	data_consumer_ = nullptr;
	data_transfer_handler_ = nullptr;
	data_limiter_ = nullptr;
	data_shutting_down_ = false;

	return prev_status;
}

void session::on_socket_event(fz::socket_event_source *source, fz::socket_event_flag type, int error)
{
	FZ_UTIL_THREAD_CHECK

	bool is_unexpected_event = false;

	const static auto source2name = [](fz::socket_event_source *source, session *self) {
		if (source->root() == self->control_socket_.root())
			return "control";

		if (self->data_listen_socket_ && source->root() == self->data_listen_socket_->root())
			return "data listen";

		if (self->data_socket_ && source->root() == self->data_socket_->root())
			return "data";

		return "none";
	};

	const static auto get_state = [](fz::socket_event_source *source, session *self) -> int {
		if (source->root() == self->control_socket_.root())
			return int(self->control_socket_.get_state());

		if (self->data_socket_ && source->root() == self->data_socket_->root())
			return int(self->data_socket_->get_state());

		return -1;
	};

	logger_.log_u(logmsg::debug_verbose, L"session::on_socket_event(): source = %s, flag = %d, error = %d, state = %d", source2name(source, this), type, error, get_state(source, this));

	if (error) {
		// Control connection
		if (source->root() == control_socket_.root()) {
			logger_.log_u(logmsg::error, L"Failed securing control connection. Reason: %s.", socket_error_description(error));
			quit(error);
		}
		// Data connection
		else {
			logger_.log_u(logmsg::error, L"Failed connection for data socket. Reason: %s.", socket_error_description(error));
			handle_data_transfer(controller::data_transfer_handler::connecting, {error, channel::error_source::socket});
		}

		return;
	}

	if (type == socket_event_flag::connection) {
		/** Control connection handling **/
		if (source->root() == control_socket_.root()) {
			// This may only happen if TLS was requested.

			// All fine, hand the socket down to the commander.
			commander_.set_socket(&control_socket_);
			notifier_->notify_protocol_info(get_protocol_info());
			return;
		}

		if (data_listen_socket_) {
			auto socket = data_listen_socket_->accept(error);
			if (error) {
				logger_.log_u(logmsg::error, L"Failed data_listen_socket_->accept(). Reason: %s.", socket_error_description(error));
				handle_data_transfer(controller::data_transfer_handler::connecting, {error, channel::error_source::socket});
				return;
			}
			else
			if (socket->peer_ip() != control_socket_.peer_ip()) {
				logger_.log_u(logmsg::error, L"Data peer IP [%s] differs from control peer IP [%s]: this shouldn't happen, aborting the data connection.", socket->peer_ip(), control_socket_.peer_ip());
				handle_data_transfer(controller::data_transfer_handler::connecting, {EINVAL, channel::error_source::socket});
				return;
			}

			data_port_lease_.set_connected();
			data_socket_ = std::make_unique<securable_socket>(event_loop_, static_cast<event_handler*>(this), std::move(socket), logger_);
			do_set_buffer_sizes();
			data_limiter_ = &data_socket_->emplace<compound_rate_limited_layer>(static_cast<event_handler*>(this), data_socket_->top());
		}

		if (data_socket_) {
			data_listen_socket_.reset();
			setup_data_channel();
		}
	}
	else
	if (data_socket_ && source->root() == data_socket_->root()) {
		if (data_shutting_down_) {
			if (type == socket_event_flag::write)
				data_socket_shutdown(data_socket_shutdown_error_);

			// Else:
			// "Spurious" reads we can safely ignore. See #137
		}
		else
		if (!data_adder_ && !data_consumer_) {
			// We can safely ignore this event, for it's the physiological effect of having not added the data_socket_ to the data_channel_ yet
			// Due to the fact that neither data_adder_ nor data_consumer_ have been specified yet
		}
		else {
			is_unexpected_event = true;
		}
	}
	else {
		is_unexpected_event = true;
	}

	if (is_unexpected_event) {
		logger_.log_u(logmsg::error,
					L"We got an unexpected socket_event. is_control [%d], flag [%d], data_socket [%s], state [%d], error [%d]",
					source->root() == control_socket_.root(), type, data_socket_ ? "valid" : "invalid", data_socket_ ? (int)data_socket_->get_state() : -1, error);

		assert(false && "We got an unexpected socket_event");
	}
}

void session::on_hostname_lookup_event(hostname_lookup *, int error, const std::vector<std::string> &ips)
{
	assert(data_local_info_handler_ != nullptr && data_listen_socket_ != nullptr);

	auto &handler = *data_local_info_handler_;
	data_local_info_handler_ = nullptr;

	if (error) {
		logger_.log(logmsg::error, L"Host '%s' lookup failed. Reason: %s.", opts_.pasv.host_override, socket_error_description(error));
		return handler.handle_data_local_info(std::nullopt);
	}

	if (ips.empty()) {
		logger_.log(logmsg::error, L"Host '%s' lookup returned no IPs.", opts_.pasv.host_override);
		return handler.handle_data_local_info(std::nullopt);
	}

	int port = data_listen_socket_->local_port(error);
	if (port < 0) {
		logger_.log_u(logmsg::error, "data_listen_socket_->local_port() failed. Reason: %s.", socket_error_description(error));
		return handler.handle_data_local_info(std::nullopt);
	}

	handler.handle_data_local_info(std::pair{ips[0], std::uint16_t(port)});
}

void session::on_channel_done_event(channel &ch [[maybe_unused]], channel::error_type error)
{
	FZ_UTIL_THREAD_CHECK

	assert(ch == data_channel_);

	if (data_socket_)
		data_socket_shutdown(error);
	else
		logger_.log_u(logmsg::debug_verbose, L"[on_channel_done_event] The socket has already been destroyed, but the done_event should have been removed. This is likely a bug.");

	if (logger_.should_log(logmsg::debug_debug)) {
		fz::logger::modularized l(logger_, "Done Event");
		data_channel_.dump_state(l);
	}
}

void session::on_authenticator_operation_result(authentication::authenticator &, std::unique_ptr<authentication::authenticator::operation> &op)
{
	FZ_UTIL_THREAD_CHECK

	user_ = authenticate_user_response_handler_->handle_authenticate_user_response(std::move(op));

	if (user_) {
		logger_.insert_meta("user", user_->lock()->name);

		subscribe(user_, *this);
		on_shared_user_changed_event(user_);

		current_number_of_failed_login_attempts_ = 0;
	}
	else
	if (user_.error().is_user_fault()) {
		autobanner_.set_failed_login(control_socket_.peer_ip(), control_socket_.address_family());
	}

}

void session::operator()(const event_base &ev)
{
	FZ_UTIL_THREAD_CHECK

	dispatch<
		socket_event,
		channel::done_event,
		authentication::authenticator::operation::result_event,
		hostname_lookup_event,
		authentication::shared_user_changed_event,
		timer_event
	>(ev, this,
		&session::on_socket_event,
		&session::on_channel_done_event,
		&session::on_authenticator_operation_result,
		&session::on_hostname_lookup_event,
		&session::on_shared_user_changed_event,
		&session::on_timer_event
	);
}

void session::update_limits(compound_rate_limited_layer *crll, std::vector<std::shared_ptr<rate_limiter>> *extra)
{
	FZ_UTIL_THREAD_CHECK

	if (!crll)
		return;

	if (extra) {
		auto our_it = extra_limiters_.begin();
		auto their_it = extra->begin();

		while (our_it != extra_limiters_.end() && their_it != extra->end()) {
			if (*our_it < *their_it) {
				crll->remove_limiter(our_it->get());
				++our_it;
			}
			else
			if (*their_it < *our_it) {
				crll->add_limiter(their_it->get());
				++their_it;
			}
			else {
				++our_it;
				++their_it;
			}
		}

		for (;our_it != extra_limiters_.end(); ++our_it)
			crll->remove_limiter(our_it->get());

		for (;their_it != extra->end(); ++their_it)
			crll->add_limiter(their_it->get());
	}
	else
	for (auto &rl: extra_limiters_)
		crll->add_limiter(rl.get());

	crll->add_limiter(user_limiter_.get());
	crll->add_limiter(&session_limiter_);
}

bool session::must_downgrade_log_level()
{
	return
		!commander_.is_executing_command() &&
		commander_.has_empty_buffers() &&
		is_authenticated() &&
		data_listen_socket_.get() == nullptr &&
		data_socket_.get() == nullptr;
}

}
