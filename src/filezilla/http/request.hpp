#ifndef FZ_HTTP_REQUEST_HPP
#define FZ_HTTP_REQUEST_HPP

#include <string>

#include <libfilezilla/buffer.hpp>
#include <libfilezilla/uri.hpp>

#include "headers.hpp"

namespace fz::http
{

class request
{
public:
	std::string method;
	fz::uri uri;
	http::headers headers;
	fz::buffer body;
};

}

#endif // FZ_HTTP_REQUEST_HPP
