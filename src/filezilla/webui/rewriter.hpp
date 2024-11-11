#ifndef FZ_WEBUI_REWRITER_HPP
#define FZ_WEBUI_REWRITER_HPP

#include "../http/server/transaction.hpp"

namespace fz::webui
{

struct rewriter: http::server::transaction_handler {
	rewriter(fz::http::server::transaction_handler &th);

	void handle_transaction(const http::server::shared_transaction &t) override;

private:
	http::server::transaction_handler &th_;
};

}

#endif // FZ_WEBUI_REWRITER_HPP
