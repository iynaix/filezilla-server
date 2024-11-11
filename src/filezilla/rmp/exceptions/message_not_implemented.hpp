#ifndef FZ_RMP_EXCEPTION_MESSAGE_NOT_IMPLEMENTED_HPP
#define FZ_RMP_EXCEPTION_MESSAGE_NOT_IMPLEMENTED_HPP

#include <libfilezilla/format.hpp>

#include "generic.hpp"
#include "../command.hpp"

#include "../../util/demangle.hpp"

namespace fz::rmp::exception {

struct message_not_implemented: generic
{
	template <typename T, std::enable_if_t<trait::is_a_command_v<T>>* = nullptr>
	message_not_implemented(const T &)
		: generic(fz::sprintf("Command is not implemented: %s", util::type_name<T>()))
	{}

	template <typename T, std::enable_if_t<trait::is_a_message_v<T> && !trait::is_a_command_v<T>>* = nullptr>
	message_not_implemented(const T &)
		: generic(fz::sprintf("Message is not implemented: %s", util::type_name<T>()))
	{}

	template <typename T, std::enable_if_t<trait::is_a_command_response_v<T>>* = nullptr>
	message_not_implemented(const T &)
		: generic(fz::sprintf("Command response is not implemented: %s", util::type_name<T>()))
	{}

	message_not_implemented() = default;
};

}

#endif // FZ_RMP_EXCEPTIONS_MESSAGE_NOT_IMPLEMENTED_HPP
