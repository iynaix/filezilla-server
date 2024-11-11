#ifndef FZ_RMP_ENGINE_DISPATCHER_HPP
#define FZ_RMP_ENGINE_DISPATCHER_HPP

#include <libfilezilla/tls_info.hpp>

#include "../../rmp/engine.hpp"

namespace fz::rmp {

template <typename AnyMessage>
class engine<AnyMessage>::dispatcher {
public:
	virtual ~dispatcher(){}

	virtual void load_and_dispatch(session &session, std::uint16_t index, serialization::binary_input_archive &l) = 0;

	virtual void connection(session *, int){}
	virtual void disconnection(session &, int){}

	virtual void send_buffer_is_in_overflow(session &){}
	virtual void certificate_verification(session &s, tls_session_info &&info)
	{
		s.set_certificate_verification_result(info.system_trust());
	}
};


}
#endif // FZ_RMP_DISPATCHER_HPP
