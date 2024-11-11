#ifndef FZ_SERIALIZATION_ACCESS_HPP
#define FZ_SERIALIZATION_ACCESS_HPP

#include <utility>

#include "../serialization/version.hpp"

namespace fz::serialization
{
	struct access
	{
		template <typename T>
		constexpr static auto version() -> decltype(T::serialization_version) {
			return T::serialization_version;
		}

		// This is going to be defined in serialization.hpp
		template <typename Archive, typename T, typename... PostProcess>
		static Archive &process(Archive &ar, T && t, const PostProcess &... post_process);

		#define FZ_SERIALIZATION_AUTO_RETURN(...) -> decltype(__VA_ARGS__) { return __VA_ARGS__; }

		template <typename Derived, typename T>
		static auto member_serialize(Derived &d, T && t) FZ_SERIALIZATION_AUTO_RETURN(
			const_cast<std::decay_t<T>&>(t).serialize(d)
		)

		template <typename Derived, typename T>
		static auto member_save(Derived &d, T && t) FZ_SERIALIZATION_AUTO_RETURN(
			const_cast<std::decay_t<T>&>(t).save(d)
		)

		template <typename Derived, typename T>
		static auto member_save_minimal(const Derived &d, T && t) FZ_SERIALIZATION_AUTO_RETURN(
			const_cast<std::decay_t<T>&>(t).save_minimal(d)
		)

		template <typename Derived, typename T>
		static auto member_save_minimal(const Derived &d, T && t, typename Derived::error_t &error) FZ_SERIALIZATION_AUTO_RETURN(
			const_cast<std::decay_t<T>&>(t).save_minimal(d, error)
		)

		template <typename Derived, typename T>
		static auto member_load(Derived &d, T && t) FZ_SERIALIZATION_AUTO_RETURN(
			const_cast<std::decay_t<T>&>(t).load(d)
		)

		template <typename Derived, typename T, typename V>
		static auto member_load_minimal(const Derived &d, T && t, V && v) FZ_SERIALIZATION_AUTO_RETURN(
			const_cast<std::decay_t<T>&>(t).load_minimal(d, std::forward<V>(v))
		)

		template <typename Derived, typename T, typename V>
		static auto member_load_minimal(const Derived &d, T && t, V && v, typename Derived::error_t &error) FZ_SERIALIZATION_AUTO_RETURN(
			const_cast<std::decay_t<T>&>(t).load_minimal(d, std::forward<V>(v), error)
		)

		template <typename Derived, typename T>
		static auto member_serialize(Derived &d, T && t, const version_of_t<T> &version) FZ_SERIALIZATION_AUTO_RETURN(
			const_cast<std::decay_t<T>&>(t).serialize(d, version)
		)

		template <typename Derived, typename T>
		static auto member_save(Derived &d, T && t, const version_of_t<T> &version) FZ_SERIALIZATION_AUTO_RETURN(
			const_cast<std::decay_t<T>&>(t).save(d, version)
		)

		template <typename Derived, typename T>
		static auto member_save_minimal(const Derived &d, T && t, const version_of_t<T> &version) FZ_SERIALIZATION_AUTO_RETURN(
			const_cast<std::decay_t<T>&>(t).save_minimal(d, version)
		)

		template <typename Derived, typename T>
		static auto member_save_minimal(const Derived &d, T && t, typename Derived::error_t &error, const version_of_t<T> &version) FZ_SERIALIZATION_AUTO_RETURN(
			const_cast<std::decay_t<T>&>(t).save_minimal(d, error, version)
		)

		template <typename Derived, typename T>
		static auto member_load(Derived &d, T && t, const version_of_t<T> &version) FZ_SERIALIZATION_AUTO_RETURN(
			const_cast<std::decay_t<T>&>(t).load(d, version)
		)

		template <typename Derived, typename T, typename V>
		static auto member_load_minimal(const Derived &d, T && t, V && v, const version_of_t<T> &version) FZ_SERIALIZATION_AUTO_RETURN(
			const_cast<std::decay_t<T>&>(t).load_minimal(d, std::forward<V>(v), version)
		)

		template <typename Derived, typename T, typename V>
		static auto member_load_minimal(const Derived &d, T && t, V && v, typename Derived::error_t &error, const version_of_t<T> &version) FZ_SERIALIZATION_AUTO_RETURN (
			const_cast<std::decay_t<T>&>(t).load_minimal(d, std::forward<V>(v), error, version)
		)

		#undef FZ_SERIALIZATION_AUTO_RETURN
	};

	template <typename T>
	struct version_of<T, std::void_t<decltype(access::version<T>())>>
	{
		using type = decltype(access::version<T>());
		static const inline type value { access::version<T>() };
	};

}

#endif // ACCESS_HPP
