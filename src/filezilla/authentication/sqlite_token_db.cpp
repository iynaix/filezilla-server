#include "sqlite_token_db.hpp"
#include <libfilezilla/encode.hpp>
#include <libfilezilla/json.hpp>
#include "../logger/type.hpp"
#include "../serialization/archives/binary.hpp"
#include "../serialization/types/containers.hpp"

namespace fz::authentication {

sqlite_token_db::sqlite_token_db(const native_string &db_path, logger_interface &logger)
	: logger_(logger, "SQLite Token DB")
	, db_path_(db_path)

{
	initialize_db();
	prepare_statements();
	load_symmetric_key();
}

sqlite_token_db::~sqlite_token_db() {
	finalize_statements();
	deinitialize_db();
}

void sqlite_token_db::initialize_db() {
	if (sqlite3_open(fz::to_string(db_path_).c_str(), &db_) != SQLITE_OK) {
		logger_.log_u(logmsg::error, L"Could not open the SQLite DB [%s]", db_path_);
		return deinitialize_db();
	}
	else {
		logger_.log_u(logmsg::debug_info, L"Successfully opened SQLite DB [%s]", db_path_);
	}

	const char *sql = R"(
		CREATE TABLE IF NOT EXISTS tokens (
			refresh_id INTEGER,
			username TEXT,
			path TEXT,
			must_impersonate INTEGER,
			created_at INTEGER,
			expires_at INTEGER
		);

		CREATE TABLE IF NOT EXISTS key_storage (
			key TEXT
		);
	)";

	char *err_msg = nullptr;

	if (sqlite3_exec(db_, sql, nullptr, nullptr, &err_msg) != SQLITE_OK) {
		logger_.log_u(logmsg::error, L"Failed to initialize SQLite database: %s.", err_msg);
		sqlite3_free(err_msg);
		return deinitialize_db();
	}
}

void sqlite_token_db::deinitialize_db()
{
	if (db_) {
		sqlite3_close(db_);
		db_ = {};
	}
}

void sqlite_token_db::load_symmetric_key() {
	if (!db_) {
		return;
	}

	if (sqlite3_step(load_key_stmt_) == SQLITE_ROW) {
		auto key_data = reinterpret_cast<const char*>(sqlite3_column_text(load_key_stmt_, 0));
		key_ = symmetric_key::from_base64(key_data);
		sqlite3_reset(load_key_stmt_);
	} else {
		key_ = symmetric_key::generate();
		save_symmetric_key();
	}
}

void sqlite_token_db::save_symmetric_key() {
	auto key_data = key_.to_base64();

	sqlite3_bind_text(save_key_stmt_, 1, key_data.c_str(), -1, SQLITE_STATIC);
	sqlite3_step(save_key_stmt_);
	sqlite3_reset(save_key_stmt_);
}

void sqlite_token_db::prepare_statements() {
	static const char select_sql[]       = "SELECT * FROM tokens WHERE rowid = ?";
	static const char insert_sql[]       = "INSERT INTO tokens (refresh_id, username, path, must_impersonate, created_at, expires_at) VALUES (?, ?, ?, ?, ?, ?)";
	static const char delete_sql[]       = "DELETE FROM tokens WHERE rowid = ?";
	static const char update_sql[]       = "UPDATE tokens SET refresh_id = ?, username = ?, path = ?, must_impersonate = ?, created_at = ?, expires_at = ? WHERE rowid = ?";
	static const char load_key_sql[]     = "SELECT key FROM key_storage LIMIT 1";
	static const char save_key_sql[]     = "INSERT INTO key_storage (key) VALUES (?)";
	static const char reset_tokens_sql[] = "DELETE FROM tokens";
	static const char reset_key_sql[]    = "DELETE FROM key_storage";

	if (!db_) {
		return;
	}

	sqlite3_prepare_v2(db_, select_sql, -1, &select_stmt_, nullptr);
	sqlite3_prepare_v2(db_, insert_sql, -1, &insert_stmt_, nullptr);
	sqlite3_prepare_v2(db_, delete_sql, -1, &delete_stmt_, nullptr);
	sqlite3_prepare_v2(db_, update_sql, -1, &update_stmt_, nullptr);
	sqlite3_prepare_v2(db_, load_key_sql, -1, &load_key_stmt_, nullptr);
	sqlite3_prepare_v2(db_, save_key_sql, -1, &save_key_stmt_, nullptr);
	sqlite3_prepare_v2(db_, reset_tokens_sql, -1, &reset_tokens_stmt_, nullptr);
	sqlite3_prepare_v2(db_, reset_key_sql, -1, &reset_key_stmt_, nullptr);
}

void sqlite_token_db::finalize_statements() {
	sqlite3_finalize(select_stmt_);
	sqlite3_finalize(insert_stmt_);
	sqlite3_finalize(delete_stmt_);
	sqlite3_finalize(update_stmt_);

	sqlite3_finalize(load_key_stmt_);
	sqlite3_finalize(save_key_stmt_);
	sqlite3_finalize(reset_tokens_stmt_);
	sqlite3_finalize(reset_key_stmt_);
}

static const datetime datetime_0(0, datetime::milliseconds);

token sqlite_token_db::select(std::uint64_t id) {
	sqlite3_bind_int64(select_stmt_, 1, std::int64_t(id));

	token t;

	int res = sqlite3_step(select_stmt_);

	if (res == SQLITE_ROW) {
		t.refresh.access.id = id;
		t.refresh.access.refresh_id = std::uint64_t(sqlite3_column_int64(select_stmt_, 0));
		t.refresh.username = reinterpret_cast<const char*>(sqlite3_column_text(select_stmt_, 1));
		t.refresh.path = reinterpret_cast<const char*>(sqlite3_column_text(select_stmt_, 2));
		t.must_impersonate = sqlite3_column_int(select_stmt_, 3);
		t.created_at = datetime_0 + duration::from_milliseconds(sqlite3_column_int64(select_stmt_, 4));
		t.expires_at = datetime_0 + duration::from_milliseconds(sqlite3_column_int64(select_stmt_, 5));
	}

	sqlite3_reset(select_stmt_);

	if (res != SQLITE_ROW) {
		logger_.log_u(logmsg::error, L"Could not select. Error: %d.", res);
		return {};
	}

	return t;
}

bool sqlite_token_db::remove(uint64_t id) {
	sqlite3_bind_int64(delete_stmt_, 1, std::int64_t(id));

	int res = sqlite3_step(delete_stmt_);

	sqlite3_reset(delete_stmt_);

	if (res != SQLITE_DONE) {
		logger_.log_u(logmsg::warning, L"Was not able to delete the token with id %d. Error: %d.", id, res);
		return false;
	}

	return true;
}

bool sqlite_token_db::update(const token &t)
{
	sqlite3_bind_int64(update_stmt_, 1, std::int64_t(t.refresh.access.refresh_id));
	sqlite3_bind_text(update_stmt_, 2, t.refresh.username.c_str(), -1, SQLITE_STATIC);
	sqlite3_bind_text(update_stmt_, 3, t.refresh.path.c_str(), -1, SQLITE_STATIC);
	sqlite3_bind_int(update_stmt_, 4, t.must_impersonate);
	sqlite3_bind_int64(update_stmt_, 5, (t.created_at - datetime_0).get_milliseconds());
	sqlite3_bind_int64(update_stmt_, 6, (t.expires_at - datetime_0).get_milliseconds());
	sqlite3_bind_int64(update_stmt_, 7, std::int64_t(t.refresh.access.id));

	int res = sqlite3_step(update_stmt_);

	sqlite3_reset(update_stmt_);

	if (res != SQLITE_DONE) {
		logger_.log_u(logmsg::warning, L"Was not able to update the token with id (%d,%d). Error: %d.", t.refresh.access.id, t.refresh.access.refresh_id, res);
		return false;
	}

	return true;
}

token sqlite_token_db::insert(std::string name, std::string path, bool needs_impersonation, fz::duration expires_in) {
	auto now = datetime::now();
	auto t = token{ { { 0,  1 }, std::move(name), std::move(path) }, needs_impersonation, now, expires_in ? now+expires_in : datetime() };

	sqlite3_bind_int64(insert_stmt_, 1, std::int64_t(t.refresh.access.refresh_id));
	sqlite3_bind_text(insert_stmt_, 2, t.refresh.username.c_str(), -1, SQLITE_STATIC);
	sqlite3_bind_text(insert_stmt_, 3, t.refresh.path.c_str(), -1, SQLITE_STATIC);
	sqlite3_bind_int(insert_stmt_, 4, t.must_impersonate);
	sqlite3_bind_int64(insert_stmt_, 5, (t.created_at-datetime_0).get_milliseconds());
	sqlite3_bind_int64(insert_stmt_, 6, (t.expires_at-datetime_0).get_milliseconds());

	int res = sqlite3_step(insert_stmt_);

	sqlite3_reset(insert_stmt_);

	if (res == SQLITE_DONE) {
		t.refresh.access.id = std::uint64_t(sqlite3_last_insert_rowid(db_));
	} else {
		logger_.log_u(logmsg::warning, L"Was not able to insert the token with id (%d,%d). Error: %d.", t.refresh.access.id, t.refresh.access.refresh_id, res);
		t = {};
	}

	return t;
}

void sqlite_token_db::reset() {
	sqlite3_step(reset_tokens_stmt_);
	sqlite3_reset(reset_tokens_stmt_);

	sqlite3_step(reset_key_stmt_);
	sqlite3_reset(reset_key_stmt_);

	key_ = symmetric_key::generate();
	save_symmetric_key();
}

const symmetric_key &sqlite_token_db::get_symmetric_key() {
	return key_;
}

} // namespace fz::authentication
