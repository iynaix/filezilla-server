#include "../administrator.hpp"

auto administrator::operator()(administration::get_ip_filters &&v)
{
	return v.success(disallowed_ips_.lock().get_list(), allowed_ips_.lock().get_list());
}

auto administrator::operator()(administration::set_ip_filters &&v)
{
	auto &&[disallowed, allowed] = std::move(v).tuple();

	set_ip_filters(std::move(disallowed), std::move(allowed), true);

	return v.success();
}

void administrator::set_ip_filters(fz::tcp::binary_address_list &&disallowed, fz::tcp::binary_address_list &&allowed, bool save)
{
	disallowed_ips_.set_list(std::move(disallowed), save);
	allowed_ips_.set_list(std::move(allowed), save);

	kick_disallowed_ips();
}

FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, administrator, administration::set_ip_filters);
FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, administrator, administration::get_ip_filters);
