#ifndef FZ_AUTHENTICATION_TOKEN_MANAGER_HPP
#define FZ_AUTHENTICATION_TOKEN_MANAGER_HPP

#include <string>

#include <libfilezilla/impersonation.hpp>
#include <libfilezilla/encryption.hpp>

#include "../logger/modularized.hpp"

#include "user.hpp"


namespace fz::authentication {


struct access_token
{
	std::uint64_t id{};
	std::uint64_t refresh_id{};

	operator bool() const
	{
		return id != 0 && refresh_id != 0;
	}

	bool operator==(const access_token &rhs) const
	{
		return std::tie(id, refresh_id) == std::tie(rhs.id, rhs.refresh_id);
	}

	bool operator!=(const access_token &rhs) const
	{
		return !operator==(rhs);
	}

	std::string encrypt(const symmetric_key &key) const;
	static access_token decrypt(std::string_view encrypted, const symmetric_key &key);

	template <typename Archive>
	void serialize(Archive &ar);
};

struct refresh_token
{
	access_token access;
	std::string username;
	std::string path;

	operator bool() const
	{
		return access && !username.empty();
	}

	bool operator==(const refresh_token &rhs) const
	{
		return std::tie(access, username, path) == std::tie(rhs.access, username, path);
	}

	bool operator!=(const refresh_token &rhs) const
	{
		return !operator==(rhs);
	}

	std::string encrypt(const symmetric_key &key) const;
	static refresh_token decrypt(std::string_view encrypted, const symmetric_key &key);

	template <typename Archive>
	void serialize(Archive &ar);
};

struct token
{
	refresh_token refresh;
	bool must_impersonate{};

	datetime created_at;
	datetime expires_at;

	explicit operator bool() const
	{
		return refresh && created_at;
	}

	bool operator==(const token &rhs) const
	{
		return std::tie(refresh, must_impersonate, created_at, expires_at) == std::tie(rhs.refresh, rhs.must_impersonate, created_at, expires_at);
	}

	bool operator!=(const token &rhs) const
	{
		return !operator==(rhs);
	}
};

struct token_db
{
	virtual ~token_db();

	virtual token select(std::uint64_t id) = 0;
	virtual token insert(std::string name, std::string path, bool needs_impersonation, fz::duration expires_in) = 0;
	virtual bool remove(std::uint64_t id) = 0;
	virtual bool update(const token &t) = 0;
	virtual void reset() = 0;
	virtual const symmetric_key &get_symmetric_key() = 0;
};


class token_manager
{
public:
	token_manager(token_db &db, logger_interface &logger);

	~token_manager();

	/// \brief Verifies that the refresh token is valid and matches the given username.
	/// \param username The username associated with the token.
	/// \param token The refresh token to verify.
	/// \param impersonation An output parameter for impersonation details.
	/// \returns A boolean indicating whether the verification succeeded.
	/// \note If the passed-in token is found to have been previously invalidated, indicating a potential replay attack,
	///       the valid token derived from the invalid one will be invalidated too.
	bool verify(std::string_view username, const refresh_token &token, impersonation_token &impersonation);

	/// \brief Creates a new refresh token for the given authenticated user.
	/// \param user The authenticated user.
	/// \param expiration The duration for which the token is valid. If the empty duration, the token will be forever valid.
	/// \param path An optional parameter to restrict access to the given path.
	/// \returns A new refresh token. If creation fails, the returned token will be invalid.
	refresh_token create(const shared_user &user, duration expiration, std::string_view path = {});

	/// \brief Refreshes an existing token. The old token becomes invalid.
	/// \param old The old refresh token.
	/// \returns A new refresh token. If refresh fails, the returned token will be invalid.
	refresh_token refresh(const refresh_token &old);

	/// \brief Destroys (invalidates) a refresh token and all of its descendants.
	/// \param token The refresh token to remove.
	/// \returns True if the refresh token was destroyed, false otherwise.
	bool destroy(const refresh_token &token);

	/// \brief Destroys (invalidates) all existing tokens and resets the manager to a clean state.
	void reset();

	/// \brief Retrieves the symmetric key used for encryption and decryption.
	/// \returns The symmetric key as a string.
	const symmetric_key &get_symmetric_key();

private:
	token_db &db_;
	logger::modularized logger_;
	mutex mutex_;
};

struct in_memory_token_db: token_db
{
	in_memory_token_db()
	{
		in_memory_token_db::reset();
	}

	token select(uint64_t id) override
	{
		auto it = map_.find(id);
		if (it != map_.end()) {
			return it->second;
		}

		return {};
	}

	bool remove(uint64_t id) override
	{
		return map_.erase(id) != 0;
	}

	bool update(const token &t) override {
		auto it = map_.find(t.refresh.access.id);
		if (it == map_.end()) {
			return false;
		}

		it->second = t;
		return true;
	}

	void reset() override
	{
		map_.clear();
		key_ = symmetric_key::generate();
	}

	const symmetric_key &get_symmetric_key() override
	{
		return key_;
	}

	token insert(std::string name, std::string path, bool needs_impersonation, fz::duration expires_in) override
	{
		auto now = datetime::now();
		auto res = map_.emplace(next_id_, token{ { { next_id_,  1 }, std::move(name), std::move(path) }, needs_impersonation, now, expires_in ? now+expires_in : datetime() });

		if (res.second) {
			next_id_ += 1;
			return res.first->second;
		}

		return {};
	}

private:
	symmetric_key key_;

	std::unordered_map<std::uint64_t, token> map_;
	std::uint64_t next_id_{1};
};



extern template void access_token::serialize<serialization::binary_input_archive>(serialization::binary_input_archive &);
extern template void access_token::serialize<serialization::binary_output_archive>(serialization::binary_output_archive &);

extern template void refresh_token::serialize<serialization::binary_input_archive>(serialization::binary_input_archive &);
extern template void refresh_token::serialize<serialization::binary_output_archive>(serialization::binary_output_archive &);

}

#endif // FZ_AUTHENTICATION_TOKEN_MANAGER_HPP
