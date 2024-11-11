#ifndef FZ_HTTP_SERVER_FILE_SERVER_HPP
#define FZ_HTTP_SERVER_FILE_SERVER_HPP

#include "../server/transaction.hpp"

namespace fz::http::handlers {

/* API Documentation:
 *
 *	GET /path/to/entry
 *		Returns the entry's content.
 *		If the entry is a directory, the content is the directory's listing. The format depends on the Accept: request header.
 *
 *		The accepted formats currently are:
 *			1) text/plain
 *			2) text/html
 *			3) application/ndjson
 *
 *		Status Codes:
 *			200 OK - Successful retrieval
 *			404 Not Found - Entry not found
 *			406 Not Acceptable - Requested format not supported
 *
 *	DELETE /path/to/entry
 *		Deletes the entry.
 *		If the entry is a directory and the FZ-Action-Recursive header is set to true, the deletion will be recursive.
 *
 *		Status Codes:
 *			204 No Content - Successful deletion
 *			404 Not Found - Entry not found
 *			400 Bad Request - Invalid request
 *
 *	PUT /path/to/entry
 *		Creates an entry at the given location.
 *		Unless one of the following conditions is met, the content of the entry is filled with the body of the request.
 *
 *		If the X-FZ-Action request header is set, then:
 *			If the action is "copy-from; path=relative/path/to/target":
 *				The entry's content will be copied from the entry identified by the "path" parameter, resolved against the directory containing the entry being created.
 *				If the X-FZ-Action-Recursive header is set to true, the copy operation will be recursive if the source is a directory.
 *				If the source is a directory and the destination already exists, a 409 Conflict status code will be returned.
 *
 *			If the action is "mkdir":
 *				The entry is created as a directory. The request MUST not have a body.
 *
 *		Status Codes:
 *			204 No Content - Successful creation without a response body
 *			400 Bad Request - Invalid request
 *			409 Conflict - conflict while copying
 *
 *	POST /path/to/directory
 *		Performs a non-idempotent operation in the context of the given target.
 *
 *		Currently supported operations:
 *			MOVE: Moves one entry from one location to another.
 *
 *		MOVE API:
 *			X-FZ-Action header:
 *				move-from; path=relative/path/to/source
 *				move-to; path=relative/path/to/destination
 *
 *			The move-from and move-to actions MUST both be present in the request.
 *			The path arguments are resolved against the request path.
 *
 *		Status Codes:
 *			204 No Content - Successful move without a response body
 *			400 Bad Request - Invalid request
 *			404 Not Found - Source or destination not found
 *			409 Conflict - Conflict in moving the entry
 */

class file_server: public http::server::transaction_handler
{
public:
	struct options: fz::util::options<options, file_server>
	{
		opt<bool> can_get = o(true);
		opt<bool> can_put = o(false);
		opt<bool> can_delete = o(false);
		opt<bool> can_post = o(false);
		opt<bool> can_list_dir = o(false);

		opt<bool> honor_406 = o(false);
		opt<std::vector<std::string>> default_index = o();
		opt<std::string> default_charset = o();

		options(){}
	};

	file_server(tvfs::engine &tvfs, logger_interface &logger, options opts = {});

	void handle_transaction(const http::server::shared_transaction &t) override;

	local_filesys::type get_file_type_or_send_error(std::string_view path, http::server::responder &res);

	static std::string_view mime_from_name(std::string_view name);
	static void send_response_from_result(http::server::responder &res, fz::result result);

private:
	fz::http::field::value negotiate_content_type(http::server::request &req, http::server::responder &res, std::initializer_list<std::string_view> list);
	result send_file(http::server::request &req, http::server::responder &res, std::string_view path);
	bool send_disposition_header(http::server::request &req, http::server::responder &res);

	void do_get(http::server::request &req, http::server::responder &res);
	void do_put(http::server::request &req, http::server::responder &res);
	void do_delete(http::server::request &req, http::server::responder &res);
	void do_post(http::server::request &req, http::server::responder &res);
	void do_put_mkdir(http::server::request &req, http::server::responder &res);
	void do_put_copy(http::server::request &req, http::server::responder &res, std::string_view source);

	#undef DELETE
	enum verbs {
		PUT    = 0b0001,
		GET    = 0b0010,
		POST   = 0b0100,
		DELETE = 0b1000
	};

	FZ_ENUM_BITOPS_FRIEND_DEFINE_FOR(verbs)

	void send_not_allowed_response(http::server::responder &res, verbs additionally_not_allowed = {});


private:
	options opts_;
	tvfs::engine &tvfs_;
	logger_interface &logger_;
};

}

#endif // FZ_HTTP_SERVER_FILE_SERVER_HPP
