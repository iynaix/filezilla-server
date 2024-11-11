#ifndef FZ_RMP_ANY_MESSAGE_HPP
#define FZ_RMP_ANY_MESSAGE_HPP

#include <type_traits>

#include "message.hpp"
#include "command.hpp"
#include "any_exception.hpp"

namespace fz::rmp {

template <typename... Messages>
struct any_message: std::variant<any_exception, Messages...> {
	using messages = typename any_message::variant;

	static_assert(
		((trait::is_a_message_v<Messages> || trait::is_a_command_v<Messages> || trait::is_a_command_response_v<Messages>) && ...),
		"Template arguments must be messages, commands or command responses"
	);

	using variant = std::variant<any_exception, Messages...>;
	using variant::variant;

	static std::size_t constexpr size()
	{
		return std::variant_size_v<variant>;
	}

	template <typename T>
	static std::size_t constexpr type_index()
	{
		return util::type_index_v<T, variant>;
	}

	template <typename Func>
	auto visit(Func && f) &&
	{
		variant && v = std::move(*this);
		return std::visit(std::forward<Func>(f), std::move(v));
	}

	template <typename Func>
	auto visit(Func && f) &
	{
		return std::visit(std::forward<Func>(f), static_cast<variant &>(*this));
	}

	template <typename Func>
	auto visit(Func && f) const &
	{
		return std::visit(std::forward<Func>(f), static_cast<const variant &>(*this));
	}

private:
	friend serialization::access;

	static constexpr version_t serialization_version { rmp::version };

	template <typename Archive>
	void serialize(Archive &ar, serialization::version_t version)
	{
		if (version != serialization_version)
			ar.error(-EBADMSG);
		else
			ar(serialization::base_class<variant>(this));
	}
};

FZ_UTIL_TRAITS_MAKE_ACCESS_TEMPLATE(any_message)

namespace trait {

template <typename Any, typename Message>
struct has_message: std::conjunction<is_a_any_message<Any>, util::has_type<Message, typename Any::variant>>{};

template <typename Any, typename Message>
inline constexpr bool has_message_v = has_message<Any, Message>::value;

}

}

namespace fz::serialization {

template <typename Archive, typename... Ts>
struct specialize<
	Archive,
	fz::rmp::any_message<Ts...>,
	specialization::versioned_member_serialize
>{};

}

#endif // FZ_RMP_ANY_MESSAGE_HPP
