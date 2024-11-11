#ifndef FZ_HTTP_SERVER_RESPONDER_HPP
#define FZ_HTTP_SERVER_RESPONDER_HPP

#include "../server.hpp"
#include "../field.hpp"

namespace fz::tvfs {

class entries_iterator;
class file_holder;

}

namespace fz::http {

/// \brief Inteface for sending HTTP responses
/// \note all the methods return a boolean for success (true) or failure (false)
/// \note in case of failure, the session gets closed
class server::responder
{
public:
	virtual ~responder() = default;

	/// \brief Sends the response result and related reason
	/// \note If the reason is the empty string, the default reason for the given code is used.
	virtual bool send_status(unsigned int code, std::string_view reason = {}) = 0;

	/// \brief Sends headers and their values.
	virtual bool send_headers(std::initializer_list<std::pair<field::name_view, field::value_view>>) = 0;

	/// \brief sends the given string_view as the body of the response
	/// \note Ends the headers before sending the body, and after sending the body implictly ends the response itself.
	/// \note After invoking this method, the request handler won't be invoked anymore for the current request.
	/// \note If Content-Type is not set, it will default to text/plain; charset=utf-8
	virtual bool send_body(std::string_view) = 0;

	/// \brief sends the given file as the body of the response
	/// \note Ends the headers before sending the body, and after sending the body implictly ends the response itself.
	/// \note After invoking this method, the request handler won't be invoked anymore for the current request.
	virtual bool send_body(tvfs::file_holder file) = 0;

	/// \brief sends the given tvfs entries listing as the body of the response.
	/// \note Ends the headers before sending the body, and after sending the body implictly ends the response itself.
	/// \note After invoking this method, the request handler won't be invoked anymore for the current request.
	/// \note The format of the response body will depend on the response's Content-Type.
	///       Currently supported: text/html; charset=utf-8, text/plain; charset=utf8, application/ndjson
	///       If not set, it will default to text/html.
	virtual bool send_body(tvfs::entries_iterator it) = 0;

	/// \brief Ends the headers and the response itself, and then prepares the session for the next request, unless the "Connection" header was set to close
	/// in which case it also ends the session.
	virtual bool send_end() = 0;

	/// \brief Simply closes the connection. Any data not sent yet is completely discarded.
	virtual void abort_send(std::string_view msg) = 0;

	/***** utilities *****/

	bool send_header(field::name_view name, field::value_view value)
	{
		return send_headers({{name, value}});
	}

	template <typename... Ts, std::enable_if_t<(sizeof...(Ts) > 0) && (std::is_constructible_v<field::value_view, Ts> && ...)>* = nullptr>
	bool send_header(field::name_view name, field::value value, Ts... vs)
	{
		return send_headers({{name, (value += ... += field::value_view(vs))}});
	}
};

}
#endif // FZ_HTTP_SERVER_RESPONDER_HPP
