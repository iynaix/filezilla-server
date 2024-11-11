#ifndef FZ_AUTHENTICATION_USER_HPP
#define FZ_AUTHENTICATION_USER_HPP

#include <string>
#include <unordered_set>

#include <libfilezilla/rate_limiter.hpp>
#include <libfilezilla/impersonation.hpp>

#include "../util/locking_wrapper.hpp"
#include "../impersonator/client.hpp"
#include "../tvfs/mount.hpp"
#include "../tvfs/limits.hpp"
#include "../util/copies_counter.hpp"

namespace fz::impersonator {
	class client;
}

namespace fz::authentication {

class user {
public:
	std::string id{};
	std::string name{};
	std::shared_ptr<tvfs::mount_tree> mount_tree{};
	std::shared_ptr<impersonator::client> impersonator{};
	std::shared_ptr<rate_limiter> limiter;
	std::vector<std::shared_ptr<rate_limiter>> extra_limiters{}; ///< sorted in ascending order
	rate::type session_inbound_limit{rate::unlimited};
	rate::type session_outbound_limit{rate::unlimited};
	tvfs::open_limits session_open_limits{};
	util::limited_copies_counter session_count_limiter;
	std::vector<std::shared_ptr<util::limited_copies_counter>> extra_session_count_limiters;

	user(const user &) = default;
	user(user &&) = default;
	user &operator=(const user &) = default;
	user &operator=(user &&) = default;

	explicit user(const std::string &id, const std::string &name)
		: id(id)
		, name(name)
	{}

	native_string home_dir() const;

	const impersonation_token &get_impersonation_token() const;
};

using shared_user = std::shared_ptr<util::locking_wrapper_interface<user>>;
using weak_user = std::weak_ptr<util::locking_wrapper_interface<user>>;

struct shared_user_deleter: std::default_delete<shared_user::element_type> {
	using default_delete::default_delete;

	std::unordered_set<event_handler *> handlers_;
	std::size_t notifications_count_{};
};

template <typename T, typename... Args>
[[nodiscard]] std::enable_if_t<std::is_base_of_v<shared_user::element_type, T>, shared_user>
make_shared_user(Args &&... args)
{
	return std::shared_ptr<T>(new T(std::forward<Args>(args)...), shared_user_deleter());
}

struct shared_user_changed_event_tag{};
using shared_user_changed_event = simple_event<shared_user_changed_event_tag, weak_user>;

bool subscribe(shared_user &, event_handler &);
bool unsubscribe(shared_user &, event_handler &);
bool notify(shared_user &);
std::size_t notifications_count(shared_user &);
std::size_t number_of_subscribers(shared_user &su);

}

#endif // FZ_AUTHENTICATION_USER_HPP
