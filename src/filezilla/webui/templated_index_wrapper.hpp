#ifndef FZ_WEBUI_TEMPLATED_INDEX_WRAPPER_HPP
#define FZ_WEBUI_TEMPLATED_INDEX_WRAPPER_HPP

#include "../http/server/transaction.hpp"
#include "../http/handlers/file_server.hpp"


namespace fz::webui {

struct templated_index_wrapper: http::server::transaction_handler
{
	templated_index_wrapper(http::handlers::file_server &fs)
		: fs_(fs)
	{}

	void handle_transaction(const http::server::shared_transaction &t) override;

private:
	http::handlers::file_server &fs_;
	mutex mutex_;
	datetime mtime_;
	std::string body_;
};

}

#endif // FZ_WEBUI_TEMPLATED_INDEX_WRAPPER_HPP
