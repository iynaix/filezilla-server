#ifndef FZ_BUFFER_OPERATOR_ADDER_HPP
#define FZ_BUFFER_OPERATOR_ADDER_HPP

#include "../buffer_operator/detail/base.hpp"

namespace fz::buffer_operator {

	class adder_interface: public detail::base_interface
	{
	public:
		virtual int /*error*/ add_to_buffer() = 0;

		bool start_adding_to_buffer() {
			return this->send_event(0);
		}
	};

	class adder: public detail::base<adder_interface> { };

	class delegate_adder: public detail::delegate_base<adder_interface>
	{
	public:
		using delegate_base::delegate_base;

		int add_to_buffer() override
		{
			return delegate_.add_to_buffer();
		}
	};

	class no_adder: public adder
	{
		int add_to_buffer() override
		{
			return ENODATA;
		}
	};
}

#endif // FZ_BUFFER_OPERATOR_ADDER_HPP
