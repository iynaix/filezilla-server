#ifndef FZ_TCP_LISTENER_HPP
#define FZ_TCP_LISTENER_HPP

#include <any>
#include <set>

#include <libfilezilla/event_handler.hpp>
#include <libfilezilla/socket.hpp>
#include <libfilezilla/logger.hpp>

#include "../tcp/address_info.hpp"

namespace fz::tcp {

class listener: private event_handler {
	struct connected_event_tag {};

public:
	struct user_data
	{
		std::any data;
		std::string name;

		user_data() = default;
		user_data(const user_data &) = default;
		user_data(user_data &&) = default;

		user_data& operator=(const user_data &) = default;
		user_data& operator=(user_data &&) = default;

		template <typename T, std::enable_if_t<!std::is_same_v<T, user_data>>* = nullptr>
		user_data(T && data)
			: data(std::forward<T>(data))
		{}

		template <typename T>
		user_data(T && data, std::string name)
			: data(std::forward<T>(data))
			, name(std::move(name))
		{}

		template <typename T, std::enable_if_t<!std::is_same_v<T, user_data>>* = nullptr>
		user_data& operator=(T && data)
		{
			return *this = user_data(std::forward<T>(data));
		}

		operator std::any & () &
		{
			return data;
		}

		operator const std::any & () const
		{
			return data;
		}

		operator std::any () &&
		{
			return std::move(data);
		}
	};

	struct peer_allowance_checker {
		virtual ~peer_allowance_checker();

		virtual bool is_peer_allowed(std::string_view ip, address_type family) const = 0;

		static const peer_allowance_checker &allow_all;
	};

	struct status_change_notifier {
		virtual ~status_change_notifier();

		virtual void listener_status_changed(const listener &listener) = 0;

		static status_change_notifier &none;
	};

	using connected_event = simple_event<connected_event_tag, listener &/*self*/>;

	enum class status
	{
		started,          //!< Port is availabe, listening has effectively started.
		stopped,          //!< Listening has stopped
		retrying_to_start //!< Port is unavailable, will try to start repeteadly until it succeeds or is excplicitly stopped.
	};

	listener(fz::thread_pool &pool, event_loop &loop, event_handler &target_handler, logger_interface &logger, address_info address_info,
			 const peer_allowance_checker &pac = peer_allowance_checker::allow_all,
			 status_change_notifier &scn = status_change_notifier::none,
			 user_data user_data = {});
	~listener() override;

	const address_info &get_address_info() const;
	address_info &get_address_info();
	const user_data &get_user_data() const;
	user_data &get_user_data();
	status get_status() const;

	// Call in a loop to get the accepted sockets, until it returns nullptr;
	std::unique_ptr<fz::socket> get_socket();
	bool has_socket();

	void start();
	void stop();

private:
	void operator ()(const event_base &ev) override;
	void on_socket_event(fz::socket_event_source *, fz::socket_event_flag type, int error);
	void on_timer_event(timer_id const &);

	void try_listen();

private:
	mutable fz::mutex mutex_;
	status status_ = status::stopped;
	std::unique_ptr<listen_socket> listen_socket_;
	fz::timer_id timer_id_{};

	thread_pool &pool_;
	event_handler &target_handler_;
	logger_interface &logger_;
	address_info address_info_;
	const peer_allowance_checker &peer_allowance_checker_;
	status_change_notifier &status_change_notifier_;

	user_data user_data_;

	std::deque<std::unique_ptr<fz::socket>> accepted_;
};


inline bool operator <(const listener &lhs, const listener &rhs)
{
	return lhs.get_address_info() < rhs.get_address_info();
}

inline bool operator <(const listener &lhs, const tcp::address_info &rhs)
{
	return lhs.get_address_info() < rhs;
}

inline bool operator <(const tcp::address_info &lhs, const listener &rhs)
{
	return lhs < rhs.get_address_info();
}

struct listeners_manager
{
	listeners_manager(thread_pool &pool,
					 event_loop &loop,
					 event_handler &target_handler,
					 logger_interface &logger,
					 const listener::peer_allowance_checker &pac = listener::peer_allowance_checker::allow_all,
					 listener::status_change_notifier &scn = listener::status_change_notifier::none);

	bool start();
	bool stop();
	bool is_running() const;

	template <typename It, typename Sentinel, typename UserDataFunc>
	auto set_address_infos(It begin, Sentinel end, const UserDataFunc &f) -> decltype(static_cast<const tcp::address_info&>(*begin), begin != end, void());

	template <typename It, typename Sentinel>
	auto set_address_infos(It begin, Sentinel end) -> decltype(static_cast<const tcp::address_info&>(*begin), begin != end, void());

private:
	mutable fz::mutex mutex_;

	thread_pool &pool_;
	event_loop &loop_;
	event_handler &target_handler_;
	logger_interface &logger_;
	const listener::peer_allowance_checker &peer_allowance_checker_;
	listener::status_change_notifier &status_change_notifier_;

	std::set<tcp::listener, std::less<void>> listeners_;
	bool is_running_{};
};

template <typename It, typename Sentinel, typename UserDataFunc>
auto listeners_manager::set_address_infos(It begin, Sentinel end, const UserDataFunc &f) -> decltype(static_cast<const tcp::address_info&>(*begin), begin != end, void())
{
	std::set<tcp::listener, std::less<void>> new_listeners;

	scoped_lock lock(mutex_);

	for (; begin != end; ++begin) {
		const auto &info = *begin;

		auto it = listeners_.find(info);

		listener::user_data user_data;
		if constexpr (!std::is_same_v<void, decltype(f(info))>)
			user_data = f(info);

		if (it != listeners_.end()) {
			auto &&node = listeners_.extract(it);

			node.value().get_user_data() = std::move(user_data);

			new_listeners.insert(std::move(node));
		}
		else {
			it = new_listeners.emplace(pool_, loop_, target_handler_, logger_, info, peer_allowance_checker_, status_change_notifier_, std::move(user_data)).first;

			if (is_running_) {
				auto &&node = new_listeners.extract(it);

				node.value().start();

				new_listeners.insert(std::move(node));
			}
		}
	}

	std::swap(new_listeners, listeners_);
}

template <typename It, typename Sentinel>
auto listeners_manager::set_address_infos(It begin, Sentinel end) -> decltype(static_cast<const tcp::address_info&>(*begin), begin != end, void())
{
	return set_address_infos(begin, end, [](const tcp::address_info &){});
}

}


#endif // FZ_TCP_LISTENER_HPP
