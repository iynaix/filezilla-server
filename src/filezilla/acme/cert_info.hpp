#ifndef FZ_ACME_CERT_INFO_HPP
#define FZ_ACME_CERT_INFO_HPP

#include <string>
#include <vector>
#include <libfilezilla/json.hpp>

#include "../util/filesystem.hpp"

namespace fz::acme
{

struct extra_account_info
{
	std::string directory;
	std::vector<std::string> contacts;
	std::string created_at;
	std::pair<json, json> jwk;

	explicit operator bool() const
	{
		return !directory.empty() && jwk.first && jwk.second;
	}

	static extra_account_info from_json(const fz::json &json);
	static extra_account_info load(const util::fs::native_path &root, const std::string &account_id);
	bool save(const util::fs::native_path &root, const std::string &account_id) const;
};

}

#endif // FZ_ACME_CERT_INFO_HPP
