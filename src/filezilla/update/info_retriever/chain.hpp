#ifndef FZ_UPDATE_INFO_RETRIEVER_CHAIN_HPP
#define FZ_UPDATE_INFO_RETRIEVER_CHAIN_HPP

#include <vector>

#include "../../logger/modularized.hpp"

#include "../info.hpp"

namespace fz::update::info_retriever
{

class chain: public info::retriever
{
public:
	using raw_data_retrievers = std::vector<info::raw_data_retriever*>;
	using raw_data_retrievers_factory = std::function<raw_data_retrievers(bool manual)>;

	chain(logger_interface &logger, std::initializer_list<std::reference_wrapper<info::raw_data_retriever>> retrievers);
	chain(logger_interface &logger, raw_data_retrievers_factory factory = {});

	void set_expiration(fz::duration expiration);
	void retrieve_info(bool manual, allow allowed, fz::receiver_handle<result>) override;

	void set_retrievers_factory(raw_data_retrievers_factory factory);
	void set_retrievers(std::initializer_list<std::reference_wrapper<info::raw_data_retriever>> retrievers);

private:
	raw_data_retrievers_factory make_factory(std::initializer_list<std::reference_wrapper<info::raw_data_retriever>>);

	logger::modularized logger_;
	fz::duration expiration_;
	raw_data_retrievers_factory factory_;
};

}

#endif // FZ_UPDATE_INFO_RETRIEVER_CHAIN_HPP
