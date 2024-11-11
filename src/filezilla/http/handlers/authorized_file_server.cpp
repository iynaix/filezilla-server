#include "authorized_file_server.hpp"

namespace fz::http::handlers
{

struct authorized_file_server::custom_authorization_data
{
	custom_authorization_data(logger_interface &logger, file_server::options opts = {})
		: tvfs(logger)
		, fs(tvfs, logger, std::move(opts))
	{}

	tvfs::engine tvfs;
	file_server fs;

	static std::shared_ptr<custom_authorization_data> get(const server::shared_transaction &t, authorized_file_server &fs)
	{
		if (auto d = fs.auth_.get_authorization_data(t, &fs)) {
			auto unc = notifications_count(d->user);
			auto c = std::static_pointer_cast<custom_authorization_data>(d->custom);

			if (unc != c->unc_) {
				c->unc_ = unc;

				auto u = d->user->lock();

				c->tvfs.set_mount_tree(u->mount_tree);
				c->tvfs.set_backend(u->impersonator);
				c->tvfs.set_open_limits(u->session_open_limits);
			}

			return c;
		}

		return {};
	}

private:
	std::size_t unc_{std::size_t(-1)};
};

authorized_file_server::authorized_file_server(authorizator &auth, logger_interface &logger, file_server::options opts)
	: auth_(auth)
	, logger_(logger)
	, opts_(std::move(opts))
{
}

std::shared_ptr<void> authorized_file_server::make_custom_authorization_data()
{
	return std::make_shared<custom_authorization_data>(logger_, opts_);
}

void authorized_file_server::handle_transaction(const server::shared_transaction &t)
{
	if (auto c = custom_authorization_data::get(t, *this)) {
		c->fs.handle_transaction(t);
	}
}

}
