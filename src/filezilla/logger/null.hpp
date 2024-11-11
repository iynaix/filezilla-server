#ifndef FZ_LOGGER_NULL_HPP
#define FZ_LOGGER_NULL_HPP

#include <libfilezilla/logger.hpp>

namespace fz::logger
{

namespace detail
{
	class null: public logger_interface
	{
	public:
		null();

		operator null*();

	private:
		void do_log(logmsg::type t, std::wstring &&msg) override;
	};
}

extern detail::null null;

}

#endif // FZ_LOGGER_NULL_HPP
