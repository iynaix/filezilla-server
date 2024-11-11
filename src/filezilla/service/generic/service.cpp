#include <csignal>

#include "../../../filezilla/service.hpp"
#include "../../../filezilla/signal_notifier.hpp"

namespace fz::service {

int make(int argc, char **argv, std::function<int (int argc, char *argv[])> start, std::function<void ()> stop, std::function<void()> reload_config)
{
	auto stop_token = signal_notifier::attach(SIGINT, [&stop](int) { if (stop) stop();});
	auto reload_config_token = signal_notifier::attach(SIGHUP, [&reload_config](int) { if (reload_config) reload_config();});

	int ret = start(argc, argv);

	signal_notifier::detach(stop_token);
	signal_notifier::detach(reload_config_token);

	return ret;
}

}
