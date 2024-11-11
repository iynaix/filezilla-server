#ifndef FZ_SERIALIZATION_DETAILS_BASES_HPP
#define FZ_SERIALIZATION_DETAILS_BASES_HPP

#include <cstdint>
#include <utility>
#include <cstring>
#include <variant>

#include "../../serialization/access.hpp"
#include "../../serialization/trait.hpp"
#include "../../serialization/helpers.hpp"

#include "../../debug.hpp"

#ifndef ENABLE_FZ_SERIALIZATION_DEBUG
#   define ENABLE_FZ_SERIALIZATION_DEBUG 0
#endif

#define FZ_SERIALIZATION_DEBUG_LOG FZ_DEBUG_LOG("serialization", ENABLE_FZ_SERIALIZATION_DEBUG)

namespace fz::serialization::detail
{
	template <typename Archive>
	struct archive_error {
		using type = int;
	};

	template <typename Suberror>
	struct archive_error_with_suberror {
		using suberror_type = Suberror;

		archive_error_with_suberror(int error = {}, Suberror suberror = {})
			: suberror_(std::move(suberror))
			, error_{error}
		{}

		archive_error_with_suberror(const archive_error_with_suberror &rhs)
			: suberror_(rhs.suberror_)
			, error_(rhs.error_)
		{}

		archive_error_with_suberror(archive_error_with_suberror && rhs)
			: suberror_(std::move(rhs.suberror_))
			, error_(std::move(rhs.error_))
		{}

		archive_error_with_suberror &operator=(const archive_error_with_suberror &rhs) {
			suberror_ = rhs.suberror_;
			error_ = rhs.error_;
			return *this;
		}

		archive_error_with_suberror &operator=(archive_error_with_suberror &&rhs) {
			suberror_ = std::move(rhs.suberror_);
			error_ = std::move(rhs.error_);
			return *this;
		}

		operator int() const noexcept {
			return error_;
		}

		int value() const noexcept {
			return error_;
		}

		const Suberror& suberror() const& {
			return suberror_;
		}

		Suberror& suberror() & {
			return suberror_;
		}

		Suberror&& suberror() && {
			return std::move(suberror_);
		}

	protected:
		Suberror suberror_;
		int error_;
	};

	template <typename Derived>
	class archive_base {
	public:
		using error_t = typename archive_error<Derived>::type;

		static_assert(std::is_convertible_v<error_t, int>, "The archive error type must be implictly convertible to int.");
		static_assert(std::is_constructible_v<error_t, int>, "The archive error type must be constructible from int.");

		template <typename ...T>
		Derived &operator()(T &&... t)
		{
			++nesting_level_;
			(void)(access::process(derived(), std::forward<T>(t)) && ...);
			--nesting_level_;

			return derived();
		}

		template <typename ...NVPs, trait::enable_if<(trait::is_minimal_nvp_v<Derived, std::decay_t<NVPs>> &&...)> = trait::sfinae>
		Derived &attributes(NVPs &&... nvps){
			return (*this)(serialization::attribute{std::forward<NVPs>(nvps)}...);
		}

		template <typename T>
		auto attribute(T && t, const char *name) -> decltype(attributes(nvp(std::forward<T>(t), name)))
		{
			return attributes(nvp(std::forward<T>(t), name));
		}

		template <typename ...NVPs, trait::enable_if<(trait::is_minimal_optional_nvp_v<Derived, std::decay_t<NVPs>> &&...)> = trait::sfinae>
		Derived &optional_attributes(NVPs &&... nvps){
			return (*this)(serialization::optional_attribute{std::forward<NVPs>(nvps)}...);
		}

		template <typename T>
		auto optional_attribute(T && t, const char *name) -> decltype(optional_attributes(optional_nvp(std::forward<T>(t), name)))
		{
			return optional_attributes(optional_nvp(std::forward<T>(t), name));
		}

		explicit operator bool() const noexcept {
			return !error_;
		}

		const error_t &error() const noexcept {
			return error_;
		}

		void error(error_t error) noexcept {
			error_ = std::move(error);
			error_nesting_level_ = nesting_level_;
		}

		error_t && error(error_t && error) const noexcept {
			return std::move(error);
		}

		std::size_t error_nesting_level() const noexcept {
			return error_nesting_level_;
		}

		std::size_t nesting_level() const noexcept {
			return nesting_level_;
		}

	protected:
		Derived &derived() {
			return static_cast<Derived&>(*this);
		}

		static std::uint8_t is_little_endian() {
			std::uint16_t flag{1};

			return *((std::uint8_t *)&flag);
		}

		template <typename T>
		Derived &process_version(const T&, version_of_t<T> &v) {
			if constexpr (trait::has_minimal_serialization_v<Derived, version_of_t<T>>)
				return access::process(this->derived(), serialization::optional_attribute(v, "fz:version"));
			else
				return access::process(this->derived(), serialization::optional_nvp(v, "fz:version"));
		}

		archive_base() = default;
		archive_base(const archive_base &) = delete;
		archive_base(archive_base &&) = delete;

		error_t error_ {0};
		std::size_t error_nesting_level_{0};
		std::size_t nesting_level_{0};
	};
}

#endif // FZ_SERIALIZATION_DETAILS_BASES_HPP
