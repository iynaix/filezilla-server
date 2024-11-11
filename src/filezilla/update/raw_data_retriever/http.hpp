#ifndef FZ_UPDATE_RAW_DATA_RETRIEVER_HTTP_HPP
#define FZ_UPDATE_RAW_DATA_RETRIEVER_HTTP_HPP

#include "../../http/client.hpp"
#include "../info.hpp"

namespace fz::update::raw_data_retriever
{

class http: public info::raw_data_retriever
{
public:
	http(event_loop &loop, thread_pool &pool, logger_interface &logger);

	void retrieve_raw_data(bool manual, fz::receiver_handle<result>) override;

	void retrieve_raw_data(std::string query_string, fz::receiver_handle<result>);
	static std::string get_query_string(bool manual);

	static const fz::duration response_timeout;

private:
	logger::modularized logger_;
	fz::http::client http_client_;
};

}

#endif // FZ_UPDATE_RAW_DATA_RETRIEVER_HTTP_HPP
