#include "router.hpp"

#include "../server/responder.hpp"

namespace fz::http::handlers {

bool router::add_route(std::string prefix, server::transaction_handler &handler)
{
	return add_route(std::move(prefix), [&handler](auto &t) {
		handler.handle_transaction(t);
	});
}

bool router::add_route(std::string prefix, transaction_handler handler)
{
	if (prefix.empty()) {
		return false;
	}

	return routes_.emplace(std::move(prefix), std::move(handler)).second;
}

/*********************************

 /foo/bar matches:

	/foo/bar/baz
	/foo/bar

 But does NOT match

	/foo/barbablu


  FIXME: implement proper handling of redirects (perhaps optional),
		 for routes with (or without) trailing slashes.

		 See: https://symfony.com/doc/current/routing.html#routing-trailing-slash-redirection


*********************************/

void router::handle_transaction(const server::shared_transaction &t)
{
	auto &req = t->req();
	auto &res = t->res();

	for (auto it = routes_.lower_bound(req.uri.path_); it != routes_.end(); ++it) {
		if (fz::starts_with(req.uri.path_, it->first)) {
			std::string new_path;

			if (it->first.back() != '/') {
				if (it->first.size() == req.uri.path_.size()) {
					new_path = "/";
				}
				else
				if (req.uri.path_[it->first.size()] == '/') {
					new_path = req.uri.path_.substr(it->first.size());
				}
				else {
					continue;
				}
			}
			else {
				new_path = req.uri.path_.substr(it->first.size()-1);
			}

			// Preserve the original URI, in case other handlers need to access it.
			auto &original_path = req.headers[headers::X_FZ_INT_Original_Path];
			if (!original_path) {
				original_path = req.uri.path_;
			}

			req.uri.path_ = std::move(new_path);

			it->second(t);
			return;
		}
	}

	res.send_status(404, "Not Found") &&
	res.send_end();
}

}
