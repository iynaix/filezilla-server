#ifndef FZ_LOGGER_HIERARCHICAL_HPP
#define FZ_LOGGER_HIERARCHICAL_HPP

#include <libfilezilla/logger.hpp>
#include <libfilezilla/mutex.hpp>

#include "../intrusive_list.hpp"

namespace fz::logger {

class hierarchical_interface;

class hierarchical_interface: public fz::logger_interface, public intrusive_node
{
public:
	void set_all(logmsg::type t) override;
	void enable(logmsg::type t) override;
	void disable(logmsg::type t) override;

	hierarchical_interface();
	hierarchical_interface(fz::logger_interface &);
	hierarchical_interface(hierarchical_interface &);
	~hierarchical_interface() override;

	hierarchical_interface(const hierarchical_interface &) = delete;
	hierarchical_interface(hierarchical_interface &&) = delete;
	hierarchical_interface &operator=(const hierarchical_interface &) = delete;
	hierarchical_interface &operator=(hierarchical_interface &&) = delete;

	template <typename Func>
	void iterate_over_children(const Func &);

	template <typename Func>
	void iterate_over_ancestors(const Func &);

	template <typename T>
	std::enable_if_t<std::is_base_of_v<hierarchical_interface, T>, T*>
	find_first_ancestor_of_type();

private:
	mutable fz::mutex mutex_;

	hierarchical_interface *const parent_;
	intrusive_list<hierarchical_interface> children_;
};


template <typename Func>
void hierarchical_interface::iterate_over_children(const Func &f)
{
	fz::scoped_lock lock(mutex_);
	for (auto &cur: children_)
		if (!f(cur))
			break;
}

template <typename Func>
void hierarchical_interface::iterate_over_ancestors(const Func &f)
{
	for (auto cur = parent_; cur; cur = cur->parent_) {
		if (!f(*cur))
			break;
	}
}

template <typename T>
std::enable_if_t<std::is_base_of_v<hierarchical_interface, T>, T*>
hierarchical_interface::find_first_ancestor_of_type()
{
	T *ret{};

	iterate_over_ancestors([&ret](hierarchical_interface &h) {
		return (ret = dynamic_cast<T*>(&h)) == nullptr;
	});

	return ret;
}

}

#endif // FZ_LOGGER_HIERARCHICAL_HPP
