#include <cstdio>

#include <libfilezilla/thread.hpp>
#include <libfilezilla/string.hpp>

#include "../logger/stdio.hpp"
#include "../util/buffer_streamer.hpp"
#include "../util/thread_id.hpp"

#include "type.hpp"

namespace fz::logger {

stdio::stdio(FILE *stream)
	: stream_(stream)
{}

stdio::stdio(FILE *stream, logmsg::type enabled_levels)
	: stream_(stream)
{
	set_all(enabled_levels);
}

void stdio::do_log(fz::logmsg::type t, std::wstring &&msg)
{
	if (!stream_)
		return;

	auto buf = buffer_.lock();

	format_message(*buf, t, std::move(msg));
	log_formatted_message(*buf);
}


datetime stdio::format_message(fz::buffer &buf, logmsg::type t, std::wstring &&msg, bool include_header, bool short_type_tag, bool remove_cntrl, bool split_lines)
{
	auto now = datetime::now();

	buf.clear();
	util::buffer_streamer stream(buf);

	auto format = [&](std::wstring_view msg) {
		if (include_header) {
			#if defined(ENABLE_FZ_DEBUG) && ENABLE_FZ_DEBUG
				stream << "{Thread: " << stream.dec(util::thread_id(), 3) << "} ";
			#endif

			stream << now.format("%Y-%m-%dT%H:%M:%S.", datetime::zone::utc) << stream.dec(now.get_milliseconds(), 3, '0') << "Z ";
		}

		stream << type2str<std::string_view>(t, short_type_tag) << " ";

		if (remove_cntrl) {
			static const auto must_be_filtered = [](wchar_t c) {
				return ((c != 9 && c <= 31) || c == 127);
			};

			for (auto begin_span = msg.begin(); begin_span != msg.end();) {
				auto end_span = std::find_if(begin_span, msg.end(), must_be_filtered);

				stream << std::wstring_view(&*begin_span, std::size_t(end_span - begin_span));

				begin_span = std::find_if(end_span, msg.end(), std::not_fn(must_be_filtered));
			}
		}
		else {
			stream << msg;
		}

		stream << '\n';
	};

	if (split_lines) {
		for (auto l: strtokenizer(msg, L"\n", false))
			format(l);
	}
	else
		format(msg);

	return now;
}

void stdio::log_formatted_message(buffer &buf)
{
	fwrite(buf.get(), buf.size(), 1, stream_);
}

}
