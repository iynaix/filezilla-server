#ifndef FZ_AUTHENTICATION_SQLITE_TOKEN_DB_HPP
#define FZ_AUTHENTICATION_SQLITE_TOKEN_DB_HPP

#ifdef HAVE_CONFIG_H
#   include "config_modules.hpp"
#endif

#if ENABLE_FZ_WEBUI

#include <sqlite3.h>
#include "token_manager.hpp"
#include "../logger/modularized.hpp"

namespace fz::authentication {

class sqlite_token_db : public token_db {
public:
	sqlite_token_db(const native_string &db_path, logger_interface &logger = get_null_logger());
	~sqlite_token_db() override;

	explicit operator bool() const
	{
		return bool(db_);
	}

	token select(std::uint64_t id) override;
	token insert(std::string name, std::string path, bool needs_impersonation, fz::duration expires_in) override;
	bool remove(std::uint64_t id) override;
	bool update(const token &t) override;
	void reset() override;
	const symmetric_key &get_symmetric_key() override;

private:
	logger::modularized logger_;

	void initialize_db();
	void deinitialize_db();
	void load_symmetric_key();
	void save_symmetric_key();
	void prepare_statements();
	void finalize_statements();

	sqlite3 *db_{};
	native_string db_path_;
	symmetric_key key_;

	sqlite3_stmt *select_stmt_{};
	sqlite3_stmt *insert_stmt_{};
	sqlite3_stmt *delete_stmt_{};
	sqlite3_stmt *update_stmt_{};
	sqlite3_stmt *load_key_stmt_{};
	sqlite3_stmt *save_key_stmt_{};
	sqlite3_stmt *reset_tokens_stmt_{};
	sqlite3_stmt *reset_key_stmt_{};
};

}

#endif

#endif // FZ_AUTHENTICATION_SQLITE_TOKEN_DB_HPP
