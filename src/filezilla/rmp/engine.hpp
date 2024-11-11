#ifndef FZ_RMP_ENGINE_HPP
#define FZ_RMP_ENGINE_HPP

#include "../rmp/any_message.hpp"
#include "../debug.hpp"

#ifndef ENABLE_FZ_RMP_DEBUG
#   define ENABLE_FZ_RMP_DEBUG 0
#endif

#define FZ_RMP_DEBUG_LOG FZ_DEBUG_LOG("rmp", ENABLE_FZ_RMP_DEBUG)

namespace fz::rmp {

template <typename AnyMessage>
struct engine
{
	using any_message = trait::access_any_message<AnyMessage>;

	class session;
	class server;
	class client;
	class access;
	class dispatcher;

	template <typename Implementation>
	class visitor;
};

#define FZ_RMP_USING_DIRECTIVES               \
	namespace exception = fz::rmp::exception; \
	using fz::rmp::message;                   \
	using fz::rmp::command;                   \
	using fz::rmp::response;                  \
	using fz::rmp::response_from_message;     \
	using fz::rmp::success;                   \
	using fz::rmp::failure;                   \
	using fz::rmp::version_t;                 \
	using fz::rmp::versioned;                 \
	using fz::serialization::size_type;       \
/***/

}

#endif // FZ_RMP_ENGINE_HPP
