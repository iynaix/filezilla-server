#include "rewriter.hpp"

namespace fz::webui
{

rewriter::rewriter(http::server::transaction_handler &th)
	: th_(th)
{}

void rewriter::handle_transaction(const http::server::shared_transaction &t) {
	using namespace std::string_view_literals;

	auto &req = t->req();

	std::string_view path = req.uri.path_;

	bool should_rewrite = true;

	for (auto p: {"/assets"sv, "/favicon.ico"sv, "/icons"sv, "/index.html"sv, "/api"sv}) {
		if (fz::starts_with(path, p)) {
			should_rewrite = false;
			break;
		}
	}

	if (should_rewrite) {
		req.uri.path_ = "/index.html"sv;
	}

	th_.handle_transaction(t);
}

}
