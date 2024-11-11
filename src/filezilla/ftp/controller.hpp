#ifndef CONTROLLER_HPP
#define CONTROLLER_HPP

#include <string_view>
#include <functional>

#include <libfilezilla/iputils.hpp>
#include <libfilezilla/local_filesys.hpp>

#include "../hostaddress.hpp"
#include "../authentication/authenticator.hpp"

#include "../buffer_operator/adder.hpp"
#include "../buffer_operator/consumer.hpp"
#include "../channel.hpp"

namespace fz::ftp {

class controller {
public:
	virtual ~controller() = default;

	virtual fz::address_type get_control_socket_address_family() const = 0;

	class authenticate_user_response_handler {
	public:
		virtual ~authenticate_user_response_handler() = default;
		virtual authentication::session_user handle_authenticate_user_response(std::unique_ptr<authentication::authenticator::operation> &&op) = 0;
	};
	virtual void authenticate_user(std::string_view user, const authentication::methods_list &methods, authenticate_user_response_handler *response_handler) = 0;

	virtual void stop_ongoing_user_authentication() = 0;
	virtual bool is_authenticated() const = 0;

	class make_secure_response_handler {
	public:
		virtual ~make_secure_response_handler() = default;
		virtual void handle_make_secure_response(bool secured) = 0;
	};

	enum class secure_state {
		insecure,
		securing,
		secured
	};

	virtual void make_secure(std::string_view preamble, make_secure_response_handler *response_handler) = 0;
	virtual secure_state get_secure_state() const = 0;
	virtual std::string get_alpn() const = 0; // Returns the negotiated ALPN if the connection is secured. Otherwise returns empty string.
	bool is_secure() const { return get_secure_state() == secure_state::secured; }
	bool is_securing() const { return get_secure_state() == secure_state::securing; }

	virtual void quit(int err = 0) = 0;

	class data_local_info_handler {
	public:
		virtual ~data_local_info_handler() = default;
		virtual void handle_data_local_info(const std::optional<std::pair<std::string, uint16_t>> &info) = 0;
	};

	virtual void get_data_local_info(address_type, bool do_get_ip, data_local_info_handler &handler) = 0;
	virtual bool set_data_peer_hostaddress(hostaddress) = 0;
	virtual bool is_data_connection_setup() = 0;

	enum class data_mode: char { unknown, S = 'S', Z = 'Z', B = 'B', C = 'C' };
	enum class data_protection_mode: char { unknown, C = 'C', S = 'S', E = 'E', P = 'P' };
	enum class set_mode_result { enabled, not_enabled, not_implemented, not_allowed, unknown };
	virtual set_mode_result set_data_mode(data_mode mode) = 0;
	virtual set_mode_result set_data_protection_mode(data_protection_mode mode) = 0;

	class data_transfer_handler {
	public:
		enum status { connecting, started, stopped };
		virtual ~data_transfer_handler() = default;
		virtual void handle_data_transfer(status st, channel::error_type error, std::string_view msg) = 0;
	};

	virtual void start_data_transfer(buffer_operator::adder_interface &adder, data_transfer_handler *handler, bool is_binary) = 0;
	virtual void start_data_transfer(buffer_operator::consumer_interface &consumer, data_transfer_handler *handler, bool is_binary) = 0;

	enum class data_connection_status
	{
		not_started,
		waiting,
		started
	};

	/// \Brief closes the ongoing data connection, if any.
	/// \Returns the status of the connection before the call
	virtual data_connection_status close_data_connection() = 0;
	virtual bool must_downgrade_log_level() = 0;
};

}
#endif // CONTROL_HPP
