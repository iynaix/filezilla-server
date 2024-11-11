#include "splitter.hpp"

namespace fz::logger {

bool splitter::add_logger(logger_interface &l)
{
	fz::scoped_lock lock(mutex_);

	return loggers_.emplace(&l, l).second;
}

bool splitter::remove_logger(logger_interface &l)
{
	fz::scoped_lock lock(mutex_);

	return loggers_.erase(&l) == 1;
}

void splitter::do_log(logmsg::type t, const info_list &l, std::wstring &&msg)
{
	modularized::do_log(t, l, std::wstring(msg));

	fz::scoped_lock lock(mutex_);

	for (auto &v: loggers_) {
		if (v.first->should_log(t))
			v.second.do_log(t, l, std::wstring(msg));
	}
}

}
