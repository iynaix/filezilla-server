#ifndef FZ_HTTP_RESPONSE_HPP
#define FZ_HTTP_RESPONSE_HPP

#include <string>
#include <functional>

#include <libfilezilla/event_handler.hpp>
#include <libfilezilla/buffer.hpp>

#include "headers.hpp"

namespace fz::http
{

struct response
{
	unsigned int code;
	std::string reason;
	http::headers headers;
	fz::buffer body;

public:
	using event = simple_event<response, response>;

	using handler = std::function<void(response &&)>;

	enum status {
		got_headers,
		got_body,
		got_end
	};

	using partial_handler = std::function<int(status, response &)>;

	static handler forward_as_event(event_handler &eh)
	{
		return [&eh](auto && response) {
			eh.send_event<event>(std::move(response));
		};
	}

	enum code_type_t
	{
		informational = 1,
		successful,
		redirect,
		client_error,
		server_error,
		internal_error = 999
	};

	code_type_t code_type() const
	{
		return 100 > code || code > 599 ? internal_error : code_type_t(code/100);
	}

	std::string code_string() const;
};

}

#endif // FZ_HTTP_RESPONSE_HPP
