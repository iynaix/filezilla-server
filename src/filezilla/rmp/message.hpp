#ifndef FZ_RMP_MESSAGE_HPP
#define FZ_RMP_MESSAGE_HPP

#include <variant>
#include <tuple>

#include "../util/traits.hpp"
#include "../serialization/types/tuple.hpp"
#include "../serialization/helpers.hpp"

#include "version.hpp"
#include "../build_info.hpp"

namespace fz::rmp {

template <std::underlying_type_t<version_t> Number, typename Tag>
struct versioned: std::integral_constant<std::underlying_type_t<version_t>, Number>{};

template <typename Signature>
struct message;

struct message_version
{
	message_version() = default;

private:
	template <typename Signature>
	friend struct message;

	friend serialization::access;

	constexpr message_version(std::underlying_type_t<version_t> value)
		: value_{value}
	{}

	template <typename Archive>
	void serialize(Archive &ar)
	{
		ar(flavour_, value_);
	}

	build_info::flavour_type flavour_{build_info::flavour};
	version_t value_{};
};

template <typename Tag, typename ...Ts>
struct message<Tag(Ts...)>
{
	using tuple_type = std::tuple<Ts...>;

	template <typename... Us, std::enable_if_t<std::is_constructible_v<tuple_type, Us...>>* = nullptr>
	message(Us &&... us)
		: v_(std::forward<Us>(us)...)
	{}

	message() = default;

	message(message &&) = default;
	message(const message &) = default;

	message &operator=(message &&) = default;
	message &operator=(const message &) = default;

	const std::tuple<Ts...> & tuple() const & { return v_;	}
	std::tuple<Ts...> & tuple() & { return v_; }
	std::tuple<Ts...> && tuple() && { return std::move(v_);	}

	template <std::size_t I>
	friend const auto &get(const message &m) { return std::get<I>(m.v_);	}

	template <std::size_t I>
	friend auto & get(message &m) { return std::get<I>(m.v_); }

	template <std::size_t I>
	friend auto && get(message &&m) { return std::get<I>(std::move(m.v_));	}

	tuple_type v_;

	template <typename Archive>
	void serialize(Archive &ar)
	{
		ar(serialization::nvp(v_, ""));
	}
};

template <std::underlying_type_t<version_t> Number, typename Tag, typename ...Ts>
struct message<versioned<Number, Tag>(Ts...)>: message<Tag(Ts...)>
{
	using message<Tag(Ts...)>::message;
	using message<Tag(Ts...)>::operator=;

	static const inline message_version serialization_version { Number };

	template <typename Archive>
	void serialize(Archive &ar, const message_version &version)
	{
		if (version.value_ != Number || version.flavour_ != build_info::flavour)
			ar.error(EBADMSG);
		else
			ar(serialization::nvp(this->v_, ""));
	}
};

FZ_UTIL_TRAITS_MAKE_ACCESS_TEMPLATE(message)
FZ_UTIL_TRAITS_MAKE_ACCESS_TEMPLATE_WITH_NAME_AND_ARGS(versioned_message, message, (std::underlying_type_t<version_t> Number, typename Tag, typename ...Ts), (versioned<Number, Tag>(Ts...)))

}

namespace std
{

	template <typename Tag, typename ...Ts>
	struct tuple_size<::fz::rmp::message<Tag(Ts...)>>: tuple_size<std::tuple<Ts...>>{};

	template <std::size_t I, typename Tag, typename ...Ts>
	struct tuple_element<I, ::fz::rmp::message<Tag(Ts...)>>: tuple_element<I, std::tuple<Ts...>>{};

}

namespace fz::serialization {

	template <typename Archive, typename T>
	struct specialize<
		Archive,
		T,
		specialization::versioned_member_serialize,
		std::enable_if_t<rmp::trait::is_a_versioned_message_v<T>>
	>{};

}

#endif // FZ_RMP_MESSAGE_HPP
