#ifndef FZ_TCP_TEMPORARY_ADDRESS_LIST_HPP
#define FZ_TCP_TEMPORARY_ADDRESS_LIST_HPP

#include <unordered_map>

#include <libfilezilla/time.hpp>
#include <libfilezilla/event_handler.hpp>
#include <libfilezilla/rwmutex.hpp>

#include "address_list.hpp"

namespace fz::tcp {

class temporary_address_list: private event_handler, public address_list {
public:
	temporary_address_list(event_loop &loop, address_list &list, duration default_expiration_duration = duration::from_milliseconds(100));
	~temporary_address_list() override;

	void set_expiration_duration(duration expiration_duration);

	bool contains(std::string_view address, address_type family) const override;
	bool add(std::string_view address, address_type family) override;
	bool remove(std::string_view address, address_type family) override;
	std::size_t size() const override;

	bool add(std::string_view address, duration expiration_duration, address_type family);

private:
	void operator()(const event_base &ev) override;

private:
	address_list &list_;
	duration default_expiration_duration_;

	std::unordered_map<timer_id, std::pair<std::string, address_type>> map_;

	fz::rwmutex mutex_;
};

}

#endif // TEMPORARY_ADDRESS_LIST_HPP
