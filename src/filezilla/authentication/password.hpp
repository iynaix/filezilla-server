#ifndef FZ_AUTHENTICATION_PASSWORD_HPP
#define FZ_AUTHENTICATION_PASSWORD_HPP

#include <variant>
#include <string_view>

#include "../serialization/helpers.hpp"

namespace fz::authentication {

namespace password {

class none {
public:
	bool verify(std::string_view) const {
		return true;
	}

	template <typename Archive>
	void serialize(Archive &) {}

	bool is_valid() const
	{
		return false;
	}
};

namespace pbkdf2 {
	class hmac_sha256 {
	public:
		/// Size in octets of key an salt.
		enum {
			hash_size = 32,
			salt_size = 32
		};

		enum: std::uint64_t {
			min_iterations = 100000
		};

		hmac_sha256() = default;
		hmac_sha256(std::string_view password);
		hmac_sha256(std::string_view password, std::basic_string_view<std::uint8_t> salt, std::uint64_t iterations = min_iterations);

		template <typename SaltContainer, std::enable_if_t<sizeof(typename SaltContainer::value_type) == sizeof(std::uint8_t)>* = nullptr>
		hmac_sha256(const std::string_view &password, const SaltContainer &salt, std::uint64_t iterations = min_iterations)
			: hmac_sha256(password, std::basic_string_view(reinterpret_cast<const uint8_t *>(salt.data()), salt.size()), iterations)
		{}

		bool verify(std::string_view password) const;

		template <typename Archive>
		void serialize(Archive &ar) {
			using serialization::nvp;
			using serialization::binary_data;

			ar(
				nvp(binary_data(hash_.data(), hash_.size()), "hash"),
				nvp(binary_data(salt_.data(), salt_.size()), "salt"),
				nvp(iterations_, "iterations")
			);
		}

		bool is_valid() const
		{
			return
				hash_ != std::array<uint8_t, hash_size>() &&
				salt_ != std::array<uint8_t, salt_size>() &&
				iterations_ != std::uint64_t();
		}

	private:
		std::array<uint8_t, hash_size> hash_{};
		std::array<uint8_t, salt_size> salt_{};
		std::uint64_t iterations_{};
	};
}

class md5 {
public:
	enum {
		hash_size = 16,
	};

	md5() = default;
	md5(std::string_view password);
	bool verify(std::string_view) const;

	template <typename Archive>
	void serialize(Archive &ar) {
		using serialization::nvp;
		using serialization::binary_data;

		ar(
			nvp(binary_data(hash_.data(), hash_.size()), "hash")
		);
	}

	static md5 from_hash(std::basic_string_view<uint8_t> hash);

	template <typename HashContainer,
		std::enable_if_t<
			sizeof(typename HashContainer::value_type) == sizeof(std::uint8_t)>* = nullptr>
	static md5 from_hash(const HashContainer &hash)
	{
		return from_hash(std::basic_string_view(reinterpret_cast<const uint8_t *>(hash.data()), hash.size()));
	}

	bool is_valid() const
	{
		return
			hash_ != std::array<uint8_t, hash_size>();
	}

private:
	std::array<uint8_t, hash_size> hash_{};
};

class sha512 {
public:
	enum {
		hash_size = 64,
		salt_size = 64
	};

	sha512() = default;
	sha512(std::string_view password);
	sha512(std::string_view password, std::basic_string_view<std::uint8_t> salt);

	template <typename SaltContainer,
		std::enable_if_t<
			sizeof(typename SaltContainer::value_type) == sizeof(std::uint8_t)>* = nullptr>
	sha512(const std::string_view &password, const SaltContainer &salt)
		: sha512(password, std::basic_string_view(reinterpret_cast<const uint8_t *>(salt.data()), salt.size()))
	{}

	bool verify(std::string_view password) const;

	template <typename Archive>
	void serialize(Archive &ar) {
		using serialization::nvp;
		using serialization::binary_data;

		ar(
			nvp(binary_data(hash_.data(), hash_.size()), "hash"),
			nvp(binary_data(salt_.data(), salt_.size()), "salt")
		);
	}

	static sha512 from_hash_and_salt(std::basic_string_view<uint8_t> hash, std::basic_string_view<std::uint8_t> salt);

	template <typename HashContainer, typename SaltContainer,
		std::enable_if_t<
			sizeof(typename HashContainer::value_type) == sizeof(std::uint8_t) &&
			sizeof(typename SaltContainer::value_type) == sizeof(std::uint8_t)>* = nullptr>
	static sha512 from_hash_and_salt(const HashContainer &hash, const SaltContainer &salt)
	{
		return from_hash_and_salt(
			std::basic_string_view(reinterpret_cast<const uint8_t *>(hash.data()), hash.size()),
			std::basic_string_view(reinterpret_cast<const uint8_t *>(salt.data()), salt.size())
		);
	}

	bool is_valid() const
	{
		return
			hash_ != std::array<uint8_t, hash_size>() &&
			salt_ != std::array<uint8_t, salt_size>();
	}

private:
	std::array<uint8_t, hash_size> hash_{};
	std::array<uint8_t, salt_size> salt_{};
};

}

using default_password = password::pbkdf2::hmac_sha256;

struct any_password: std::variant<
	password::none,
	password::pbkdf2::hmac_sha256,
	password::md5,    /// Just for backwards compatibility. You shouldn't use this in production code.
	password::sha512  /// Just for backwards compatibility. You shouldn't use this in production code.
>
{
	using variant::variant;

	template <typename T>
	auto is() -> decltype(std::get_if<T>(this)) {
		return std::get_if<T>(this);
	}

	template <typename T>
	auto is() const -> decltype(std::get_if<T>(this)) {
		return std::get_if<T>(this);
	}

	bool verify(std::string_view password) const {
		return std::visit([&password](const auto &p) { return p.verify(password); }, *static_cast<const variant*>(this));
	}

	constexpr bool has_password() const
	{
		return !std::holds_alternative<password::none>(static_cast<const variant &>(*this));
	}

	constexpr explicit operator bool() const
	{
		return has_password();
	}

	bool has_valid_password() const {
		return std::visit([](const auto &p) {
			return p.is_valid();
		}, *static_cast<const variant*>(this));
	}
};

}

#endif // PASSWORD_HPP
