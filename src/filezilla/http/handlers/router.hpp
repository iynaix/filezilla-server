#ifndef FZ_HTTP_HANDLERS_ROUTER_HPP
#define FZ_HTTP_HANDLERS_ROUTER_HPP

#include <map>

#include "../server/transaction.hpp"

namespace fz::http::handlers {

class router: public http::server::transaction_handler
{
	using transaction_handler = std::function<void (const server::shared_transaction &t)>;

	using map_type = std::map<std::string, transaction_handler, std::greater<>>;

public:
	void handle_transaction(const server::shared_transaction &t) override;

	bool add_route(std::string prefix, http::server::transaction_handler &handler);
	bool add_route(std::string prefix, transaction_handler handler);

	template <typename T>
	bool add_route(std::string prefix, void (T::*handler)(const server::shared_transaction &t), T *obj)
	{
		return add_route(std::move(prefix), [obj, handler](const server::shared_transaction &t) {
			return (obj->*handler)(t);
		});
	}

private:
	map_type routes_;
};


}
#endif // FZ_HTTP_HANDLERS_ROUTER_HPP
