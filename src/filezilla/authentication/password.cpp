#include <libfilezilla/hash.hpp>
#include <libfilezilla/util.hpp>

#include "password.hpp"

fz::authentication::password::pbkdf2::hmac_sha256::hmac_sha256(std::string_view password, std::basic_string_view<uint8_t> salt, uint64_t iterations)
{
	if (iterations > std::numeric_limits<unsigned>::max())
		return;

	if (salt.size() != salt_.size())
		return;

	const auto &hash = fz::pbkdf2_hmac_sha256(password, salt, hash_size, static_cast<unsigned>(iterations));
	if (hash.size() != hash_.size())
		return;

	std::copy(hash.begin(), hash.end(), hash_.begin());
	std::copy(salt.begin(), salt.end(), salt_.begin());
	iterations_ = iterations;
}

fz::authentication::password::pbkdf2::hmac_sha256::hmac_sha256(std::string_view password)
	: hmac_sha256(password, fz::random_bytes(salt_size))
{}

bool fz::authentication::password::pbkdf2::hmac_sha256::verify(std::string_view password) const
{
	return fz::equal_consttime(hash_, fz::pbkdf2_hmac_sha256(password, salt_, hash_size, static_cast<unsigned>(iterations_))) && iterations_ >= min_iterations;
}

fz::authentication::password::md5::md5(std::string_view password)
{
	hash_accumulator acc(hash_algorithm::md5);
	acc.update(password);

	const auto &hash = acc.digest();
	if (hash.size() != hash_.size())
		return;

	std::copy(hash.begin(), hash.end(), hash_.begin());
}

bool fz::authentication::password::md5::verify(std::string_view password) const
{
	return fz::equal_consttime(hash_, md5(password).hash_);
}

fz::authentication::password::md5 fz::authentication::password::md5::from_hash(std::basic_string_view<uint8_t> hash)
{
	md5 res;

	if (hash.size() == res.hash_.size())
		std::copy(hash.begin(), hash.end(), res.hash_.begin());

	return res;
}

fz::authentication::password::sha512::sha512(std::string_view password, std::basic_string_view<uint8_t> salt)
{
	if (salt.size() != salt_.size())
		return;

	hash_accumulator acc(hash_algorithm::sha512);
	acc.update(password);
	acc.update(salt);

	const auto &hash = acc.digest();
	if (hash.size() != hash_.size())
		return;

	std::copy(hash.begin(), hash.end(), hash_.begin());
	std::copy(salt.begin(), salt.end(), salt_.begin());
}

fz::authentication::password::sha512::sha512(std::string_view password)
	: sha512(password, fz::random_bytes(salt_size))
{}

bool fz::authentication::password::sha512::verify(std::string_view password) const
{
	hash_accumulator acc(hash_algorithm::sha512);
	acc.update(password);
	acc.update(std::basic_string_view<uint8_t>(salt_.data(), salt_.size()));

	return fz::equal_consttime(hash_, acc.digest());
}

fz::authentication::password::sha512 fz::authentication::password::sha512::from_hash_and_salt(std::basic_string_view<uint8_t> hash, std::basic_string_view<uint8_t> salt)
{
	sha512 res;

	if (hash.size() == res.hash_.size() && salt.size() == res.salt_.size()) {
		std::copy(hash.begin(), hash.end(), res.hash_.begin());
		std::copy(salt.begin(), salt.end(), res.salt_.begin());
	}

	return res;
}
