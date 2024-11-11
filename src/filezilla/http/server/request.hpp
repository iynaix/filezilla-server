#ifndef FZ_HTTP_SERVER_REQUEST_HPP
#define FZ_HTTP_SERVER_REQUEST_HPP

#include <libfilezilla/uri.hpp>
#include <libfilezilla/file.hpp>

#include "../server.hpp"
#include "../headers.hpp"
#include "../../tvfs/engine.hpp"

namespace fz::http {

class server::request {
public:
	std::string method;
	fz::uri uri;
	http::headers headers;

	bool is_secure() const;
	tcp::session::id get_session_id() const;
	std::string get_peer_address() const;
	fz::address_type get_peer_address_type() const;

	enum versions {
		version_1_0,
		version_1_1
	} version {};

	request(server::transaction &t);
	virtual ~request();
	request(const request &) = delete;
	request(request &&) = delete;

	void receive_body(std::string &&body, std::function<void(std::string body, bool success)> on_end = {});
	void receive_body(tvfs::file_holder &&file, std::function<void (tvfs::file_holder, bool)> on_end = {});

private:
	server::transaction &t_;
};

}

#endif // FZ_HTTP_SERVER_REQUEST_HPP
