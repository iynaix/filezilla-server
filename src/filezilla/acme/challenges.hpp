#ifndef FZ_ACME_CHALLENGES_HPP
#define FZ_ACME_CHALLENGES_HPP

#include <vector>
#include <variant>

#include "../tcp/address_info.hpp"
#include "../util/filesystem.hpp"

namespace fz::acme
{

struct serve_challenges
{
	struct internally
	{
		// The tls_mode field is ignored.
		std::vector<tcp::address_info> addresses_info;
	};

	struct externally
	{
		util::fs::native_path well_known_path;
		bool create_if_not_existing = false;
	};

	struct how: std::variant<std::monostate, internally, externally>
	{
		using variant::variant;

		bool has_value() const
		{
			return !std::holds_alternative<std::monostate>(*this);
		}

		explicit operator bool() const
		{
			return has_value();
		}
	};
};

}

#endif // FZ_ACME_CHALLENGES_HPP
