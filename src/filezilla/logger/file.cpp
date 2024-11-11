#include <libfilezilla/format.hpp>

#include "../logger/file.hpp"

#include "../util/filesystem.hpp"
#include "../util/parser.hpp"
#include "../build_info.hpp"

namespace fz::logger {

file::file(file::options opts)
	: stdio(stderr)
{
	set_options(std::move(opts));
}

void file::set_options(const file::options &opts)
{
	bool emit_start_line = opts.start_line() && opts.name() != opts_.name();
	set_all(opts.enabled_types());

	{
		auto lock = buffer_.lock();
		opts_ = opts;

		open(fz::file::creation_flags::existing);
	}

	if (emit_start_line)
		log_u(logmsg::status, L"===== %s %s new logging started =====", fz::build_info::package_name, fz::build_info::version);
}

void file::do_log(logmsg::type t, std::wstring &&msg)
{
	auto buffer = buffer_.lock();

	auto now = format_message(*buffer, t, std::move(msg), opts_.include_headers(), opts_.short_type_tag(), opts_.remove_cntrl(), opts_.split_lines());
	if (!file_.opened()) {
		stdio::log_formatted_message(*buffer);
		return;
	}

	maybe_rotate(now);

	while (buffer->size()) {
		auto to_consume = file_.write(buffer->get(), (int64_t)buffer->size());

		if (to_consume < 0) {
			stdio::log_formatted_message(*buffer);
			break;
		}

		file_size_ += to_consume;

		buffer->consume((std::size_t)to_consume);
	}
}

void file::open(fz::file::creation_flags flags)
{
	file_.close();
	file_dt_ = {};

	if (!opts_.name().empty() && opts_.max_file_size() > 0) {
		file_.open(opts_.name(), fz::file::mode::writing, flags | fz::file::creation_flags::current_user_and_admins_only);

		if (flags & fz::file::creation_flags::existing)
			file_.seek(0, fz::file::seek_mode::end);

		auto size = file_.size();
		file_size_ = size < 0 ? 0 : size;
		file_dt_ = local_filesys::get_modification_time(opts_.name());
	}
}

void file::maybe_rotate(const datetime &now)
{
	if (opts_.max_amount_of_rotated_files() == 0)
		return;

	// Check if rotation criteria are satisfied
	switch (opts_.rotation_type()) {
		case rotation_type::size_based:
			if (file_size_ < opts_.max_file_size())
				return;
			break;

		case rotation_type::daily:
			static const auto origin = datetime(0, datetime::days);
			if ((now - origin).get_days() == (file_dt_ - origin).get_days())
				return;
			break;
	}

	// In which case, rotate.

	file_.close();
	file_size_ = 0;

	if (native_string_view name = opts_.name(); !name.empty()) {
		std::vector<std::pair<native_string /* date */, std::size_t /* index */>> suffixes;

		native_string_view suffix;

		if (auto dotpos = name.rfind(fzT(".")); dotpos != native_string_view::npos) {
			suffix = name.substr(dotpos);
			name.remove_suffix(suffix.size());
		}

		auto parse_date_suffix = [this](auto &&r, native_string_view &date) {
			if (!opts_.date_in_name()) {
				date = {};
				return true;
			}

			std::uint16_t year;
			std::uint8_t month;
			std::uint8_t day;

			static constexpr std::uint8_t days_in_month[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

			auto begin = r.it;

			bool matched = lit(r, '.') &&
				parse_int(r, 4, year) && lit(r, '-') &&
				parse_int(r, 2, month) && month >= 1 && month <= 12 && lit(r, '-') &&
				parse_int(r, 2, day) && day >= 1 && (day <= ((month == 2 && year % 4 == 0) ? 29 : days_in_month[month - 1]));

			if (matched) {
				date = native_string_view(&*begin, std::size_t(r.it - begin));
				return true;
			}

			return false;
		};

		auto parse_index_suffix = [this](auto &&r, std::size_t &index) {
			auto begin = r.it;

			if (lit(r, '.')) {
				if (parse_int(r, index) && index > 0)
					return true;

				r.it = begin;
			}

			index = 0;
			return opts_.date_in_name();
		};

		auto make_name = [&name, &suffix](const native_string &date_suffix, std::size_t index_suffix)	{
			auto full_name = native_string(name) + date_suffix;

			if (index_suffix)
				full_name.append(fzT(".")).append(fz::toString<fz::native_string>(index_suffix));

			full_name.append(suffix);

			return full_name;
		};

		for (const auto &file: util::fs::native_path(opts_.name()).parent()) {
			util::parseable_range r(file.str());

			native_string_view date;
			std::size_t index;

			if (match_string(r, name) && parse_date_suffix(r, date) && parse_index_suffix(r, index) && match_string(r, suffix) && eol(r))
				suffixes.emplace_back(native_string(date), index);
		}

		std::sort(suffixes.begin(), suffixes.end(), [](auto &s1, auto &s2) {
			if (s1.first == s2.first) {
				return s1.second < s2.second;
			}

			return s2.first < s1.first;
		});

		// Remove the excess files
		if (suffixes.size() >= opts_.max_amount_of_rotated_files()) {
			for (auto it = suffixes.begin() + opts_.max_amount_of_rotated_files()-1; it != suffixes.end(); ++it)
				fz::remove_file(make_name(it->first, it->second), false);

			suffixes.resize(opts_.max_amount_of_rotated_files()-1);
		}

		// Determine the new suffixes
		native_string date_suffix;
		std::size_t index_suffix = 1;

		if (opts_.date_in_name()) {
			date_suffix = fzT(".") + file_dt_.format(fzT("%Y-%m-%d"), datetime::utc);
			if (suffixes.size() == 0 || suffixes[0].first != date_suffix)
				index_suffix = 0;
		}

		// Rename the indices
		if (index_suffix && suffixes.size() > 0) {
			auto rename_end = suffixes.begin();

			while (rename_end != suffixes.end() && rename_end->first == date_suffix)
				++rename_end;

			for (auto it = std::make_reverse_iterator(rename_end); it != suffixes.rend(); ++it)
				fz::rename_file(make_name(it->first, it->second), make_name(it->first, it->second+1));
		}

		fz::rename_file(opts_.name(), make_name(date_suffix, index_suffix));
	}

	// Create the new file
	open(fz::file::creation_flags::empty);
}

}
