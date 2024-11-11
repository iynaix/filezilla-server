#ifndef FZ_TCP_AUTOMATICALLY_SERIALIZABLE_BINARY_ADDRESS_LIST_IPP
#define FZ_TCP_AUTOMATICALLY_SERIALIZABLE_BINARY_ADDRESS_LIST_IPP

#include "automatically_serializable_binary_address_list.hpp"

#include "../serialization/archives/xml.hpp"
#include "../serialization/types/binary_address_list.hpp"

namespace fz::tcp {

automatically_serializable_binary_address_list::automatically_serializable_binary_address_list(event_loop &loop, binary_address_list& list, std::string listname, native_string filename, duration wait_time, event_handler *targer_handler)
	: list_(list)
	, delayed_archiver_(loop, list, listname, filename, wait_time, &mutex_, targer_handler)
{
}

bool automatically_serializable_binary_address_list::contains(std::string_view address, address_type family) const
{
	scoped_lock lock(mutex_);

	return list_.contains(address, family);
}

bool automatically_serializable_binary_address_list::add(std::string_view address, address_type family)
{
	scoped_lock lock(mutex_);

	if (list_.add(address, family)) {
		delayed_archiver_.save_later();

		return true;
	}

	return false;
}

bool automatically_serializable_binary_address_list::remove(std::string_view address, address_type family)
{
	scoped_lock lock(mutex_);

	if (list_.remove(address, family)) {
		delayed_archiver_.save_later();

		return true;
	}

	return false;
}

std::size_t automatically_serializable_binary_address_list::size() const
{
	return list_.size();
}

void automatically_serializable_binary_address_list::set_list(binary_address_list &&list, bool save)
{
	scoped_lock lock(mutex_);

	list_ = std::move(list);

	if (save)
		delayed_archiver_.save_later();
}

void automatically_serializable_binary_address_list::set_duration(duration wait_time)
{
	delayed_archiver_.set_saving_delay(wait_time);
}

typename automatically_serializable_binary_address_list::locked_list automatically_serializable_binary_address_list::lock() const
{
	return {*this};
}

serialization::xml_input_archive::error_t automatically_serializable_binary_address_list::load_into(binary_address_list &l)
{
	return delayed_archiver_.load_into(l);
}

bool automatically_serializable_binary_address_list::save(util::xml_archiver_base::event_dispatch_mode mode)
{
	return delayed_archiver_.save_now(mode) == 0;
}

automatically_serializable_binary_address_list::locked_list::locked_list(const automatically_serializable_binary_address_list &list)
	: owner_(list)
{
	owner_.mutex_.lock();
}

automatically_serializable_binary_address_list::locked_list::~locked_list()
{
	owner_.mutex_.unlock();
}

const binary_address_list &automatically_serializable_binary_address_list::locked_list::get_list() const
{
	return owner_.list_;
}

}

#endif // FZ_TCP_AUTOMATICALLY_SERIALIZABLE_BINARY_ADDRESS_LIST_IPP
