#ifndef FZ_TCP_ADDRESS_LIST_HPP
#define FZ_TCP_ADDRESS_LIST_HPP

#include <string>
#include <libfilezilla/socket.hpp>

namespace fz::tcp {

class address_list {
public:
	virtual ~address_list() = default;

	virtual bool contains(std::string_view address, address_type family) const = 0;
	virtual bool add(std::string_view address, address_type family) = 0;
	virtual bool remove(std::string_view address, address_type family) = 0;
	virtual std::size_t size() const = 0;

	bool empty() const {
		return size() == 0;
	}
};

}



#endif // ADDRESS_LIST
