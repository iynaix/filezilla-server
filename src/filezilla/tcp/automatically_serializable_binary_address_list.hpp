#ifndef FZ_TCP_AUTOMATICALLY_SERIALIZABLE_BINARY_ADDRESS_LIST_HPP
#define FZ_TCP_AUTOMATICALLY_SERIALIZABLE_BINARY_ADDRESS_LIST_HPP

#include <libfilezilla/event_handler.hpp>
#include <libfilezilla/mutex.hpp>

#include "binary_address_list.hpp"

#include "../hostaddress.hpp"
#include "../serialization/types/binary_address_list.hpp"
#include "../util/xml_archiver.hpp"

namespace fz::tcp {

class automatically_serializable_binary_address_list: public address_list {
	struct locked_list {
		locked_list(const automatically_serializable_binary_address_list &owner);
		~locked_list();

		const binary_address_list &get_list() const;

	private:
		const automatically_serializable_binary_address_list &owner_;
	};

public:
	automatically_serializable_binary_address_list(event_loop &loop, binary_address_list& list, std::string listname, native_string filename, duration wait_time, event_handler *target_handler = nullptr);

public:
	bool contains(std::string_view address, address_type family) const override;
	bool add(std::string_view address, address_type family) override;
	bool remove(std::string_view address, address_type family) override;
	std::size_t size() const override;

	void set_list(binary_address_list &&list, bool save = true);
	void set_duration(duration wait_time);

	locked_list lock() const;
	serialization::xml_input_archive::error_t load_into(binary_address_list &l);

	bool save(util::xml_archiver_base::event_dispatch_mode mode);

private:
	mutable fz::mutex mutex_;

	binary_address_list &list_;
	util::xml_archiver<binary_address_list> delayed_archiver_;

};

}

#endif // FZ_TCP_SERIALIZABLE_BINARY_ADDRESS_LIST_HPP
