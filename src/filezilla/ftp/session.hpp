#ifndef FZ_FTP_SESSION_HPP
#define FZ_FTP_SESSION_HPP

#include <libfilezilla/logger.hpp>
#include <libfilezilla/time.hpp>

#include "../channel.hpp"
#include "../securable_socket.hpp"
#include "../authentication/authenticator.hpp"
#include "../tcp/session.hpp"
#include "../util/invoke_later.hpp"
#include "../port_randomizer.hpp"
#include "../logger/modularized.hpp"

#include "controller.hpp"
#include "commander.hpp"
#include "../authentication/autobanner.hpp"

namespace fz {

class compound_rate_limited_layer;
class hostname_lookup;

}

namespace fz::ftp {

class server;
class ascii_layer;

class session final
	: public tcp::session
	, private event_handler
	, private controller
	, private channel::progress_notifier
{
	FZ_UTIL_THREAD_CHECK_INIT

public:
	struct options {
		struct pasv {
			struct port_range {
				std::uint16_t  min = port_randomizer::min_ephemeral_value;
				std::uint16_t  max = port_randomizer::max_ephemeral_value;

				constexpr port_range(std::uint16_t min, std::uint16_t max) noexcept
					: min(min < max ? min : max)
					, max(min < max ? max : min)
				{}

				constexpr port_range() noexcept {}
			};

			fz::native_string host_override;
			bool do_not_override_host_if_peer_is_local{true};
			std::optional<port_range> port_range;
		};

		pasv                   pasv = {};
		securable_socket::info tls  = {};

		options(){}
	};

	struct protocol_info: notifier::protocol_info
	{
		using security_t = std::optional<securable_socket::session_info>;

		security_t security;

	public:
		status get_status() const override;
		std::string get_name() const override;

	public:
		protocol_info() = default;
		protocol_info(security_t security)
			: security(std::move(security))
		{}

		protocol_info(const protocol_info &) = default;
		protocol_info(protocol_info &&) = default;
		protocol_info &operator=(const protocol_info &) = default;
		protocol_info &operator=(protocol_info &&) = default;
	};

	enum tls_mode: uint8_t {
		allow_tls,
		implicit_tls,
		require_tls
	};

	session(fz::thread_pool &pool, event_loop &loop, event_handler &target_event_handler,
			rate_limit_manager &rate_limit_manager,
			std::unique_ptr<notifier> notifier,
			id id,
			datetime start,
			std::unique_ptr<socket> control_socket,
			tls_mode tls_mode,
			authentication::autobanner &autobanner,
			authentication::authenticator &authenticator,
			port_manager &port_manager,
			const commander::welcome_message &welcome_message,
			const std::string &refuse_message,
			options opts = {});

	~session() override;

	notifier *get_notifier();

	bool is_alive() const override;

	void shutdown(int err = 0) override;

	/// Updates the options, while live
	void set_options(options opts);
	void set_data_buffer_sizes(std::int32_t receive, std::int32_t send);
	void set_timeouts(const duration &login_timeout, const duration &activity_timeout);

private:
	protocol_info get_protocol_info() const;
	void update_limits(compound_rate_limited_layer *crll, std::vector<std::shared_ptr<rate_limiter> > *extra = {});
	void do_set_buffer_sizes();

	rate_limiter session_limiter_;
	std::shared_ptr<rate_limiter> user_limiter_{};
	std::vector<std::shared_ptr<rate_limiter>> extra_limiters_{};
	compound_rate_limited_layer *control_limiter_{};
	compound_rate_limited_layer *data_limiter_{};

private:
	thread_pool &pool_;
	std::unique_ptr<notifier> notifier_;

	logger::modularized logger_;

	datetime start_datetime_;
	securable_socket control_socket_;
	port_manager &port_manager_;

	options opts_;
	std::int32_t receive_buffer_size_ = -1;
	std::int32_t send_buffer_size_    = -1;

	monotonic_clock last_activity_;

	tvfs::engine tvfs_;
	authentication::autobanner &autobanner_;
	monotonic_clock time_of_first_failed_login_attempt_;
	std::size_t current_number_of_failed_login_attempts_{};

	// controller interface
public:
	fz::address_type get_control_socket_address_family() const override;

	void authenticate_user(std::string_view user, const authentication::methods_list &method, authenticate_user_response_handler *response_handler) override;
	void stop_ongoing_user_authentication() override;
	bool is_authenticated() const override;
	void make_secure(std::string_view preamble, controller::make_secure_response_handler *response_handler) override;
	secure_state get_secure_state() const override;
	std::string get_alpn() const override;
	void quit(int err = 0) override;
	data_connection_status close_data_connection() override;
	void get_data_local_info(address_type family, bool do_resolve_hostname, data_local_info_handler &handler) override;
	bool set_data_peer_hostaddress(hostaddress) override;
	bool is_data_connection_setup() override;
	set_mode_result set_data_mode(data_mode mode) override;
	set_mode_result set_data_protection_mode(data_protection_mode mode) override;
	void start_data_transfer(buffer_operator::adder_interface &adder, data_transfer_handler *handler, bool is_binary) override;
	void start_data_transfer(buffer_operator::consumer_interface &consumer, data_transfer_handler *handler, bool is_binary) override;

private:
	authentication::authenticator &authenticator_;
	controller::authenticate_user_response_handler *authenticate_user_response_handler_{};

	authentication::session_user user_;

	std::unique_ptr<listen_socket> data_listen_socket_{};
	std::unique_ptr<securable_socket> data_socket_{};
	bool data_shutting_down_{};
	bool data_is_binary_{};
	port_lease data_port_lease_{};
	int64_t data_previous_read_amount_{};
	int64_t data_previous_written_amount_{};

	std::unique_ptr<hostname_lookup> hostname_lookup_{};
	controller::data_local_info_handler *data_local_info_handler_{};

	hostaddress data_peer_hostaddress_{};
	data_protection_mode data_protection_mode_{data_protection_mode::C};

private:
	bool must_downgrade_log_level() override;

private:
	void notify_channel_socket_read_amount(const monotonic_clock &time_point, std::int64_t) override;
	void notify_channel_socket_written_amount(const monotonic_clock &time_point, std::int64_t) override;

private:
	void operator()(const event_base &ev) override;

	void on_socket_event(fz::socket_event_source *, fz::socket_event_flag type, int error);
	void on_make_secure_event(controller::make_secure_response_handler *response_handler);
	void on_channel_done_event(channel &, channel::error_type error);
	void on_authenticator_operation_result(authentication::authenticator &, std::unique_ptr<authentication::authenticator::operation> &op);
	void on_hostname_lookup_event(hostname_lookup*, int, const std::vector<std::string> &ips);
	void on_shared_user_changed_event(const authentication::weak_user &su);
	void on_timer_event(timer_id id);

private:
	bool setup_data_channel();
	void data_socket_shutdown(channel::error_type error);
	bool handle_data_transfer(data_transfer_handler::status, channel::error_type error, std::string_view msg = {});

	channel::error_type data_socket_shutdown_error_{};
	buffer_operator::adder_interface *data_adder_{};
	buffer_operator::consumer_interface *data_consumer_{};

	data_transfer_handler *data_transfer_handler_{};

	controller::data_transfer_handler::status previous_data_transfer_status_{};
	channel::error_type previous_data_transfer_error_{};
	std::string_view previous_data_transfer_msg_{};
	bool previous_data_transfer_is_consuming_{};

	timer_id check_if_control_is_secured_id_{};

private:
	static std::string logger_info_to_string(const logger::modularized::info &i, const logger::modularized::info_list &parent_info_list);

private:
	commander::buffer_operators commander_buffer_operators_;
	channel data_channel_{*this, 128*1024, 10, false, *this};
	commander commander_;
	util::invoker_handler invoke_later_;
};

}

#endif
