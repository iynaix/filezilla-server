#include <cstdio>
#include <cinttypes>
#include <clocale>

#include <libfilezilla/hash.hpp>
#include <libfilezilla/util.hpp>
#include <libfilezilla/encode.hpp>

#include "../../filezilla/authentication/password.hpp"
#include "../../filezilla/service.hpp"
#include "../../filezilla/tls_exit.hpp"

int FZ_SERVICE_PROGRAM_MAIN(argc, argv)
{
	if (argc != 2) {
		std::fputs("Missing parameters.\n", stderr);

		fz::tls_exit(EXIT_FAILURE);
	}

	std::setlocale(LC_ALL, "");

	auto root = fz::to_utf8(argv[1]);

	std::string password;

	for (int c;;) {
		c = fgetc(stdin);
		if (c == EOF || c == '\n' || c == '\r')
			break;
		password.append(1, char(c));
	}

	// Assume the input is in the native multibyte encoding, then convert it to utf8.
	// If the native multibyte encoding is utf8 to begin with, this next line is at all effects a no-op.
	password = fz::to_utf8(fz::to_wstring(password));

	if (password.empty()) {
		printf("--%s@index=%zu", root.c_str(), fz::authentication::any_password::variant(fz::authentication::password::none()).index());
	}
	else {
		auto salt = fz::random_bytes(fz::authentication::password::pbkdf2::hmac_sha256::salt_size);
		auto iterations = fz::authentication::password::pbkdf2::hmac_sha256::min_iterations;
		auto hash_size = fz::authentication::password::pbkdf2::hmac_sha256::hash_size;

		auto hash = fz::pbkdf2_hmac_sha256(password, salt, hash_size, static_cast<unsigned>(iterations));

		printf("--%s@index=%zu ", root.c_str(), fz::authentication::any_password::variant(fz::authentication::password::pbkdf2::hmac_sha256()).index());
		printf("--%s.hash=%s ", root.c_str(), fz::base64_encode(hash, fz::base64_type::standard, false).c_str());
		printf("--%s.salt=%s ", root.c_str(), fz::base64_encode(salt, fz::base64_type::standard, false).c_str());
		printf("--%s.iterations=%" PRIu64, root.c_str(), iterations);
	}

	fz::tls_exit(EXIT_SUCCESS);
}
