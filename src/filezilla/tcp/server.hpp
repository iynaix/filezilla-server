#ifndef FZ_TCP_SERVER_HPP
#define FZ_TCP_SERVER_HPP

#include <unordered_map>
#include <set>

#include <libfilezilla/event_handler.hpp>
#include <libfilezilla/logger.hpp>

#include "listener.hpp"
#include "session.hpp"

namespace fz::tcp {

class server: protected event_handler
{
public:
	class context;

	template <typename Derived>
	class delegate;

	server(server::context &context, logger_interface &logger, session::factory &session_factory);
	~server() override;

	bool start();
	bool stop(bool destroy_all_sessions = false);
	bool is_running() const;

	template <typename It, typename Sentinel, typename UserDataFunc>
	auto set_listen_address_infos(It begin, Sentinel end, const UserDataFunc &f) -> decltype(static_cast<const tcp::address_info&>(*begin), begin != end, void());

	template <typename It, typename Sentinel>
	auto set_listen_address_infos(It begin, Sentinel end) -> decltype(static_cast<const tcp::address_info&>(*begin), begin != end, void());

	//! Iterates over ALL the active peers.
	//! \param func is a functor that gets invoked over each of the iterated over peers. \return false to make the iteration stop.
	//! \returns the number of all iterated over peers.
	template <typename Func, std::enable_if_t<std::is_invocable_v<Func, session&>>* = nullptr>
	size_t iterate_over_sessions(Func && func);

	//! Iterates over the active peers.
	//! \param ids is the list of peers ids to use as a filter. If empty, then all peers are iterated over.
	//! \param func is a functor that gets invoked over each of the iterated over peers. \return false to make the iteration stop.
	//! \returns the number of all available peers, regardless of how many were iterated over.
	template <typename Func, std::enable_if_t<std::is_invocable_v<Func, session&>>* = nullptr>
	size_t iterate_over_sessions(const std::vector<session::id> &ids, Func && func);

	//! Disconnect the active sessions.
	//! \param ids is the list of sessions ids to use as a filter. If empty, then all sessions are disconnected.
	std::size_t end_sessions(const std::vector<session::id> &ids = {}, int err = 0);

	//! \returns the number of sessions currently active
	std::size_t get_number_of_sessions() const;

	//! \returns a locked proxy to a session with the specifid id, if it exists.
	//! Careful: the session list will be locked for as long as the returned locked proxy is alive.
	util::locked_proxy<session> get_session(session::id id);

private:
	void operator ()(const event_base &ev) override;
	void on_connected_event(tcp::listener &listener);
	void on_status_changed(const fz::tcp::listener &listener);
	void on_session_ended_event(session::id, const channel::error_type &);

	mutable fz::mutex mutex_;

	server::context &context_;
	logger_interface &logger_;
	session::factory &session_factory_;
	event_loop listeners_loop_;

	listeners_manager listeners_;

	std::unordered_map<session::id, std::unique_ptr<session>> sessions_{};
	std::atomic<std::size_t> num_sessions_{};
};

template <typename Func, std::enable_if_t<std::is_invocable_v<Func, session&>>*>
inline size_t server::iterate_over_sessions(Func && func)
{
	scoped_lock lock(mutex_);

	for (auto &[id, s]: sessions_)
		if (!func(*s))
			break;

	return sessions_.size();
}

template <typename Func, std::enable_if_t<std::is_invocable_v<Func, session&>>*>
inline size_t server::iterate_over_sessions(const std::vector<session::id> &ids, Func && func)
{
	scoped_lock lock(mutex_);

	// An empty list of peers ids means: iterate over all available peers
	if (ids.empty())
		return iterate_over_sessions(func);

	for (auto id: ids) {
		auto it = sessions_.find(id);
		if (it != sessions_.end())
			if (!func(*it->second))
				break;
	}

	return sessions_.size();
}

inline std::size_t server::get_number_of_sessions() const
{
	return num_sessions_;
}

template <typename It, typename Sentinel, typename UserDataFunc>
auto server::set_listen_address_infos(It begin, Sentinel end, const UserDataFunc &f) -> decltype(static_cast<const tcp::address_info&>(*begin), begin != end, void())
{
	return listeners_.set_address_infos(begin, end, f);
}

template <typename It, typename Sentinel>
auto server::set_listen_address_infos(It begin, Sentinel end) -> decltype(static_cast<const tcp::address_info&>(*begin), begin != end, void())
{
	return listeners_.set_address_infos(begin, end);
}

namespace trait {

template <typename T, typename = void>
struct has_get_user_data: std::false_type{};

template <typename T>
struct has_get_user_data<T, std::void_t<decltype(std::declval<T>().get_user_data())>>: std::true_type{};

}

class server::context {
public:
	context(thread_pool &pool, event_loop &loop) : pool_(pool), loop_(loop) {}

	thread_pool& pool() const { return pool_; }
	event_loop& loop() const { return loop_; }

	session::id next_session_id() {
		scoped_lock lock(mutex_);
		return ++last_session_id_;
	}

private:
	thread_pool &pool_;
	event_loop &loop_;
	session::id last_session_id_{};
	fz::mutex mutex_;
};


template <typename Derived>
class server::delegate
{
public:
	delegate(tcp::server &server)
		: server_(server)
	{
		using Session = typename Derived::session;
		using AddressInfo = typename Derived::address_info;

		static_assert(std::is_base_of_v<delegate, Derived>, "Derived must be derived from tcp::server::delegate<Derived>");
		static_assert(std::is_base_of_v<session, Session>, "Derived::session must derive from tcp::session");
		static_assert(std::is_base_of_v<address_info, AddressInfo>, "Derived::address_info must derive from tcp::address_info");
	}

	bool start()
	{
		return server_.start();
	}

	bool stop(bool destroy_all_sessions = false)
	{
		return server_.stop(destroy_all_sessions);
	}

	bool is_running() const
	{
		return server_.is_running();
	}

	template <typename It, typename Sentinel, typename D = Derived, typename AddressInfo = typename D::address_info>
	auto set_listen_address_infos(It begin, Sentinel end) -> decltype(static_cast<const AddressInfo&>(*begin), begin != end, void())
	{
		if constexpr (trait::has_get_user_data<AddressInfo>::value) {
			return server_.set_listen_address_infos(begin, end, [](const tcp::address_info &ai) {
				return static_cast<const AddressInfo&>(ai).get_user_data();
			});
		}
		else {
			return server_.set_listen_address_infos(begin, end);
		}
	}

	template <typename D = Derived, typename AddressInfo = typename D::address_info, typename Range = std::initializer_list<AddressInfo>>
	auto set_listen_address_infos(const Range &r) -> decltype(static_cast<const AddressInfo&>(*std::begin(r)), std::begin(r) != std::end(r), void())
	{
		return this->set_listen_address_infos(std::begin(r), std::end(r));
	}

	//! Iterates over ALL the active peers.
	//! \param func is a functor that gets invoked over each of the iterated over peers. \return false to make the iteration stop.
	//! \returns the number of all iterated over peers.
	template <typename Func, typename D = Derived, typename Session = typename D::session, std::enable_if_t<std::is_invocable_v<Func, Session&>>* = nullptr>
	size_t iterate_over_sessions(Func && func)
	{
		return server_.iterate_over_sessions([&func](tcp::session &tcp_session) {
			auto &s = static_cast<Session &>(tcp_session);
			return func(s);
		});
	}

	//! Iterates over the active peers.
	//! \param ids is the list of peers ids to use as a filter. If empty, then all peers are iterated over.
	//! \param func is a functor that gets invoked over each of the iterated over peers. \return false to make the iteration stop.
	//! \returns the number of all available peers, regardless of how many were iterated over.
	template <typename Func, typename D = Derived, typename Session = typename D::session, std::enable_if_t<std::is_invocable_v<Func, Session&>>* = nullptr>
	size_t iterate_over_sessions(const std::vector<session::id> &ids, Func && func)
	{
		return server_.iterate_over_sessions(ids, [&func](tcp::session &tcp_session) {
			auto &s = static_cast<Session &>(tcp_session);
			return func(s);
		});
	}

	//! Disconnect the active sessions.
	//! \param ids is the list of sessions ids to use as a filter. If empty, then all sessions are disconnected.
	std::size_t end_sessions(const std::vector<session::id> &ids = {}, int err = 0)
	{
		return server_.end_sessions(ids, err);
	}

	//! \returns the number of sessions currently active
	std::size_t get_number_of_sessions() const
	{
		return server_.get_number_of_sessions();
	}

	//! \returns a locked proxy to a session with the specifid id, if it exists.
	//! Careful: the session list will be locked for as long as the returned locked proxy is alive.
	template <typename D = Derived, typename Session = typename D::session>
	util::locked_proxy<Session> get_session(session::id id)
	{
		return util::static_locked_proxy_cast<Session>(server_.get_session(id));
	}

private:
	tcp::server &server_;
};


}

#endif // FZ_TCP_SERVER_HPP
