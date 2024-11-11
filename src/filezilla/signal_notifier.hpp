#ifndef FZ_SIGNAL_NOTIFIER_HPP
#define FZ_SIGNAL_NOTIFIER_HPP

#include <functional>
#include <list>
#include <utility>

namespace fz {

using signal_handler_t = std::function<void(int signum)>;

class signal_notifier
{
public:
	using handlers_list = std::list<signal_handler_t>;

	struct token_t
	{
		int signum{};
		handlers_list::iterator handler_it{};

		explicit operator bool() const
		{
			return signum > 0;
		}
	};

	//! \brief attaches an handler to a signal
	//! \returns a token referring to the handler, which can later be used to \ref detach(token_t) it
	//! \note You cannot use \ref attach(int, signal_handler_t) or \ref detach(token_t) from within a signal handler.
	static token_t attach(int signum, signal_handler_t handler);

	//! \brief detaches an handler from a signal
	//! \returns true if the handler was detached, false otherwise.
	//! \note You cannot use \ref attach(int, signal_handler_t) or \ref detach(token_t) from within a signal handler.
	static bool detach(token_t &token);
};

}
#endif // FZ_SIGNAL_NOTIFIER_HPP
