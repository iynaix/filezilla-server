#ifndef FZ_HTTP_SERVER_TRANSACTION_HPP
#define FZ_HTTP_SERVER_TRANSACTION_HPP

#include "request.hpp"
#include "responder.hpp"

namespace fz::http {

class server::transaction
{
public:
	virtual ~transaction()
	{}

	virtual request &req() = 0;
	virtual responder &res() = 0;
	virtual util::locked_proxy<session> get_session() = 0;
	virtual event_loop &get_event_loop() = 0;
};


class server::transaction_handler
{
public:
	virtual ~transaction_handler()
	{}
	// The Handler will be invoked when all the headers have been parsed
	virtual void handle_transaction(const server::shared_transaction &t) = 0;
};

}

#endif // FZ_HTTP_SERVER_TRANSACTION_HPP
