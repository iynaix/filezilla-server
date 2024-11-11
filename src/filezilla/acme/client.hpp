#ifndef FZ_ACME_CLIENT_HPP
#define FZ_ACME_CLIENT_HPP

#include <stack>

#include <libfilezilla/jws.hpp>
#include <libfilezilla/hash.hpp>
#include <libfilezilla/tls_layer.hpp>

#include "../http/client.hpp"
#include "../logger/modularized.hpp"

#include "challenges.hpp"

namespace fz::acme
{

class client
{
	class challenger;

public:
	using opid_t = std::size_t;

	struct error_event_tag{};
	using error_event = simple_event<error_event_tag, opid_t, native_string /* error */, datetime /* retry at */>;

	struct terms_of_service_event_tag{};
	using terms_of_service_event = simple_event<terms_of_service_event_tag, opid_t, std::string /*terms_of_service*/>;

	struct account_event_tag{};
	using account_event = simple_event<account_event_tag, opid_t, fz::uri /* directory */, std::string /* kid */, std::pair<json, json> /* jkw */, json /* object */>;

	struct certificate_event_tag{};
	using certificate_event = simple_event<certificate_event_tag, opid_t, fz::uri /* directory */, securable_socket::certs_and_key, securable_socket::certs_and_key::sources::acme>;

	client(thread_pool &pool, event_loop &loop, logger_interface &logger, tls_system_trust_store &trust_store);
	~client();

	opid_t get_terms_of_service(const fz::uri &directory, event_handler &target_handler);
	opid_t get_account(const fz::uri &directory, const std::vector<std::string> &contacts, const std::pair<json, json> &jwk, bool already_existing, event_handler &target_handler);
	opid_t get_certificate(const fz::uri &directory, const std::pair<json, json> &jwk, const std::vector<std::string> &hosts, const serve_challenges::how &how_to_serve_challenges, fz::duration allowed_max_server_time_difference, event_handler &target_handler);
	opid_t get_certificate(const fz::uri &directory, const std::pair<json, json> &jwk, const std::vector<std::string> &hosts, tls_param key, native_string password, const serve_challenges::how &how_to_serve_challenges, fz::duration allowed_max_server_time_difference, event_handler &target_handler);

private:
	bool get_terms_of_service_impl();
	bool get_account_impl();
	bool get_certificate_impl();

private:
	inline static std::size_t max_non_acme_error_size = 1024;

	http::response::handler handle(http::response::handler h);

	fz::json make_jws(const std::string &url, const fz::json &payload, const std::string &nonce, const std::pair<json, json> &jwk, const std::string &kid);

	bool do_get_account();
	bool do_get_directory();
	bool do_get_nonce();
	bool do_get_certificate_order();
	bool do_get_account_authorizations();
	bool do_start_challenges();
	bool do_wait_for_challenges_done();
	bool do_finalize();
	bool do_get_certificate();

private:
	fz::mutex mutex_;

	using operation = bool (acme::client::*)();
	std::stack<operation> opstack_{};
	opid_t opid_{};

	bool update_opid();
	bool execute(operation op);
	void reenter();

	template <typename Event, typename... Args>
	std::enable_if_t<std::is_constructible_v<typename Event::tuple_type, Args...>>
	stop(Args &&... args);

	template <typename Err>
	bool stop_if(Err err, native_string_view msg);

private:
	thread_pool &pool_;
	event_loop &loop_;
	logger::modularized logger_;

	http::client http_client_;

private:
	event_handler *target_handler_{};

	struct data {
		fz::uri directory_uri_;
		std::vector<std::string> contacts_;
		std::pair<json, json> jwk_;
		fz::json directory_;
		std::string nonce_;
		bool already_existing_account_{};
		fz::json account_info_;
		std::string kid_;
		std::vector<std::string> hosts_;
		json certificate_order_;
		fz::uri certificate_order_location_;
		fz::datetime certificate_order_retry_at_;
		std::vector<json> account_auths_; // Will have the same size as hosts_'s.
		std::deque<json> http_01_challenges_; // Will have the same size as hosts_'s.
		std::size_t current_account_auths_polling_{};
		std::string certificate_chain_;
		tls_param certificate_key_;
		native_string certificate_key_password_;
		fz::duration allowed_max_server_time_difference_{};

		data(){}
	};

	std::optional<data> d_;
	std::unique_ptr<challenger> challenger_;
};

}

#endif // FZ_ACME_CLIENT_HPP
