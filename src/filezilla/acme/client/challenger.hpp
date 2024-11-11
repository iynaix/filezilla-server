#ifndef FZ_ACME_CLIENT_CHALLENGER_HPP
#define FZ_ACME_CLIENT_CHALLENGER_HPP

#include "../client.hpp"
#include "../../tcp/server.hpp"

namespace fz::acme
{

class client::challenger
{
public:
	class internal;
	class external;

	virtual ~challenger();

	virtual native_string /*error*/ serve(const std::string &token, const std::pair<json, json> &jwk) = 0;

protected:
	std::string get_jwk_fingerprint(const std::pair<json, json> &jwk);
};

class client::challenger::external: public challenger
{
public:
	external(const serve_challenges::externally &how);
	~external() override;

	native_string /*error*/ serve(const std::string &token, const std::pair<json, json> &jwk) override;

private:
	logger::modularized logger_;
	serve_challenges::externally how_;
	std::vector<native_string> paths_;
};

class client::challenger::internal: public challenger, tcp::session::factory
{
	class session;

public:
	internal(thread_pool &pool, event_loop &loop, logger_interface &logger, const serve_challenges::internally &how);

	native_string /*error*/ serve(const std::string &token, const std::pair<json, json> &jwk) override;

private:
	std::unique_ptr<tcp::session> make_session(event_handler &target_handler, tcp::session::id id, std::unique_ptr<socket> socket, const std::any &user_data, int &error /* In-Out */) override;

	fz::mutex mutex_;
	logger::modularized logger_;

	serve_challenges::internally how_;
	std::map<std::string, std::string> map_;
	tcp::server::context context_;
	tcp::server server_;
};

}

#endif // FZ_ACME_CLIENT_CHALLENGER_HPP
