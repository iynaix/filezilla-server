#ifndef FZ_LOGGER_SPLITTER_HPP
#define FZ_LOGGER_SPLITTER_HPP

#include <unordered_set>
#include "modularized.hpp"

namespace fz::logger {

class splitter: public modularized
{
public:
	using modularized::modularized;

	bool add_logger(logger_interface &);
	bool remove_logger(logger_interface &);

private:
	void do_log(logmsg::type t, const info_list &l, std::wstring &&msg) override;

	fz::mutex mutex_{};
	std::unordered_map<logger_interface *, modularized> loggers_{};
};

}

#endif // FZ_LOGGER_SPLITTER_HPP
