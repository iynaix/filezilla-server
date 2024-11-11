#ifndef FZ_UTIL_DISPATCHER_HPP
#define FZ_UTIL_DISPATCHER_HPP

#include <libfilezilla/event_handler.hpp>

namespace fz::util {

template <typename... EFs>
struct dispatcher;

template <typename... Es, typename... Fs>
struct dispatcher<std::pair<Es, Fs>...>: public fz::event_handler, Fs...
{
	dispatcher(fz::event_loop &loop, Fs &&... fs) noexcept
		: fz::event_handler(loop)
		, Fs(std::forward<Fs>(fs))...
	{}

	~dispatcher() override
	{
		remove_handler();
	}

	void operator()(const fz::event_base &ev) override
	{
		fz::dispatch<Es...>(ev, this, &Fs::operator()...);
	}
};

template <typename... Es, typename... Fs>
dispatcher<std::pair<Es, Fs>...> make_dispatcher(fz::event_loop &loop, Fs &&... fs)
{
	return {loop, std::forward<Fs>(fs)...};
}

template <typename... Es, typename... Fs>
std::unique_ptr<dispatcher<std::pair<Es, Fs>...>> make_unique_dispatcher(fz::event_loop &loop, Fs &&... fs)
{
	return std::make_unique<dispatcher<std::pair<Es, Fs>...>>(loop, std::forward<Fs>(fs)...);
}

template <typename... Es, typename... Fs>
std::shared_ptr<dispatcher<std::pair<Es, Fs>...>> make_shared_dispatcher(fz::event_loop &loop, Fs &&... fs)
{
	return std::make_shared<dispatcher<std::pair<Es, Fs>...>>(loop, std::forward<Fs>(fs)...);
}

}

#endif // FZ_UTIL_DISPATCHER_HPP
