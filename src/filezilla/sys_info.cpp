#include <tuple>

#include <libfilezilla/libfilezilla.hpp>

#if defined(FZ_WINDOWS)
#	include <libfilezilla/glue/windows.hpp>
#elif defined(FZ_UNIX)
#	include <sys/utsname.h>
#elif defined(FZ_MAC)
#	include <Carbon/Carbon.h>
#else
#	error "Unknown architecture"
#endif

#include "sys_info.hpp"

namespace fz::sys_info
{

#if defined(__i386__) || defined(__x86_64__) || defined(_M_X64) || defined(_M_IX86)
#define HAVE_CPUID 1
#endif

#if HAVE_CPUID
#	ifdef _MSC_VER
		namespace
		{

			void cpuid(int32_t f, int32_t sub, int32_t reg[4])
			{
				__cpuidex(reg, f, sub);
			}

		}
#	else
#		include <cpuid.h>

		namespace
		{

			void cpuid(int32_t f, int32_t sub, int32_t reg[4])
			{
				__cpuid_count(f, sub, reg[0], reg[1], reg[2], reg[3]);
			}

		}
#	endif
#endif

const std::vector<std::string> cpu_caps = [] {

	std::vector<std::string> ret;

#if HAVE_CPUID
	int reg[4];
	cpuid(0, 0, reg);

	int const max = reg[0];

	cpuid(int32_t(0x80000000), 0, reg);
	int const extmax = reg[0];

	// function (aka leaf), subfunction (subleaf), register, bit, description
	std::tuple<int32_t, int32_t, int32_t, int32_t, std::string> const caps[] = {
		std::make_tuple(1, 0, 3, 25, "sse"),
		std::make_tuple(1, 0, 3, 26, "sse2"),
		std::make_tuple(1, 0, 2, 0,  "sse3"),
		std::make_tuple(1, 0, 2, 9,  "ssse3"),
		std::make_tuple(1, 0, 2, 19, "sse4.1"),
		std::make_tuple(1, 0, 2, 20, "sse4.2"),
		std::make_tuple(1, 0, 2, 28, "avx"),
		std::make_tuple(7, 0, 1, 5,  "avx2"),
		std::make_tuple(1, 0, 2, 25, "aes"),
		std::make_tuple(1, 0, 2, 1,  "pclmulqdq"),
		std::make_tuple(1, 0, 2, 30, "rdrnd"),
		std::make_tuple(7, 0, 1, 3,  "bmi"),
		std::make_tuple(7, 0, 1, 8,  "bmi2"),
		std::make_tuple(7, 0, 1, 19, "adx"),
		std::make_tuple(0x80000001, 0, 3, 29, "lm")
	};

	for (auto const& cap : caps) {
		auto leaf = std::get<0>(cap);
		if (leaf > 0 && max < leaf) {
			continue;
		}
		if (leaf < 0 && leaf > extmax) {
			continue;
		}

		cpuid(leaf, std::get<1>(cap), reg);
		if (reg[std::get<2>(cap)] & (1 << std::get<3>(cap)))
			ret.push_back(std::get<4>(cap));
	}
#endif

	return ret;
}();

const os_version_info os_version = []() -> os_version_info {
#ifdef FZ_WINDOWS
	static auto is_at_least = [](unsigned int major, unsigned int minor = 0) -> bool
	{
		OSVERSIONINFOEX vi{};
		vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
		vi.dwMajorVersion = major;
		vi.dwMinorVersion = minor;
		vi.dwPlatformId = VER_PLATFORM_WIN32_NT;

		DWORDLONG mask{};
		VER_SET_CONDITION(mask, VER_MAJORVERSION, VER_GREATER_EQUAL);
		VER_SET_CONDITION(mask, VER_MINORVERSION, VER_GREATER_EQUAL);
		VER_SET_CONDITION(mask, VER_PLATFORMID, VER_EQUAL);

		return VerifyVersionInfo(&vi, VER_MAJORVERSION | VER_MINORVERSION | VER_PLATFORMID, mask) != 0;
	};

	// Microsoft, in its insane stupidity, has decided to make GetVersion(Ex) useless, starting with Windows 8.1,
	// this function no longer returns the operating system version but instead some arbitrary and random value depending
	// on the phase of the moon.
	// This function instead returns the actual Windows version. On non-Windows systems, it's equivalent to
	// wxGetOsVersion
	unsigned int major = 4;
	unsigned int minor = 0;

	while (is_at_least(++major, minor))
	{
	}
	--major;

	while (is_at_least(major, ++minor))
	{
	}
	--minor;

	return {major, minor};
#elif defined (FZ_UNIX)
	os_version_info ret;
	utsname buf{};

	if (!::uname(&buf)) {
		char const* p = buf.release;

		while (*p >= '0' && *p <= '9') {
			ret.major *= 10;
			ret.major += unsigned(*p - '0');
			++p;
		}

		if (*p == '.') {
			++p;
			while (*p >= '0' && *p <= '9') {
				ret.minor *= 10;
				ret.minor += unsigned(*p - '0');
				++p;
			}
		}
	}

	return ret;
#elif defined(FZ_MAC)
	SInt32 major{};
	Gestalt(gestaltSystemVersionMajor, &major);

	SInt32 minor{};
	Gestalt(gestaltSystemVersionMinor, &minor);

	return {static_cast<unsigned int>(major), static_cast<unsigned int>(minor)};
#else
	return {};
#endif
}();

}
