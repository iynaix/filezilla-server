#ifndef FZ_RECEIVER_CONTEXT_HPP
#define FZ_RECEIVER_CONTEXT_HPP

#include <libfilezilla/event_handler.hpp>

#include "../shared_context.hpp"
#include "../intrusive_list.hpp"

namespace fz {


class receiver_context
{
public:
	struct managed_object: private virtual_intrusive_node
	{
		friend receiver_context;
	};

	~receiver_context();
	receiver_context(event_handler &eh)
		: eh(eh)
	{}

	template <typename T, typename... Args>
	std::enable_if_t<
		std::is_base_of_v<managed_object, T>,
	T *> create_managed(Args &&... args)
	{
		T *ret = new T(std::forward<Args>(args)...);
		managed_objects_.push_back(static_cast<virtual_intrusive_node&>(*ret));
		return ret;
	}

	void destroy_managed(managed_object &o)
	{
		o.remove();
		delete &o;
	}

public:
	event_handler &eh;

private:
	intrusive_list<virtual_intrusive_node> managed_objects_{};
};

using shared_receiver_context = shared_context<receiver_context>;

}

#endif // FZ_RECEIVER_CONTEXT_HPP
