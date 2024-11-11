#include <map>

#include "file_server.hpp"
#include "../server/responder.hpp"

#include "../../strresult.hpp"
#include "../../string.hpp"

#include "../../logger/type.hpp"

namespace fz::http::handlers {

void file_server::send_response_from_result(server::responder &res, result result)
{
	if (result) {
		res.send_status(204, "No Content") &&
		res.send_end();
	}
	else
	if (result.error_ == result.noperm) {
		res.send_status(403, "Forbidden") &&
		res.send_end();
	}
	else
	if (result.error_ == result.nofile || result.error_ == result.nodir) {
		res.send_status(404, "Not Found") &&
		res.send_end();
	}
	else
	if (result.raw_ == FZ_RESULT_RAW_NOT_IMPLEMENTED) {
		res.send_status(501, "Not Implemented") &&
		res.send_end();
	}
	else
	if (result.raw_ == FZ_RESULT_RAW_ALREADY_EXISTS) {
		res.send_status(409, "Conflict") &&
		res.send_end();
	}
	else {
		res.send_status(500, "Internal Server Error") &&
		res.send_header(http::headers::Connection, "close") &&
		res.send_body(fz::sprintf("%s\n", fz::strresult(result)));
	}
}

void file_server::send_not_allowed_response(server::responder &res, verbs additionally_not_allowed)
{
	http::field::value allowed;

	if (opts_.can_get() && !(additionally_not_allowed & verbs::GET)) {
		allowed += "GET";
	}

	if (opts_.can_put() && !(additionally_not_allowed & verbs::PUT)) {
		allowed += "PUT";
	}

	if (opts_.can_delete() && !(additionally_not_allowed & verbs::DELETE)) {
		allowed += "DELETE";
	}

	if (opts_.can_post() && !(additionally_not_allowed & verbs::POST)) {
		allowed += "POST";
	}

	if (!allowed.empty()) {
		res.send_status(405, "Method Not Allowed") &&
		res.send_header(fz::http::headers::Allowed, allowed) &&
		res.send_end();
	}
	else {
		res.send_status(403, "Forbidden") &&
		res.send_end();
	}

	return;
}

std::string_view file_server::mime_from_name(std::string_view name) {
	static const std::map<std::string_view /* ext */, std::string_view /* mime */, fz::less_insensitive_ascii> map = {
		{ "js",   "text/javascript" },
		{ "css",  "text/css" },
		{ "html", "text/html"},
		{ "svg",  "image/svg+xml" },
		{ "png",  "image/png" },
		{ "jpeg", "image/jpeg" },
		{ "jpg",  "image/jpeg" },
		{ "gif",  "image/gif" }
	};

	if (auto dot = name.rfind('.'); dot != name.npos) {
		auto it = map.find(name.substr(dot+1));
		if (it != map.end()) {
			return it->second;
		}
	}

	return "application/octet-stream";
}

field::value file_server::negotiate_content_type(server::request &req, server::responder &res, std::initializer_list<std::string_view> list)
{
	if (list.begin() != list.end()) {
		auto content_type = req.headers.match_preferred_content_type(list);

		if (content_type) {
			return content_type;
		}

		if (!opts_.honor_406()) {
			logger_.log_u(fz::logmsg::debug_warning, L"Content-Type \"%s\" was not agreed upon, but honor_406 is set to false. Picking the first one in the list.");
			return *list.begin();
		}

		res.send_status(406, "Not Acceptable") &&
		res.send_body(fz::sprintf("Client must accept one of: %s.\n", fz::join<std::string>(list, ", ")));
	}

	return {};
}

result file_server::send_file(server::request &req, server::responder &res, std::string_view path)
{
	tvfs::file_holder file;
	auto result = tvfs_.open_file(file, path, file::reading, 0);


	if (result) {
		auto content_type = negotiate_content_type(req, res, {mime_from_name(req.headers.get(headers::X_FZ_INT_File_Name, path))});

		if (content_type) {
			res.send_status(200, "Ok") &&
			res.send_header(http::headers::Content_Type, content_type) &&
			res.send_header(http::headers::Last_Modified, file->get_modification_time().get_rfc822()) &&
			res.send_header(http::headers::Vary, http::headers::Accept) &&
			send_disposition_header(req, res) &&
			res.send_body(std::move(file));
		}
	}

	return result;
}

bool file_server::send_disposition_header(server::request &req, server::responder &res)
{
	auto disposition = [&]() -> std::string_view {
		if (!req.uri.query_.empty()) {
			query_string q(req.uri.query_);
			if (q.pairs().count("download")) {
				return "attachment";
			}
		}

		return "inline";
	}();

	if (auto n = req.headers.get(headers::X_FZ_INT_File_Name)) {
		return res.send_header(http::headers::Content_Disposition, fz::sprintf("%s; filename*=UTF-8''%s", disposition, percent_encode(n)));
	}

	return res.send_header(http::headers::Content_Disposition, disposition);
}

void file_server::do_get(server::request &req, server::responder &res)
{
	tvfs::entries_iterator it;
	auto result = tvfs_.get_entries(it, req.uri.path_, tvfs::traversal_mode::only_children);

	if (result)	{
		if (opts_.can_list_dir() || !opts_.default_index().empty()) {
			bool slash_appended = false;

			if (req.uri.path_.back() != '/') {
				req.uri.path_.append(1, '/');
				slash_appended = true;
			}

			if (!opts_.default_index().empty()) {
				for (auto &index: opts_.default_index()) {
					if (index.empty() || index.find("/") != std::string::npos) {
						logger_.log(fz::logmsg::warning, L"One of the provided default index files is invalid, skipping it.");
						continue;
					}

					if (send_file(req, res, req.uri.path_ + index)) {
						return;
					}
				}
			}

			if (opts_.can_list_dir()) {
				if (slash_appended) {
					auto location = percent_encode(req.headers.get(headers::X_FZ_INT_Original_Path, req.uri.path_), true) + '/';
					if (!req.uri.query_.empty()) {
						location += '?';
						location += req.uri.query_;
					}

					// If there's no slash at the end of the bearer, redirect to the same url, with the slash appended.
					res.send_status(301, "Moved Permanently") &&
					res.send_header(http::headers::Location, location) &&
					res.send_end();

					return;
				}

				auto content_type = negotiate_content_type(req, res, {
					"text/html",
					"text/plain",
					"application/ndjson"
				});

				if (content_type) {
					res.send_status(200, "Ok") &&
					res.send_header(http::headers::Content_Type, content_type) &&
					res.send_header(http::headers::Vary, http::headers::Accept) && [&] {
						if (it.mtime()) {
							return res.send_header(http::headers::Last_Modified, it.mtime().get_rfc822());
						}

						return true;
					}() &&
					send_disposition_header(req, res) &&
					res.send_body(std::move(it));
				}

				return;
			}
		}

		send_response_from_result(res, {fz::result::noperm});
		return;
	}

	if (result.error_ == result.nodir) {
		auto result = send_file(req, res, req.uri.path_);
		if (result) {
			return;
		}
	}

	send_response_from_result(res, result);
}

void file_server::do_put(server::request &req, server::responder &res)
{
	if (auto action = req.headers.get(headers::X_FZ_Action)) {
		if (action.is("mkdir")) {
			return do_put_mkdir(req, res);
		}

		if (action.is("copy-from")) {
			if (auto source = action.get_param("path"); source && *source) {
				return do_put_copy(req, res, *source);
			}
		}

		logger_.log(logmsg::error, L"Invalid %s header.", headers::X_FZ_Action);

		res.send_status(404, "Bad Request") &&
		res.send_end();

		return;
	}

	tvfs::file_holder file;
	auto result = tvfs_.open_file(file, req.uri.path_, file::writing, 0);

	if (result) {
		req.receive_body(std::move(file), [&res](tvfs::file_holder, bool success) {
			if (success) {
				res.send_status(204, "No Content") &&
				res.send_end();

				return;
			}

			res.send_status(500, "Internal Server Error") &&
			res.send_header(http::headers::Connection, "close") &&
			res.send_end();
		});

		return;
	}

	send_response_from_result(res, result);
}

void file_server::do_delete(server::request &req, server::responder &res)
{
	fz::result result;

	if (req.uri.path_.back() == '/') {
		result = tvfs_.remove_directory(req.uri.path_, req.headers.get(headers::X_FZ_Recursive) == "true");
	}
	else {
		result = tvfs_.remove_file(req.uri.path_);

		if (result.error_ == fz::result::nofile) {
			result = tvfs_.remove_directory(req.uri.path_, req.headers.get(headers::X_FZ_Recursive) == "true");
		}
	}

	send_response_from_result(res, result);
}

void file_server::do_post(server::request &req, server::responder &res)
{
	auto result = tvfs_.get_entry(req.uri.path_);
	if (!result.first) {
		return send_response_from_result(res, result.first);
	}

	if (result.second.type() != local_filesys::dir) {
		return send_not_allowed_response(res, verbs::POST);
	}

	if (auto actions = req.headers.get(headers::X_FZ_Action).as_list()) {
		auto from = actions.get("move-from").get_param("path");
		auto to = actions.get("move-to").get_param("path");

		if (from && to && *from && *to) {
			auto cwd = util::fs::absolute_unix_path(req.uri.path_);

			auto result = tvfs_.rename(cwd / percent_decode_s(*from, false, true), cwd / percent_decode_s(*to, false, true));
			return send_response_from_result(res, result);
		}

		logger_.log(logmsg::error, L"Invalid %s header.", headers::X_FZ_Action);
	}
	else {
		logger_.log(logmsg::error, L"Missing required %s header.", headers::X_FZ_Action);
	}

	res.send_status(404, "Bad Request") &&
	res.send_end();
}

void file_server::do_put_mkdir(server::request &req, server::responder &res)
{
	auto result = tvfs_.make_directory(req.uri.path_);
	if (!result.first && result.first.raw_ == FZ_RESULT_RAW_ALREADY_EXISTS) {
		// PUT is indepotent, hence it's fine if the directory already exists
		result.first = { result::ok };
	}

	send_response_from_result(res, result.first);
}

void file_server::do_put_copy(server::request &, server::responder &res, std::string_view)
{
	res.send_status(501, "Not Implemented") &&
	res.send_end();
}

void file_server::handle_transaction(const server::shared_transaction &t) {
	auto &req = t->req();
	auto &res = t->res();

	logger_.log_u(logmsg::debug_verbose, L"Id: %d", req.get_session_id());

	for (auto &h: t->req().headers) {
		logger_.log_u(logmsg::debug_verbose, L"H: %s: %s", h.first, h.second);
	}

	logger_.log_u(logmsg::debug_info, L"PATH: %s", req.uri.path_);

	if (!req.uri.is_absolute() || !req.uri.get_authority(true).empty()) {
		res.send_status(400, "Bad Request") &&
		res.send_header(http::headers::Connection, "close") &&
		res.send_end();

		return;
	}

	bool method_allowed{};

	bool must_get{};
	bool must_put{};
	bool must_delete{};
	bool must_post{};

	if (req.method == "GET" || req.method == "HEAD") {
		method_allowed = must_get = opts_.can_get();
	}
	else
	if (req.method == "PUT") {
		method_allowed = must_put = opts_.can_put();
	}
	else
	if (req.method == "DELETE") {
		method_allowed = must_delete = opts_.can_delete();
	}
	else
	if (req.method == "POST") {
		method_allowed = must_post = opts_.can_post();
	}

	if (!method_allowed) {
		return send_not_allowed_response(res);
	}

	if (req.headers.get(fz::http::headers::Expect) == "100-continue") {
		// 100 is the only result we can send as many times as we want.
		res.send_status(100, "Continue");
	}

	if (must_get) {
		return do_get(req, res);
	}

	if (must_put) {
		return do_put(req, res);
	}

	if (must_delete) {
		return do_delete(req, res);
	}

	if (must_post) {
		return do_post(req, res);
	}

	res.send_status(500, "Internal Server Error") &&
	res.send_header(http::headers::Connection, "close") &&
	res.send_body(fz::sprintf("Unexpected internal state at %s:%d\n", __FILE__, __LINE__)) &&
	res.send_end();
}

local_filesys::type file_server::get_file_type_or_send_error(std::string_view path, server::responder &res)
{
	auto f = tvfs_.get_entry(path);
	if (!f.first) {
		send_response_from_result(res, f.first);
		return local_filesys::unknown;
	}

	return f.second.type();
}

file_server::file_server(tvfs::engine &tvfs, logger_interface &logger, options opts)
	: opts_(std::move(opts))
	, tvfs_(tvfs)
	, logger_(logger)
{}

}
