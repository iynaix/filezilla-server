#include <libfilezilla/buffer.hpp>
#include <libfilezilla/json.hpp>
#include <libfilezilla/local_filesys.hpp>
#include <libfilezilla/encode.hpp>

#include "cert_info.hpp"
#include "../securable_socket.hpp"
#include "../util/io.hpp"

namespace fz::acme
{

extra_account_info extra_account_info::from_json(const fz::json &account_info)
{
	extra_account_info extra{};

	if (account_info) {
		extra.directory = account_info["directory"].string_value();
		for (auto &c: account_info["contact"])
			extra.contacts.push_back(c.string_value());
		extra.created_at = account_info["createdAt"].string_value();
		extra.jwk.first = std::move(account_info["jwk"]["priv"]);
		extra.jwk.second = std::move(account_info["jwk"]["pub"]);
	}

	return extra;
}

extra_account_info extra_account_info::load(const util::fs::native_path &root, const std::string &account_id)
{
	if (!root.is_absolute()) {
		return {};
	}

	auto encoded_account_id = fz::to_native(fz::base32_encode(fz::md5(account_id), base32_type::locale_safe, false));
	auto account_info = fz::json::parse(util::io::read(root / fzT("acme") / encoded_account_id / fzT("account.info")));

	return from_json(account_info);
}

bool extra_account_info::save(const util::fs::native_path &root, const std::string &account_id) const
{
	if (!root.is_absolute()) {
		return false;
	}

	json account_info;

	account_info["kid"] = account_id;
	account_info["directory"] = directory;
	account_info["createdAt"] = created_at;
	account_info["jwk"]["priv"] = jwk.first;
	account_info["jwk"]["pub"] = jwk.second;

	auto &c = account_info["contacts"] = json(json_type::array);
	for (std::size_t i = 0; i < contacts.size(); ++i)
		c[i] = contacts[i];


	auto encoded_account_id = fz::to_native(fz::base32_encode(fz::md5(account_id), base32_type::locale_safe, false));
	auto account_info_dir = root / fzT("acme") / encoded_account_id;

	if (!fz::mkdir(account_info_dir, true, mkdir_permissions::cur_user_and_admins)) {
		return false;
	}

	return util::io::write(fz::file(account_info_dir / fzT("account.info"), fz::file::writing, fz::file::current_user_and_admins_only | fz::file::empty), account_info.to_string());
}

}
