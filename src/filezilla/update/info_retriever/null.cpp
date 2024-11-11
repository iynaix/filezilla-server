#include "null.hpp"

namespace fz::update::info_retriever
{

null::null(){}

void null::retrieve_info(bool, allow, fz::receiver_handle<result> h)
{
	h(info{});
}

}
