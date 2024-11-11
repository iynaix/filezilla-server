#ifndef FZ_UPDATE_INFO_RETRIEVER_NULL_HPP
#define FZ_UPDATE_INFO_RETRIEVER_NULL_HPP

#include "../info.hpp"

namespace fz::update::info_retriever
{

class null: public info::retriever
{
public:
	null();

	void retrieve_info(bool, allow, fz::receiver_handle<result> h) override;
};

}

#endif // FZ_UPDATE_INFO_RETRIEVER_CHAIN_HPP
