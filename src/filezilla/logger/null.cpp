#include "null.hpp"

fz::logger::detail::null::null()
{
	set_all(logmsg::type());
}

fz::logger::detail::null::operator null *()
{
	return &fz::logger::null;
}

void fz::logger::detail::null::do_log(fz::logmsg::type, std::wstring &&)
{
}

fz::logger::detail::null fz::logger::null;
