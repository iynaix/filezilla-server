#include <libfilezilla/time.hpp>
#include <libfilezilla/encode.hpp>
#include <libfilezilla/util.hpp>
#include <libfilezilla/hash.hpp>

#include "proof_of_work.hpp"
#include "../transformed_view.hpp"
#include "../string.hpp"

namespace fz::util {

query_string proof_of_work(const std::string &name, std::size_t difficulty, std::initializer_list<std::pair<std::string /*name*/, std::string /*value*/>> params)
{
	std::vector<uint8_t> nonce;
	std::vector<uint8_t> proof;
	std::string now;

	for (bool valid = {}; !valid;) {
		now = fz::to_string(fz::datetime::now().get_time_t());
		nonce = fz::random_bytes(32);
		auto input = name + '|' + now + '|' + join(transformed_view(params, [](auto &p) { return p.second; }), "|");
		proof = fz::hmac_sha256(nonce, input);

		valid = [&]() {
			for (size_t i = 0; i < difficulty / 8; ++i) {
				if (proof[i]) {
					return false;
				}
			}

			if (difficulty % 8) {
				if (proof[difficulty / 8] >= 1u << (8 - difficulty % 8)) {
					return false;
				}
			}

			return true;
		}();
	}

	query_string ret;
	ret[name] = "";
	ret["ts"] = now;
	ret["nonce"] = fz::hex_encode<std::string>(nonce);
	ret["proof"] = fz::hex_encode<std::string>(proof);

	for (auto &p: params) {
		if (!p.first.empty()) {
			ret[p.first] = p.second;
		}
	}

	return ret;
}

}


