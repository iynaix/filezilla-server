#ifndef FZ_RMP_ENGINE_VISITOR_HPP
#define FZ_RMP_ENGINE_VISITOR_HPP

#include "../../rmp/engine/dispatcher.hpp"
#include "../../rmp/engine/access.hpp"
#include "../../mpl/with_index.hpp"
#include "../../mpl/at.hpp"

namespace fz::rmp {

template <typename AnyMessage>
template <typename Implementation>
class engine<AnyMessage>::visitor: public dispatcher
{
public:
	visitor(Implementation &implementation) noexcept
		: implementation_(implementation)
	{}

	void load_and_dispatch(session &session, std::uint16_t index, serialization::binary_input_archive &l) override
	{
		mpl::with_index<any_message::size()>(index, [&](auto i) {
			using Message = mpl::at_c_t<typename any_message::messages, i>;
			access::template load_and_dispatch<Message>(implementation_, session, l);
		});
	}

private:
	Implementation &implementation_;
};

}
#endif // FZ_RMP_ENGINE_VISITOR_HPP
