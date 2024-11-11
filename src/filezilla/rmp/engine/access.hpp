#ifndef FZ_RMP_ENGINE_ACCESS_HPP
#define FZ_RMP_ENGINE_ACCESS_HPP

#include "../../rmp/engine/session.hpp"

namespace fz::rmp {

template <typename AnyMessage>
class engine<AnyMessage>::access {
public:
	class detail {
	public:
		friend class access;

		using no_implementation = std::integral_constant<int, 0>;
		using implementation_with_session = std::integral_constant<int, 1>;
		using implementation_without_session = std::integral_constant<int, 2>;
		using too_many_implementations = std::integral_constant<int, 3>;

		template <typename Implementation, typename T, typename = void>
		struct has_implementation_without_session: no_implementation{};

		template <typename Implementation, typename T>
		struct has_implementation_without_session<Implementation, T, std::void_t<decltype(std::declval<Implementation>()(std::declval<T>()))>>: implementation_without_session{};

		template <typename Implementation, typename T, typename = void>
		struct has_implementation_with_session: no_implementation{};

		template <typename Implementation, typename T>
		struct has_implementation_with_session<Implementation, T, std::void_t<decltype(std::declval<Implementation>()(std::declval<T>(), std::declval<session &>()))>>: implementation_with_session{};

		template <typename Implementation, typename T>
		using select_implementation = std::integral_constant<int,
			has_implementation_without_session<Implementation, T>::value |
			has_implementation_with_session<Implementation, T>::value
		>;

		template <typename Implementation, typename Message, typename Type = select_implementation<Implementation, Message>>
		struct dispatcher;

		template <typename Implementation, typename Message>
		struct dispatcher<Implementation, Message, implementation_without_session>
		{
			static void dispatch(Implementation &implementation, class session &s, Message &&m)
			{
				using return_type = decltype(implementation(std::move(m)));
				FZ_RMP_DEBUG_LOG(L"dispatch without session. Returns [%s]", util::type_name<return_type>());

				if constexpr (std::is_same_v<void, return_type>)
					return implementation(std::move(m));
				else
					s.send(implementation(std::move(m)));
			}
		};

		template <typename Implementation, typename Message>
		struct dispatcher<Implementation, Message, implementation_with_session>
		{
			static void dispatch(Implementation &implementation, class session &s, Message &&m)
			{
				using return_type = decltype(implementation(std::move(m), s));

				FZ_RMP_DEBUG_LOG(L"dispatch with session. Returns [%s]", util::type_name<return_type>());

				if constexpr (std::is_same_v<void, return_type>)
					return implementation(std::move(m), s);
				else
					s.send(implementation(std::move(m), s));
			}
		};

		template <typename Implementation, typename Message>
		struct dispatcher<Implementation, Message, no_implementation>
		{
			static void dispatch(Implementation &implementation, class session &s, Message &&m)
			{
				if constexpr (std::is_same_v<any_exception, Message>) {
					FZ_RMP_DEBUG_LOG(L"dispatch any_exception");

					std::move(m).handle(
						[&s, &implementation](auto && ex) {
							using T = std::decay_t<decltype(ex)>;
							detail::template dispatcher<Implementation, T>::dispatch(implementation, s, std::move(ex));
						}
					);
				}
				else
				if constexpr (std::is_base_of_v<exception::generic, Message>) {
					FZ_RMP_DEBUG_LOG(L"UNHANDLED EXCEPTION [%s]", util::type_name<Message>());
				}
				else {
					//static_assert(util::delayed_assert<Message>, "message_no_implementation");

					FZ_RMP_DEBUG_LOG(L"UNHANDLED MESSAGE", util::type_name<Message>());

					s.template send<any_exception>(rmp::exception::message_not_implemented(m));
				}
			}
		};

		template <typename Implementation, typename Message>
		struct dispatcher<Implementation, Message, too_many_implementations>
		{
			static_assert(util::delayed_assert<Message>, "There are too many implementations for the message");

			static void dispatch(Implementation &, class session &, Message &&)
			{
			}
		};
	};

	template <typename Implementation>
	friend class forwarder;

	template <typename Message, typename Implementation>
	static void load_and_dispatch(Implementation &i, session &s, serialization::binary_input_archive &l);
};

// This is defined out-of-class so that its explicit instantiation doesn't also trigger implicit instantiation in the place of use.
// Doing otherwise would inline the wrong code or, perhaps worse, violate ODR.
// See §17.7.2 [temp.expl] of N4659: https://timsong-cpp.github.io/cppwp/n4659/temp.explicit#10
template <typename AnyMessage>
template <typename Message, typename Implementation>
void engine<AnyMessage>::access::load_and_dispatch(Implementation &i, session &s, serialization::binary_input_archive &l)
{
	FZ_RMP_DEBUG_LOG(L"load_and_dispatch(%s)", util::type_name<Message>());

	if (Message m; l(serialization::nvp(m, "message"))) {
		#if ENABLE_FZ_SERIALIZATION_DEBUG
			fz::buffer b;
			{
				serialization::xml_output_archive::buffer_saver saver(b);
				serialization::xml_output_archive xml(saver);
				xml(serialization::nvp(m, "message"));
			}

			FZ_SERIALIZATION_DEBUG_LOG(to_wstring(std::string_view(reinterpret_cast<const char *>(b.get()), b.size())));
		#endif
		detail::template dispatcher<Implementation, Message>::dispatch(i, s, std::move(m));
	}
	else {
		FZ_RMP_DEBUG_LOG(L"access::load_and_dispatch(): deserialization failed. error: %d", (int)l.error());
	}
}


#define FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(Engine, Impl, Message) \
	template void Engine::access::load_and_dispatch<Message>(Impl &, Engine::session &, ::fz::serialization::binary_input_archive &)
/***/

#define FZ_RMP_INSTANTIATE_EXTERNALLY_DISPATCHING_FOR(Engine, Impl, Message) \
	extern FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(Engine, Impl, Message)
/***/

}

#endif // FZ_RMP_ENGINE_ACCESS_HPP
