#ifndef FZ_TCP_PROXY_LAYER_HPP
#define FZ_TCP_PROXY_LAYER_HPP

#include <libfilezilla/buffer.hpp>
#include <libfilezilla/socket.hpp>
#include <libfilezilla/logger.hpp>

namespace fz::tcp {

enum class proxy_type {
	NONE,
	HTTP,
	SOCKS5,
	SOCKS4
};

class proxy_layer final: private fz::event_handler, public fz::socket_layer
{
public:
	proxy_layer(fz::event_loop &loop, fz::event_handler* pEvtHandler, fz::socket_interface & next_layer, proxy_type t, fz::native_string const& proxy_host, unsigned int proxy_port, std::wstring const& user, std::wstring const& pass, logger_interface &logger);
	virtual ~proxy_layer() override;

	virtual int connect(fz::native_string const& host, unsigned int port, fz::address_type family = fz::address_type::unknown) override;

	fz::socket_state get_state() const override { return state_; }

	virtual int read(void *buffer, unsigned int size, int& error) override;
	virtual int write(void const* buffer, unsigned int size, int& error) override;

	proxy_type get_proxy_type() const { return type_; }
	std::wstring get_user() const;
	std::wstring get_pass() const;

	virtual fz::native_string peer_host() const override;
	virtual int peer_port(int& error)  const override;

	virtual int shutdown() override;

protected:
	proxy_type type_{};
	fz::native_string proxy_host_;
	unsigned int proxy_port_{};
	std::string user_;
	std::string pass_;
	logger_interface &logger_;

	fz::native_string host_;
	unsigned int port_{};
	fz::address_type family_{};

	fz::socket_state state_{};

	int m_handshakeState{};

	fz::buffer sendBuffer_;
	fz::buffer receiveBuffer_;

	virtual void operator()(fz::event_base const& ev) override;
	void on_socket_event(socket_event_source* source, fz::socket_event_flag t, int error);

	void on_read();
	void on_write();

	bool can_write_{};
	bool can_read_{};
};

std::wstring to_wstring(proxy_type t);

}

#endif // FZ_TCP_PROXY_LAYER_HPP
