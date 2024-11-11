#include "../administrator.hpp"

auto administrator::operator()(administration::get_groups_and_users &&v)
{
	fz::authentication::file_based_authenticator::groups groups;
	fz::authentication::file_based_authenticator::users users;

	authenticator_.get_groups_and_users(groups, users);
	return v.success(std::move(groups), std::move(users));
}

auto administrator::operator()(administration::set_groups_and_users &&v)
{
	auto &&[groups, users, do_save] = std::move(v).tuple();

	set_groups_and_users(std::move(groups), std::move(users));

	if (!do_save || authenticator_.save())
		return v.success();

	return v.failure();
}

void administrator::set_groups_and_users(fz::authentication::file_based_authenticator::groups &&groups, fz::authentication::file_based_authenticator::users &&users)
{
	authenticator_.set_groups_and_users(std::move(groups), std::move(users));
}

FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, administrator, administration::get_groups_and_users);
FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, administrator, administration::set_groups_and_users);
