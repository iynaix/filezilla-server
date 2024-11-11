#ifndef FZ_LOGGER_FILE_HPP
#define FZ_LOGGER_FILE_HPP

#include <libfilezilla/logger.hpp>
#include <libfilezilla/file.hpp>
#include <libfilezilla/buffer.hpp>
#include <libfilezilla/mutex.hpp>

#include "../logger/stdio.hpp"
#include "../logger/type.hpp"
#include "../util/options.hpp"


namespace fz::logger {

class file: public logger::stdio {
public:
	enum rotation_type {
		size_based,
		daily
	};

	struct options: util::options<options, logger::file> {
		enum: std::int64_t {
			max_possible_size = std::numeric_limits<std::int64_t>::max()
		};

		opt<logmsg::type>       enabled_types               = o(fz::logmsg::type(fz::logmsg::default_types));
		opt<native_string>      name                        = o();
		opt<std::uint16_t>      max_amount_of_rotated_files = o();
		opt<enum rotation_type> rotation_type               = o(rotation_type::size_based);
		opt<int64_t>            max_file_size               = o(max_possible_size);
		opt<bool>               include_headers             = o(true);
		opt<bool>               start_line                  = o(true);
		opt<bool>               remove_cntrl                = o(true);
		opt<bool>               split_lines                 = o(true);
		opt<bool>               date_in_name                = o(false);
		opt<bool>               short_type_tag              = o(true);

		options(){}
	};

	file(options opts = {});

	void set_options(const options &opts);

protected:
	void do_log(logmsg::type t, std::wstring &&msg) override;

private:
	void maybe_rotate(const fz::datetime &now);
	void open(fz::file::creation_flags flags);

	options opts_;

	fz::file file_{};
	int64_t file_size_{};
	fz::datetime file_dt_{};
};

}

#endif // FZ_LOGGER_FILE_HPP
