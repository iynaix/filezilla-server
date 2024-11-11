#ifndef FZ_SERIALIZATION_TYPES_LOGGER_FILE_HPP
#define FZ_SERIALIZATION_TYPES_LOGGER_FILE_HPP

#include "../../serialization/types/optional.hpp"
#include "../../logger/file.hpp"

namespace fz::serialization {

template <typename Archive>
void serialize(Archive &ar, logger::file::options &o) {
	using namespace serialization;

	ar(
		value_info(optional_nvp(o.name(),
				   "name"),
				   "The name of the log file. If empty, the log goes to stderr."),

		value_info(optional_nvp(o.max_amount_of_rotated_files(),
				   "max_amount_of_rotated_files"),
				   "The maximum number of files to be used for the log rotation. Default is 0, meaning no rotation happens."),

		value_info(optional_nvp(o.max_file_size(),
				   "max_file_size"),
				   "The maximum size each log file can reach before being closed and a new one being opened. Only meaningful if max_amount_of_rotated_files > 0."),

		value_info(optional_nvp(o.enabled_types(),
				   "enabled_types"),
				   "Which types of logs must be enabled. Defaults to error|status|reply|command . See <libfilezilla/logger.hpp> for the values of the various types."),

		value_info(optional_nvp(o.rotation_type(),
				   "rotation_type"),
				   "Criteria used to rotate files. Currently: size-based (0) or daily rotation (1)."),

		value_info(optional_nvp(o.include_headers(),
				   "include_headers"),
				   "Include headers for each line of logging (date&time and possibly others). Set it to false if you have your own way of tagging log lines. Default is true."),

		value_info(optional_nvp(o.date_in_name(),
				   "date_in_name"),
				   "Append the date of the log file to its name when rotating, before any suffix the name might have.")
	);
}

}
#endif // FZ_SERIALIZATION_TYPES_LOGGER_FILE_HPP
