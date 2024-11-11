#ifndef FZ_RMP_COMMAND_HPP
#define FZ_RMP_COMMAND_HPP

#include "../serialization/trait.hpp"
#include "../serialization/types/expected.hpp"
#include "../rmp/message.hpp"
#include "../expected.hpp"

namespace fz::rmp {

struct response;
struct success;
struct failure;
struct response_from_message;

template <typename Signature, typename Response>
struct command;

template <typename Response>
struct command_response;

template <typename Tag>
struct response_success;

template <typename Tag>
struct response_failure;

template <typename Tag, typename ...Us>
struct command_response<Tag (Us...)>: expected<message<response_success<Tag> (Us...)>, std::monostate>
{
	using command_response::expected::expected;
	using command_response::expected::operator=;

	constexpr bool is_success() const noexcept {
		return this->has_value();
	}
};

template <typename Tag, typename ...Us, typename ...Xs>
struct command_response<Tag (success(Us...), failure(Xs...))>: expected<message<response_success<Tag> (Us...)>, message<response_failure<Tag> (Xs...)>>
{
	using command_response::expected::expected;
	using command_response::expected::operator=;

	constexpr bool is_success() const noexcept {
		return this->has_value();
	}
};

template <std::underlying_type_t<version_t> Number, typename Tag, typename ...Us>
struct command_response<versioned<Number, Tag>(Us...)>: command_response<Tag (Us...)>
{
	using command_response<Tag (Us...)>::command_response;

	static constexpr version_t serialization_version { Number };

	template <typename Archive>
	void serialize(Archive &ar, version_t version)
	{
		if (version != serialization_version)
			ar.error(EBADMSG);
		else
			ar(serialization::base_class<command_response<Tag (Us...)>>(this));
	}
};

template <typename Tag, typename ...Ts, typename ...Us>
struct command<Tag (Ts...), response (Us...)>: message<Tag (Ts...)>
{
	using message<Tag (Ts...)>::message;
	using message<Tag (Ts...)>::operator=;

	using response = command_response<Tag (Us...)>;

	static response failure()
	{
		return { unexpect };
	}

	template <typename ...Xs>
	std::enable_if_t<sizeof...(Xs) == sizeof...(Us), response>
	static success(Xs &&... xs)
	{
		return { std::in_place, std::forward<Xs>(xs)... };
	}
};

template <typename Tag, typename ...Ts, typename Message>
struct command<Tag (Ts...), response_from_message (Message)>: message<Tag (Ts...)>
{
	using message<Tag (Ts...)>::message;
	using message<Tag (Ts...)>::operator=;

	using response = Message;
};

template <typename Tag, typename ...Ts, typename ...Us, typename ...Xs>
struct command<Tag (Ts...), response (success (Us...), failure (Xs...))>: message<Tag (Ts...)>
{
	using message<Tag (Ts...)>::message;
	using message<Tag (Ts...)>::operator=;

	using response = command_response<Tag (success (Us...), failure (Xs...))>;

	template <typename ...Fs>
	auto static failure(Fs &&... fs) -> decltype(response(unexpect, std::forward<Fs>(fs)...))
	{
		return { unexpect, std::forward<Fs>(fs)... };
	}

	template <typename ...Ss>
	auto static success(Ss &&... ss) -> decltype(response(std::in_place, std::forward<Ss>(ss)...))
	{
		return { std::in_place, std::forward<Ss>(ss)... };
	}
};

FZ_UTIL_TRAITS_MAKE_ACCESS_TEMPLATE(command)
FZ_UTIL_TRAITS_MAKE_ACCESS_TEMPLATE(command_response)
FZ_UTIL_TRAITS_MAKE_ACCESS_TEMPLATE_WITH_NAME_AND_ARGS(versioned_command_response, command_response, (std::underlying_type_t<version_t> Number, typename Tag, typename ...Ts), (versioned<Number, Tag>(Ts...)))


} // namespace fz::rmp

namespace fz::serialization
{

template <typename Archive, typename T>
struct specialize<
	Archive,
	T,
	fz::serialization::specialization::versioned_member_serialize,
	std::enable_if_t<rmp::trait::is_a_command_response_v<T> && rmp::trait::is_a_versioned_command_response_v<T>>
>{};

}

namespace std
{

template <typename Tag, typename ...Us, typename ...Ts>
struct tuple_size<::fz::rmp::command<Tag(Ts...), ::fz::rmp::response(Us...)>>: tuple_size<std::tuple<Ts...>>{};

template <std::size_t I, typename Tag, typename ...Us, typename ...Ts>
struct tuple_element<I, ::fz::rmp::command<Tag(Ts...), ::fz::rmp::response(Us...)>>: tuple_element<I, std::tuple<Ts...>>{};

template <typename Tag, typename ...Us>
struct tuple_size<::fz::rmp::command_response<Tag (Us...)>>: tuple_size<std::tuple<Us...>>{};

template <std::size_t I, typename Tag, typename ...Us>
struct tuple_element<I, ::fz::rmp::command_response<Tag (Us...)>>: tuple_element<I, std::tuple<Us...>>{};

}


#endif // FZ_RMP_COMMAND_HPP
