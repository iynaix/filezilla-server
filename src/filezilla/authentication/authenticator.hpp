#ifndef FZ_AUTHENTICATION_AUTHENTICATOR_HPP
#define FZ_AUTHENTICATION_AUTHENTICATOR_HPP

#include <vector>

#include <libfilezilla/event_handler.hpp>
#include <libfilezilla/iputils.hpp>

#include "user.hpp"
#include "error.hpp"
#include "method.hpp"

#ifndef FZ_AUTHENTICATION_AUTHENTICATOR_USERS_CASE_INSENSITIVE
#	ifdef FZ_WINDOWS
#		define FZ_AUTHENTICATION_AUTHENTICATOR_USERS_CASE_INSENSITIVE 1
#	else
#		define FZ_AUTHENTICATION_AUTHENTICATOR_USERS_CASE_INSENSITIVE 0
#	endif
#endif

#if FZ_AUTHENTICATION_AUTHENTICATOR_USERS_CASE_INSENSITIVE
#	include <map>
#endif

namespace fz::authentication {

class authenticator {
public:
	struct operation {
		using result_event = simple_event<operation, authenticator &, std::unique_ptr<operation>>;

		virtual ~operation();

		virtual shared_user get_user() = 0;
		virtual available_methods get_methods() = 0;
		virtual error get_error() = 0;

		friend void stop(std::unique_ptr<operation> op)
		{
			if (op)
				op->stop();
		}

		friend bool next(std::unique_ptr<operation> op, const methods_list &methods)
		{
			if (op)
				return op->next(methods);

			return false;
		}

	private:
		/// \brief Stops the authetication process.
		/// Can be invoked only once.
		virtual void stop() = 0;

		/// \brief Invokes the next step of the authentication.
		/// Can be invoked only once.
		/// \returns false if invoking failed.
		virtual bool next(const methods_list &methods) = 0;
	};

	virtual ~authenticator();

	/// \brief Starts the authentication process.
	/// The \param methods contains a sequence of methods to be evaluated at once. Authentication succeeds IFF all of the methods succeed.
	virtual void authenticate(std::string_view user_name, const methods_list &methods, address_type family, std::string_view ip, event_handler &target, logger::modularized::meta_map meta_for_logging = {}) = 0;
	virtual void stop_ongoing_authentications(event_handler &target) = 0;

protected:
#if FZ_AUTHENTICATION_AUTHENTICATOR_USERS_CASE_INSENSITIVE
	struct case_insensitive_utf8_less
	{
		bool operator()(const std::string &lhs, const std::string &rhs) const
		{
			return fz::stricmp(fz::to_wstring_from_utf8(lhs), fz::to_wstring_from_utf8(rhs)) < 0;
		}
	};

	template <typename T>
	using users_map = std::map<std::string /*user name*/, T, case_insensitive_utf8_less>;
#else
	template <typename T>
	using users_map = std::unordered_map<std::string /*user name*/, T>;
#endif
};

void stop(std::unique_ptr<authenticator::operation> op);
bool next(std::unique_ptr<authenticator::operation> op, const methods_list &methods);

class session_user
{
public:
	session_user();
	explicit session_user(std::unique_ptr<authenticator::operation> op, logger_interface &logger = get_null_logger());

	session_user(const session_user &) = delete;
	session_user(session_user &&) = default;
	session_user& operator=(const session_user &) = delete;
	session_user& operator=(session_user &&) = default;

	decltype(auto) operator->() {
		return su_.operator->();
	}

	decltype(auto) operator*() {
		return su_.operator*();
	}

	decltype(auto) get() const noexcept
	{
		return su_.get();
	}

	explicit operator bool() const {
		return su_ && !error_;
	}

	authentication::error error() const {
		return error_;
	}

	bool friend subscribe(session_user &su, event_handler &eh)
	{
		return subscribe(su.su_, eh);
	}

	bool friend unsubscribe(session_user &su, event_handler &eh)
	{
		return unsubscribe(su.su_, eh);
	}

	bool friend operator==(const shared_user &lhs, const session_user &rhs) {
		return lhs == rhs.su_;
	}

	bool friend operator==(const session_user &lhs, const shared_user &rhs) {
		return lhs.su_ == rhs;
	}

	bool friend operator!=(const shared_user &lhs, const session_user &rhs) {
		return lhs != rhs.su_;
	}

	bool friend operator!=(const session_user &lhs, const shared_user &rhs) {
		return lhs.su_ != rhs;
	}

	operator weak_user() const
	{
		return su_;
	}

	operator shared_user() const
	{
		return su_;
	}

	void reset() {
		*this = {};
	}

private:
	shared_user su_;
	authentication::error error_;
	util::limited_copies_counter session_count_limiter_;
	std::vector<util::limited_copies_counter> extra_session_count_limiters_;
};

struct none_authenticator: authenticator
{

	void authenticate(std::string_view, const methods_list &, address_type, std::string_view, event_handler &, logger::modularized::meta_map meta_for_logging = {}) override;

	void stop_ongoing_authentications(event_handler &) override;
};

}

#endif // FZ_AUTHENTICATION_AUTHENTICATOR_HPP
