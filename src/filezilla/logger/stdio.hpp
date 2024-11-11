#ifndef FZ_LOGGER_STDIO_HPP
#define FZ_LOGGER_STDIO_HPP

#include <cstdio>

#include <libfilezilla/buffer.hpp>

#include "../util/locking_wrapper.hpp"
#include "hierarchical.hpp"

namespace fz::logger {

class stdio: public hierarchical_interface {
public:
	stdio(FILE *stream);
	stdio(FILE *stream, logmsg::type enabled_levels);

protected:
	void do_log(fz::logmsg::type t, std::wstring &&msg) override;
	static datetime format_message(fz::buffer &buf, fz::logmsg::type t, std::wstring &&msg, bool include_header = true, bool short_type_tag = true, bool remove_cntrl = true, bool split_lines = true);
	void log_formatted_message(fz::buffer &buf);

	FILE *stream_{};

	util::locking_wrapper<fz::buffer> buffer_;
};

}

#endif
