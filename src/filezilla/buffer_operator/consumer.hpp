#ifndef FZ_BUFFER_OPERATOR_CONSUMER_HPP
#define FZ_BUFFER_OPERATOR_CONSUMER_HPP

#include "../buffer_operator/detail/base.hpp"

namespace fz::buffer_operator {

	class consumer_interface: public detail::base_interface {
	public:
		virtual int /*error*/ consume_buffer() = 0;
	};

	class consumer: public detail::base<consumer_interface> { };

	class delegate_consumer: public detail::delegate_base<consumer_interface>
	{
	public:
		using delegate_base::delegate_base;

		int consume_buffer() override
		{
			return delegate_.consume_buffer();
		}
	};

	class no_consumer: public consumer
	{
		int consume_buffer() override
		{
			return ECANCELED;
		}
	};

}

#endif // FZ_BUFFER_OPERATOR_CONSUMER_HPP

