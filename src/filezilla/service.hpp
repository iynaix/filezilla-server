#ifndef FZ_SERVICE_HPP
#define FZ_SERVICE_HPP

#include <libfilezilla/libfilezilla.hpp>

#include <functional>
#include <string>

namespace fz::service {

#if defined(FZ_WINDOWS) && FZ_WINDOWS
	using argv_t = wchar_t **;
#   define FZ_SERVICE_PROGRAM_MAIN(argc, argv) wmain(int argc, wchar_t *argv[])
#else
	using argv_t = char **;
#   define FZ_SERVICE_PROGRAM_MAIN(argc, argv) main(int argc, char *argv[])
#endif

/// \brief Handles all the bureocracy to deal with the generic "service" concept.
/// On Win32 it uses the Windows service API.
/// On Unices it starts a normal process and reacts on SIGINT, in which case the stop function is invoked.
/// \param start is the service main function. Return from this function in order to exit the service.
/// \param stop is a function in charge of stopping the service.
/// \param reload_config is a function in charge of reloading the service's config, if any.
int make(int argc, argv_t argv, std::function<int(int argc, char *argv[])> start, std::function<void()> stop = {}, std::function<void()> reload_config = {});

}
#endif // FZ_SERVICE_HPP
