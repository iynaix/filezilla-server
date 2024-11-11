#include "build_info.hpp"
#include "util/parser.hpp"


#ifdef HAVE_CONFIG_H
#	include "config.hpp"
#endif

#ifndef FZ_BUILD_TYPE
#	define FZ_BUILD_TYPE official
#endif

#ifndef FZ_BUILD_FLAVOUR
#	define FZ_BUILD_FLAVOUR standard
#endif

#ifndef PACKAGE_VERSION
#	define PACKAGE_VERSION "0.0.0"
#endif

#if __has_include("build_datetime.hpp")
#	include "build_datetime.hpp"
#endif

#ifndef FZ_BUILD_HOST
#	define FZ_BUILD_HOST "unknown";
#endif

namespace fz::build_info {

const build_type type = build_type::FZ_BUILD_TYPE;
const flavour_type flavour = flavour_type::FZ_BUILD_FLAVOUR;
const version_info version = PACKAGE_VERSION;
const std::string package_name = PACKAGE_NAME;
const std::string host = FZ_BUILD_HOST;
const std::string url = PACKAGE_URL;
const std::string copyright = PACKAGE_COPYRIGHT;

static_assert(version_info(PACKAGE_VERSION), "PACKAGE_VERSION must be defined to a valid version, but instead it's [" PACKAGE_VERSION "]");

const std::string warning_message = []() constexpr {
	if constexpr (type != build_type::nightly)
		return "";
	else {
		return
			"Attention, you are using a Nightly Build, built automatically every night.\n"
			"Please be advised that nightly builds are untested, only use them if you absolutely need to test changes that were made since the last stable release.\n"
			"Use FileZilla Server nightly builds at your own risk. This build might not work and may damage your data.";
	}
}();

bool convert(std::string_view s, flavour_type &f)
{
	f = {};

	// First check if the string is in a valid format: must be a C-like identifier.
	if (util::parseable_range r(s); !(lit(r, 'a', 'z') || lit(r, 'A', 'Z') || lit(r, '_')))
		return false;
	else
	while (!eol(r)) {
		if (!(lit(r, 'a', 'z') || lit(r, 'A', 'Z') || lit(r, '_') || lit(r, '0', '9')))
			return false;
	}

	if (s == "standard")
		f = flavour_type::standard;
	else
	if (s == "professional_enterprise")
		f = flavour_type::professional_enterprise;

	return true;
}

const fz::datetime datetime = [] {
#if defined(FZ_BUILD_DATETIME)
	fz::datetime dt;
	dt.set(FZ_BUILD_DATETIME, fz::datetime::utc);
	return dt;
#else
	// The following assumes __DATE__ and __TIME__ are in UTC.
	static constexpr std::string_view date = __DATE__;
	static constexpr std::string_view time = __TIME__;

	static constexpr auto month = []() constexpr {
		constexpr const std::string_view months[12] = {
			"Jan", "Feb", "Mar", "Apr", "May", "Jun",
			"Jul", "Ago", "Sep", "Oct", "Nov", "Dec"
		};

		auto m = date.substr(0, 3);

		for (int i = 0; i < 12; ++i) {
			if (m == months[i])
				return i + 1;
		}

		return 0;
	}();

	static constexpr auto day = []() constexpr {
		int a = date[4] == ' ' ? 0 : date[4] - '0';
		int b = date[5] - '0';

		return a*10 + b;
	}();

	static constexpr auto year = []() constexpr {
		int a = date[7]-'0';
		int b = date[8]-'0';
		int c = date[9]-'0';
		int d = date[10]-'0';

		return a*1000 + b*100 + c*10 + d;
	}();

	static constexpr auto hour = []() constexpr {
		int a = time[0]-'0';
		int b = time[1]-'0';

		return a*10 + b;
	}();

	static constexpr auto minute = []() constexpr {
		int a = time[3]-'0';
		int b = time[4]-'0';

		return a*10 + b;
	}();

	static constexpr auto second = []() constexpr {
		int a = time[6]-'0';
		int b = time[7]-'0';

		return a*10 + b;
	}();

	return fz::datetime(fz::datetime::utc, year, month, day, hour, minute, second);
#endif
}();

}
