// ----------------------------
// Ported from FileZilla client
// ----------------------------

#include <algorithm>

#include <assert.h>
#include <string.h>

#include <libfilezilla/iputils.hpp>
#include <libfilezilla/translate.hpp>

#include "../build_info.hpp"
#include "proxy_layer.hpp"

#define _(x) ::fz::translate(x)

namespace fz::tcp {

enum handshake_state
{
	http_wait,

	socks5_method,
	socks5_auth,
	socks5_request,

	socks4_handshake
};

proxy_layer::proxy_layer(fz::event_loop &loop, fz::event_handler* pEvtHandler, fz::socket_interface & next_layer, proxy_type t, fz::native_string const& proxy_host, unsigned int proxy_port, std::wstring const& user, std::wstring const& pass, logger_interface &logger)
	: fz::event_handler(loop)
	, fz::socket_layer(pEvtHandler, next_layer, false)
	, type_(t)
	, proxy_host_(proxy_host)
	, proxy_port_(proxy_port)
	, user_(fz::to_utf8(user))
	, pass_(fz::to_utf8(pass))
	, logger_(logger)
{
	next_layer_.set_event_handler(this);
}

proxy_layer::~proxy_layer()
{
	remove_handler();
	next_layer_.set_event_handler(nullptr);
}

int proxy_layer::connect(fz::native_string const& host, unsigned int port, fz::address_type family)
{
	if (state_ != fz::socket_state::none) {
		if (state_ == fz::socket_state::failed) {
			return EINVAL;
		}
		else {
			return EALREADY;
		}
	}

	if (next_layer_.get_state() != fz::socket_state::none && next_layer_.get_state() != fz::socket_state::connecting) {
		state_ = fz::socket_state::failed;
		return EINVAL;
	}

	host_ = host;
	port_ = port;
	family_ = family;

	if (type_ == proxy_type::NONE || proxy_host_.empty() || proxy_port_ < 1 || proxy_port_ > 65535) {
		state_ = fz::socket_state::failed;
		return EINVAL;
	}

	if (host.empty() || port < 1 || port > 65535) {
		state_ = fz::socket_state::failed;
		return EINVAL;
	}

	if (state_ != fz::socket_state::none) {
		return EALREADY;
	}

	if (type_ != proxy_type::HTTP && type_ != proxy_type::SOCKS5 && type_ != proxy_type::SOCKS4) {
		state_ = fz::socket_state::failed;
		return EPROTONOSUPPORT;
	}

	state_ = fz::socket_state::connecting;

	if (type_ == proxy_type::HTTP) {
		m_handshakeState = http_wait;

		std::string auth;
		if (!user_.empty()) {
			auth = "Proxy-Authorization: Basic ";
			auth += fz::base64_encode(user_ + ":" + pass_);
			auth += "\r\n";
		}

		// Bit oversized, but be on the safe side
		std::string host_raw = fz::to_utf8(host);
		sendBuffer_.append(fz::sprintf("CONNECT %s:%u HTTP/1.1\r\nHost: %s:%u\r\n%sUser-Agent: %s\r\n\r\n",
			host_raw, port,
			host_raw, port,
			auth,
			fz::replaced_substrings(build_info::package_name, " ", "-").append(1, '/').append(build_info::version)));
	}
	else if (type_ == proxy_type::SOCKS4) {
		std::string ip;
		auto const addressType = fz::get_address_type(host_);
		if (addressType == fz::address_type::ipv6) {
			logger_.log_u(logmsg::error, _("IPv6 addresses are not supported with SOCKS4 proxy"));
			return EINVAL;
		}
		else if (addressType == fz::address_type::ipv4) {
			ip = fz::to_string(host_);
		}
		else {
			logger_.log_u(logmsg::error, L"Cannot use hostnames for use with SOCKS4 proxy.");
			return EINVAL;
		}

		logger_.log_u(logmsg::status, _("SOCKS4 proxy will connect to: %s"), ip);

		unsigned char* out = sendBuffer_.get(9);
		out[0] = 4; // Protocol version
		out[1] = 1; // Stream mode
		out[2] = (port_ >> 8) & 0xFF; // Port in network order
		out[3] = port_ & 0xFF;
		int i = 0;
		memset(out + 4, 0, 5);
		for (auto p = ip.c_str(); *p && i < 4; ++p) {
			auto const& c = *p;
			if (c == '.') {
				++i;
				continue;
			}
			out[i + 4] *= 10;
			out[i + 4] += c - '0';
		}
		sendBuffer_.add(9);

		m_handshakeState = socks4_handshake;
	}
	else {
		if (user_.size() > 255 || pass_.size() > 255) {
			logger_.log_u(logmsg::status, _("SOCKS5 does not support usernames or passwords longer than 255 characters."));
			return EINVAL;
		}

		unsigned char* out = sendBuffer_.get(4);
		out[0] = 5; // Protocol version
		if (!user_.empty()) {
			out[1] = 2; // # auth methods supported
			out[2] = 0; // Method: No auth
			out[3] = 2; // Method: Username and password
			sendBuffer_.add(4);
		}
		else {
			out[1] = 1; // # auth methods supported
			out[2] = 0; // Method: No auth
			sendBuffer_.add(3);
		}

		m_handshakeState = socks5_method;
	}

	if (next_layer_.get_state() == fz::socket_state::none) {
		int ret = next_layer_.connect(proxy_host_, proxy_port_);

		if (ret) {
			state_ = fz::socket_state::failed;
			return ret;
		}
	}
	else {
		if (can_write_) {
			on_write();
		}
	}
	return 0;
}

void proxy_layer::operator()(fz::event_base const& ev)
{
	fz::dispatch<fz::socket_event, fz::hostaddress_event>(ev, this,
		&proxy_layer::on_socket_event,
		&proxy_layer::forward_hostaddress_event);
}

void proxy_layer::on_socket_event(socket_event_source* s, fz::socket_event_flag t, int error)
{
	if (state_ != fz::socket_state::connecting) {
		return;
	}

	if (t == fz::socket_event_flag::connection_next) {
		forward_socket_event(s, t, error);
		return;
	}

	if (error) {
		state_ = fz::socket_state::failed;
		forward_socket_event(s, t, error);
		return;
	}

	switch (t) {
		case fz::socket_event_flag::connection:
			logger_.log_u(logmsg::status, _("Connection with proxy established, performing handshake..."));
			on_write();
			break;

		case fz::socket_event_flag::read:
			on_read();
			break;

		case fz::socket_event_flag::write:
			on_write();
			break;

		case fz::socket_event_flag::connection_next:
			break;
		}
}

void proxy_layer::on_read()
{
	can_read_ = true;

	if (state_ != fz::socket_state::connecting) {
		return;
	}

	while (can_read_) {
		loop:
		int to_read = 1024;
		unsigned char* buf = receiveBuffer_.get(size_t(to_read));

		int error;
		int read = next_layer_.read(buf, to_read, error);

		if (read < 0) {
			if (error != EAGAIN) {
				state_ = fz::socket_state::failed;
				if (event_handler_) {
					event_handler_->send_event<fz::socket_event>(this, fz::socket_event_flag::connection, error);
				}
			}
			else {
				can_read_ = false;
			}
			return;
		}
		if (!read) {
			state_ = fz::socket_state::failed;
			if (event_handler_) {
				event_handler_->send_event<fz::socket_event>(this, fz::socket_event_flag::connection, ECONNABORTED);
			}
			return;
		}
		receiveBuffer_.add(size_t(read));

		switch (m_handshakeState) {
		case http_wait:
			{
				// Look for \r\n\r\n
				buf = receiveBuffer_.get();
				size_t i = 0;
				for (i = 0; i + 4 <= receiveBuffer_.size(); ++i) {
					if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n') {
						break;
					}
				}
				if (i + 4 > receiveBuffer_.size()) {
					// Not found yet
					if (receiveBuffer_.size() >= 2048) {
						state_ = fz::socket_state::failed;
						logger_.log_u(logmsg::debug_warning, L"Incoming header too large");
						if (event_handler_) {
							event_handler_->send_event<fz::socket_event>(this, fz::socket_event_flag::connection, ENOMEM);
						}
						return;
					}
					break;
				}

				// Found end of header
				unsigned char* eol = reinterpret_cast<unsigned char*>(strchr(reinterpret_cast<char*>(buf), '\r')); // Never fails as old buf ends on CRLFCRLF
				*eol = 0;
				std::wstring const reply = fz::to_wstring_from_utf8(std::string(reinterpret_cast<char*>(buf))); // Terminate at first emedded null
				logger_.log_u(logmsg::reply, _("Proxy reply: %s"), reply);

				if (reply.substr(0, 10) != L"HTTP/1.1 2" && reply.substr(0, 10) != L"HTTP/1.0 2") {
					state_ = fz::socket_state::failed;
					if (event_handler_) {
						event_handler_->send_event<fz::socket_event>(this, fz::socket_event_flag::connection, ECONNRESET);
					}
				}
				else {
					state_ = fz::socket_state::connected;
					if (event_handler_) {
						event_handler_->send_event<fz::socket_event>(this, fz::socket_event_flag::connection, 0);
					}
					receiveBuffer_.consume(i + 4);
					set_event_passthrough();
				}
				return;
			}
		case socks4_handshake:
			{
				if (receiveBuffer_.size() < 8) {
					break;
				}

				unsigned char const* const buf = receiveBuffer_.get();
				if (buf[1] != 0x5A) {
					std::wstring error;
					switch (buf[1]) {
						case 0x5B:
							error = _("Request rejected or failed");
							break;
						case 0x5C:
							error = _("Request failed - client is not running identd (or not reachable from server)");
							break;
						case 0x5D:
							error = _("Request failed - client's identd could not confirm the user ID string");
							break;
						default:
							error = fz::sprintf(_("Unassigned error code %d"), (int)buf[1]);
							break;
					}
					logger_.log_u(logmsg::error, _("Proxy request failed: %s"), error);
					state_ = fz::socket_state::failed;
					if (event_handler_) {
						event_handler_->send_event<fz::socket_event>(this, fz::socket_event_flag::connection, ECONNABORTED);
					}
				}
				else {
					state_ = fz::socket_state::connected;
					if (event_handler_) {
						event_handler_->send_event<fz::socket_event>(this, fz::socket_event_flag::connection, 0);
					}
					receiveBuffer_.consume(8);
					set_event_passthrough();
				}
				return;
			}
		case socks5_method:
		case socks5_auth:
		case socks5_request:
			if (sendBuffer_) {
				logger_.log_u(logmsg::error, _("Proxy sent data while we haven't sent out request yet"));
				state_ = fz::socket_state::failed;
				if (event_handler_) {
					event_handler_->send_event<fz::socket_event>(this, fz::socket_event_flag::connection, ECONNABORTED);
				}
				return;
			}

			// All data got read, parse it
			switch (m_handshakeState) {
			default:
				if (receiveBuffer_[0] != 5) {
					logger_.log_u(logmsg::error, _("Unknown SOCKS protocol version: %d"), (int)receiveBuffer_[0]);
					state_ = fz::socket_state::failed;
					if (event_handler_) {
						event_handler_->send_event<fz::socket_event>(this, fz::socket_event_flag::connection, ECONNABORTED);
					}
					return;
				}
				break;
			case socks5_auth:
				if (receiveBuffer_[0] != 1) {
					logger_.log_u(logmsg::error, _("Unknown protocol version of SOCKS Username/Password Authentication subnegotiation: %d"), receiveBuffer_[0]);
					state_ = fz::socket_state::failed;
					if (event_handler_) {
						event_handler_->send_event<fz::socket_event>(this, fz::socket_event_flag::connection, ECONNABORTED);
					}
					return;
				}
				break;
			}

			switch (m_handshakeState) {
				case socks5_method:
				{
					if (receiveBuffer_.size() < 2) {
						goto loop;
					}
					char const method = char(receiveBuffer_[1]);
					switch (method)
					{
					case 0:
						m_handshakeState = socks5_request;
						break;
					case 2:
						m_handshakeState = socks5_auth;
						break;
					default:
						logger_.log_u(logmsg::error, _("No supported SOCKS5 auth method"));
						state_ = fz::socket_state::failed;
						if (event_handler_) {
							event_handler_->send_event<fz::socket_event>(this, fz::socket_event_flag::connection, ECONNABORTED);
						}
						return;
					}
					receiveBuffer_.consume(2);
				}
				break;
			case socks5_auth:
				if (receiveBuffer_.size() < 2) {
					goto loop;
				}
				if (receiveBuffer_[1] != 0) {
					logger_.log_u(logmsg::error, _("Proxy authentication failed"));
					state_ = fz::socket_state::failed;
					if (event_handler_) {
						event_handler_->send_event<fz::socket_event>(this, fz::socket_event_flag::connection, ECONNABORTED);
					}
					return;
				}
				m_handshakeState = socks5_request;
				receiveBuffer_.consume(2);
				break;
			case socks5_request:
				if (receiveBuffer_.size() < 2) {
					goto loop;
				}
				if (receiveBuffer_[1]) {
					std::wstring errorMsg;
					switch (receiveBuffer_[1])
					{
					case 1:
						errorMsg = _("General SOCKS server failure");
						break;
					case 2:
						errorMsg = _("Connection not allowed by ruleset");
						break;
					case 3:
						errorMsg = _("Network unreachable");
						break;
					case 4:
						errorMsg = _("Host unreachable");
						break;
					case 5:
						errorMsg = _("Connection refused");
						break;
					case 6:
						errorMsg = _("TTL expired");
						break;
					case 7:
						errorMsg = _("Command not supported");
						break;
					case 8:
						errorMsg = _("Address type not supported");
						break;
					default:
						errorMsg = fz::sprintf(_("Unassigned error code %d"), receiveBuffer_[1]);
						break;
					}

					logger_.log_u(logmsg::error, _("Proxy request failed. Reply from proxy: %s"), errorMsg);
					state_ = fz::socket_state::failed;
					if (event_handler_) {
						event_handler_->send_event<fz::socket_event>(this, fz::socket_event_flag::connection, ECONNABORTED);
					}
					return;
				}

				// We need to parse the returned address type to determine the length of the address that follows.
				// Unfortunately the information in the type and address is useless, many proxies just return
				// syntactically valid bogus values
				if (receiveBuffer_.size() < 4) {
					goto loop;
				}
				switch (receiveBuffer_[3])
				{
				case 1:
					// syntactically valid bogus values
					if (receiveBuffer_.size() < 10) {
						goto loop;
					}
					receiveBuffer_.consume(10);
					break;
				case 3:
					if (receiveBuffer_.size() < 5) {
						goto loop;
					}
					if (receiveBuffer_.size() < receiveBuffer_[4] + 7u) {
						goto loop;
					}
					receiveBuffer_.consume(receiveBuffer_[4] + 7u);
					break;
				case 4:
					if (receiveBuffer_.size() < 22) {
						goto loop;
					}
					receiveBuffer_.consume(22);
					break;
				default:
					logger_.log_u(logmsg::error, _("Proxy request failed: Unknown address type in CONNECT reply"));
					state_ = fz::socket_state::failed;
					if (event_handler_) {
						event_handler_->send_event<fz::socket_event>(this, fz::socket_event_flag::connection, ECONNABORTED);
					}
					return;
				}

				// We're done
				state_ = fz::socket_state::connected;
				if (event_handler_) {
					event_handler_->send_event<fz::socket_event>(this, fz::socket_event_flag::connection, 0);
				}
				set_event_passthrough();
				return;
			default:
				assert(false);
				break;
			}

			switch (m_handshakeState)
			{
			case socks5_auth:
				{
					auto ulen = static_cast<unsigned char>(std::min(user_.size(), size_t(255)));
					auto plen = static_cast<unsigned char>(std::min(pass_.size(), size_t(255)));
					unsigned char* out = sendBuffer_.get(ulen + plen + 3);
					out[0] = 1;
					out[1] = ulen;
					memcpy(out + 2, user_.c_str(), ulen);
					out[ulen + 2] = plen;
					memcpy(out + ulen + 3, pass_.c_str(), plen);
					sendBuffer_.add(ulen + plen + 3);
				}
				break;
			case socks5_request:
				{
					std::string host = fz::to_utf8(host_);
					size_t addrlen = std::max(host.size(), size_t(16));

					unsigned char * out = sendBuffer_.get(7 + addrlen);
					out[0] = 5;
					out[1] = 1; // CONNECT
					out[2] = 0; // Reserved

					// FIXME: use parse_ip() instead.
					auto const type = fz::get_address_type(host);
					if (type == fz::address_type::ipv6) {
						auto ipv6 = fz::get_ipv6_long_form(host);
						addrlen = 16;
						for (size_t i = 0; i < 16; ++i) {
							auto nibble_left = unsigned(fz::hex_char_to_int(ipv6[i * 2 + i / 2]));
							auto nibble_right = unsigned(fz::hex_char_to_int(ipv6[i * 2 + 1 + i / 2]));

							out[4 + i] = (unsigned char)((nibble_left << 4) + nibble_right);
						}

						out[3] = 4; // IPv6
					}
					else if (type == fz::address_type::ipv4) {
						int i = 0;
						memset(out + 4, 0, 4);
						for (auto p = host.c_str(); *p && i < 4; ++p) {
							auto const& c = *p;
							if (c == '.') {
								++i;
								continue;
							}
							out[i + 4] *= 10;
							out[i + 4] += c - '0';
						}

						addrlen = 4;

						out[3] = 1; // IPv4
					}
					else {
						out[3] = 3; // Domain name

						auto hlen = static_cast<unsigned char>(std::min(host.size(), size_t(255)));
						out[4] = hlen;
						memcpy(out + 5, host.c_str(), hlen);
						addrlen = hlen + 1;
					}

					out[addrlen + 4] = (port_ >> 8) & 0xFF; // Port in network order
					out[addrlen + 5] = port_ & 0xFF;

					sendBuffer_.add(6 + addrlen);
				}
				break;
			default:
				assert(false);
				break;
			}
			if (sendBuffer_ && can_write_) {
				on_write();
			}
			break;
		default:
			state_ = fz::socket_state::failed;
			logger_.log_u(logmsg::debug_warning, L"Unhandled handshake state %d", m_handshakeState);
			if (event_handler_) {
				event_handler_->send_event<fz::socket_event>(this, fz::socket_event_flag::connection, ECONNABORTED);
			}
			return;
		}
	}
}

void proxy_layer::on_write()
{
	can_write_ = true;
	if (state_ != fz::socket_state::connecting || !sendBuffer_) {
		return;
	}

	for (;;) {
		int error;
		int written = next_layer_.write(sendBuffer_.get(), sendBuffer_.size(), error);
		if (written == -1) {
			if (error != EAGAIN) {
				state_ = fz::socket_state::failed;
				if (event_handler_) {
					event_handler_->send_event<fz::socket_event>(this, fz::socket_event_flag::connection, error);
				}
			}
			else {
				can_write_ = false;
			}

			return;
		}

		sendBuffer_.consume(size_t(written));
		if (sendBuffer_.empty()) {
			if (can_read_) {
				on_write();
			}
			return;
		}
	}
}

int proxy_layer::read(void * buffer, unsigned int size, int& error)
{
	if (receiveBuffer_) {
		if (size > receiveBuffer_.size()) {
			size = unsigned(receiveBuffer_.size());
		}

		memcpy(buffer, receiveBuffer_.get(), size);
		receiveBuffer_.consume(size);
		return signed(size);
	}

	return next_layer_.read(buffer, size, error);
}

int proxy_layer::write(void const* buffer, unsigned int size, int& error)
{
	return next_layer_.write(buffer, size, error);
}

std::wstring proxy_layer::get_user() const
{
	return fz::to_wstring_from_utf8(user_);
}

std::wstring proxy_layer::get_pass() const
{
	return fz::to_wstring_from_utf8(pass_);
}

fz::native_string proxy_layer::peer_host() const
{
	return host_;
}

int proxy_layer::peer_port(int& error) const
{
	if (!port_) {
		error = ENOTCONN;
		return -1;
	}
	return static_cast<int>(port_);
}

int proxy_layer::shutdown()
{
	if (state_ == fz::socket_state::shut_down) {
		return 0;
	}

	if (state_ != fz::socket_state::connected && state_ != fz::socket_state::shutting_down) {
		return ENOTCONN;
	}

	state_ = fz::socket_state::shutting_down;
	int res = next_layer_.shutdown();
	if (!res) {
		state_ = fz::socket_state::shut_down;
	}
	else if (res != EAGAIN) {
		state_ = fz::socket_state::failed;
	}
	return res;
}

std::wstring to_wstring(proxy_type t)
{
	switch (t) {
		case proxy_type::HTTP:
			return L"HTTP";

		case proxy_type::SOCKS4:
			return L"SOCKS4";

		case proxy_type::SOCKS5:
			return L"SOCKS5";

		case proxy_type::NONE:
			return L"NONE";
	}

	return _("unknown");
}

}
