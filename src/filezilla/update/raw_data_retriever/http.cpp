#include "../../string.hpp"
#include "../../sys_info.hpp"

#include "http.hpp"

namespace fz::update::raw_data_retriever
{

static bool verify_certificate(const fz::tls_session_info &info)
{
	// BASE-64 encoded DER without the BEGIN/END CERTIFICATE
	static const auto updater_cert = fz::base64_decode("\
	MIIFsTCCA5ugAwIBAgIESnXLbzALBgkqhkiG9w0BAQ0wSTELMAkGA1UEBhMCREUx\
	GjAYBgNVBAoTEUZpbGVaaWxsYSBQcm9qZWN0MR4wHAYDVQQDExVmaWxlemlsbGEt\
	cHJvamVjdC5vcmcwHhcNMDkwODAyMTcyMjU2WhcNMzEwNjI4MTcyMjU4WjBJMQsw\
	CQYDVQQGEwJERTEaMBgGA1UEChMRRmlsZVppbGxhIFByb2plY3QxHjAcBgNVBAMT\
	FWZpbGV6aWxsYS1wcm9qZWN0Lm9yZzCCAh8wCwYJKoZIhvcNAQEBA4ICDgAwggIJ\
	AoICAJqWXy7YzVP5pOk8VB9bd/ROC9SVbAxJiFHh0I0/JmyW+jSfzFCYWr1DKGVv\
	Oui+qiUsaSgjWTh/UusnVu4Q4Lb00k7INRF6MFcGFkGNmOZPk4Qt0uuWMtsxiFek\
	9QMPWSYs+bxk+M0u0rNOdAblsIzeV16yhfUQDtrJxPWbRpuLgp9/4/oNbixet7YM\
	pvwlns2o1KXcsNcBcXraux5QmnD4oJVYbTY2qxdMVyreA7dxd40c55F6FvA+L36L\
	Nv54VwRFSqY12KBG4I9Up+c9OQ9HMN0zm0FhYtYeKWzdMIRk06EKAxO7MUIcip3q\
	7v9eROPnKM8Zh4dzkWnCleirW8EKFEm+4+A8pDqirMooiQqkkMesaJDV361UCoVo\
	fRhqfK+Prx0BaJK/5ZHN4tmgU5Tmq+z2m7aIKwOImj6VF3somVvmh0G/othnU2MH\
	GB7qFrIUMZc5VhrAwmmSA2Z/w4+0ToiR+IrdGmDKz3cVany3EZAzWRJUARaId9FH\
	v/ymA1xcFAKmfxsjGNlNpXd7b8UElS8+ccKL9m207k++IIjc0jUPgrM70rU3cv5M\
	Kevp971eHLhpWa9vrjbz/urDzBg3Dm8XEN09qwmABfIEnhm6f7oz2bYXjz73ImYj\
	rZsogz+Jsx3NWhHFUD42iA4ZnxHIEgchD/TAihpbdrEhgmdvAgMBAAGjgacwgaQw\
	EgYDVR0TAQH/BAgwBgEB/wIBAjAmBgNVHREEHzAdgRthZG1pbkBmaWxlemlsbGEt\
	cHJvamVjdC5vcmcwDwYDVR0PAQH/BAUDAwcGADAdBgNVHQ4EFgQUd4w2verFjXAn\
	CrNLor39nFtemNswNgYDVR0fBC8wLTAroCmgJ4YlaHR0cHM6Ly9jcmwuZmlsZXpp\
	bGxhLXByb2plY3Qub3JnL2NybDALBgkqhkiG9w0BAQ0DggIBAF3fmV/Bs4amV78d\
	uhe5PkW7yTO6iCfKJVDB22kXPvL0rzZn4SkIZNoac8Xl5vOoRd6k+06i3aJ78w+W\
	9Z0HK1jUdjW7taYo4bU58nAp3Li+JwjE/lUBNqSKSescPjdZW0KzIIZls91W30yt\
	tGq85oWAuyVprHPlr2uWLg1q4eUdF6ZAz4cZ0+9divoMuk1HiWxi1Y/1fqPRzUFf\
	UGK0K36iPPz2ktzT7qJYXRfC5QDoX7tCuoDcO5nccVjDypRKxy45O5Ucm/fywiQW\
	NQfz/yQAmarQSCfDjNcHD1rdJ0lx9VWP6xi+Z8PGSlR9eDuMaqPVAE1DLHwMMTTZ\
	93PbfP2nvgbElgEki28LUalyVuzvrKcu/rL1LnCJA4jStgE/xjDofpYwgtG4ZSnE\
	KgNy48eStvNZbGhwn2YvrxyKmw58WSQG9ArOCHoLcWnpedSZuTrPTLfgNUx7DNbo\
	qJU36tgxiO0XLRRSetl7jkSIO6U1okVH0/tvstrXEWp4XwdlmoZf92VVBrkg3San\
	fA5hBaI2gpQwtpyOJzwLzsd43n4b1YcPiyzhifJGcqRCBZA1uArNsH5iG6z/qHXp\
	KjuMxZu8aM8W2gp8Yg8QZfh5St/nut6hnXb5A8Qr+Ixp97t34t264TBRQD6MuZc3\
	PqQuF7sJR6POArUVYkRD/2LIWsB7\
	");

	auto &certs = info.get_certificates();
	if (certs.empty())
		return false;

	return certs.back().get_raw_data() == updater_cert;
}

const fz::duration http::response_timeout = fz::duration::from_seconds(10);

http::http(event_loop &loop, thread_pool &pool, logger_interface &logger)
	: logger_(logger, "Update HTTP Retriever")
	, http_client_(pool, loop, logger_, fz::http::client::options()
		.follow_redirects(true)
		.default_timeout(response_timeout)
		.cert_verifier(verify_certificate)
)
{
}

void http::retrieve_raw_data(bool manual, fz::receiver_handle<result> h)
{
	return retrieve_raw_data(get_query_string(manual), std::move(h));
}

void http::retrieve_raw_data(std::string query_string, fz::receiver_handle<result> h)
{
	fz::uri uri("https://update.filezilla-project.org/update.php");

	uri.query_ = std::move(query_string);

	http_client_.perform("GET", uri).confidentially().and_then([h=std::make_shared<receiver_handle<result>>(std::move(h)), this](const fz::http::response &r) {
		if (r.code != 200)
			(*h)(fz::unexpected(fz::sprintf("HTTP %s: %s", r.code_string(), r.reason)));
		else {
			(*h)(std::string(r.body.to_view()));
		}
	});
}

std::string http::get_query_string(bool manual)
{
	static constexpr auto product_id = "1";

	fz::query_string q;

	q["osarch"] = to_string(sizeof(std::size_t)*CHAR_BIT);
	q["product"] = product_id;
	q["version"] = build_info::version;
	q["platform"] = build_info::host;
	q["manual"] = fz::to_string(manual);
	q["cpuid"] = fz::join(sys_info::cpu_caps, ",");
	//q["test"] = "1";

	return q.to_string(true);
}

}
