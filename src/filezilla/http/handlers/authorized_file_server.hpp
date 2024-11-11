#ifndef FZ_HTTP_HANDLERS_AUTHORIZED_FILE_SERVER_HPP
#define FZ_HTTP_HANDLERS_AUTHORIZED_FILE_SERVER_HPP

#include "file_server.hpp"
#include "authorizator.hpp"

namespace fz::http::handlers
{

class authorized_file_server: public server::transaction_handler, authorizator::custom_authorization_data_factory
{
public:
	authorized_file_server(authorizator &auth, logger_interface &logger, file_server::options opts);

private:
	struct custom_authorization_data;

	std::shared_ptr<void> make_custom_authorization_data() override;

	void handle_transaction(const server::shared_transaction &t) override;

	handlers::authorizator &auth_;
	logger_interface &logger_;
	file_server::options opts_;
};

}

#endif // FZ_HTTP_HANDLERS_AUTHORIZED_FILE_SERVER_HPP
