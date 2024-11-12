#include <memory>

#include <libfilezilla/local_filesys.hpp>
#include "filesystem.hpp"
#include "strsyserror.hpp"

#if !defined(FZ_WINDOWS) || !FZ_WINDOWS
#	include <unistd.h>
#	include <sys/stat.h>
#	include <cerrno>
#	include <pwd.h>
#else
#	include <windows.h>
#	include <aclapi.h>
#	include <sddl.h>
#	include "scope_guard.hpp"
#endif

#include "parser.hpp"

namespace fz::util::fs {

namespace {

template <typename Char>
static constexpr Char dotc[] = { '.', '\0' };
template <typename Char>
static constexpr Char dotdotc[] = { '.', '.', '\0' };
template <typename Char>
static const auto dot = std::basic_string_view<Char>(dotc<Char>);
template <typename Char>
static const auto dotdot = std::basic_string_view<Char>(dotdotc<Char>);

}

template<typename Char, path_format Format, path_kind Kind>
basic_path<Char, Format, Kind>::basic_path(String && string, path_format invalid_chars_within_path_elements_as_in_path_format)
	: string_(std::move(string))
{
	normalize();
	validate(invalid_chars_within_path_elements_as_in_path_format);
}

template<typename Char, path_format Format, path_kind Kind>
std::vector<std::basic_string_view<Char>> basic_path<Char, Format, Kind>::elements_view() const
{
	auto pos = first_after_root();
	if (pos != String::npos) {
		return strtok_view(View(string_).substr(pos), Util::separator(), true);
	}

	return {};
}

template<typename Char, path_format Format, path_kind Kind>
std::vector<std::basic_string<Char>> basic_path<Char, Format, Kind>::elements() const
{
	auto pos = first_after_root();
	if (pos != String::npos) {
		return strtok(View(string_).substr(pos), Util::separator(), true);
	}

	return {};
}

template<typename Char, path_format Format, path_kind Kind>
fz::file basic_path<Char, Format, Kind>::open(file::mode mode, file::creation_flags flags) const
{
	if (mode == file::mode::writing) {
		auto mkd_flag = fz::mkdir_permissions::normal;

		if (flags & file::current_user_only)
			mkd_flag = fz::mkdir_permissions::cur_user;
		else
		if (flags & file::current_user_and_admins_only)
			mkd_flag = fz::mkdir_permissions::cur_user_and_admins;

		parent().mkdir(true, mkd_flag);
	}

	return {fz::to_native(string_), mode, flags};
}

template<typename Char, path_format Format, path_kind Kind>
result basic_path<Char, Format, Kind>::mkdir(bool recurse, mkdir_permissions permissions, native_string *last_created) const
{
	return fz::mkdir(to_native(string_), recurse, permissions, last_created);
}

template<typename Char, path_format Format, path_kind Kind>
template<typename C, path_format F, std::enable_if_t<std::is_same_v<C, native_string::value_type> && F == native_format>*>
bool basic_path<Char, Format, Kind>::check_ownership(path_ownership ownership, logger_interface &logger) const
{
#if !defined(FZ_WINDOWS) || !FZ_WINDOWS

	static auto get_name_from_uid = [](::uid_t uid) -> std::string {
		struct ::passwd pwd;
		struct ::passwd *result{};

		std::vector<char> buf(1024);

		while (::getpwuid_r(uid, &pwd, buf.data(), buf.size(), &result) == ERANGE) {
			buf.resize(buf.size()*2);
		}

		if (result) {
			return pwd.pw_name;
		}

		return {};
	};

	auto check = [&](const struct stat &st) {
		auto username = get_name_from_uid(st.st_uid);
		logger.log(logmsg::status, L"Owner of `%s': %s", string_, username.empty() ? "<could not retrieve>" : username);

		if (st.st_uid == geteuid()) {
			return true;
		}

		if (ownership == user_or_admin_ownership) {
			if (st.st_uid == 0) {
				return true;
			}

		#ifdef FZ_UTIL_FILESYSTEM_NIX_ADMIN_UID
			if (st.st_uid == FZ_UTIL_FILESYSTEM_NIX_ADMIN_UID) {
				return true;
			}
		#endif
		}

		return false;
	};

	struct stat stat_result;

	if (::lstat(string_.c_str(), &stat_result) == -1) {
		auto err = errno;
		logger.log(logmsg::error, L"lstat(%s) failed: %s (%d).", string_.c_str(), strsyserror(err), err);
		return false;
	}

	if (!check(stat_result)) {
		return false;
	}

	if (S_ISLNK(stat_result.st_mode)) {
		if (::stat(string_.c_str(), &stat_result) == -1) {
			auto err = errno;
			logger.log(logmsg::error, L"stat(%s) failed: %s (%d).", string_.c_str(), strsyserror(err), err);
			return false;
		}

		if (!check(stat_result)) {
			return false;
		}
	}

	return true;
#else
	auto self = [&] {
		std::vector<TOKEN_USER> ret;

		HANDLE handle = nullptr;
		if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &handle)) {
			DWORD size = 0;
			GetTokenInformation(handle, TokenUser, nullptr, 0, &size);
			ret.resize(size/sizeof(TOKEN_USER) + size/sizeof(TOKEN_USER));

			if (!GetTokenInformation(handle, TokenUser, ret.data(), size, &size)) {
				auto err = GetLastError();
				logger.log(logmsg::error, L"GetTokenInformation(self) failed: %s (%d)", strsyserror(err), err);
				ret.clear();
			}

			CloseHandle(handle);
		}
		else {
			auto err = GetLastError();
			logger.log(logmsg::error, L"OpenProcessToken(GetCurrentProcess()) failed: %s (%d)", strsyserror(err), err);
		}

		return ret;
	}();

	if (self.empty()) {
		return false;
	}

	auto admin = [&] {
		std::vector<BYTE> ret;

		DWORD size = 0;
		CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, nullptr, &size);
		ret.resize(size);

		if (!CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, ret.data(), &size)) {
			auto err = GetLastError();
			logger.log(logmsg::error, L"CreateWellKnownSid(WinBuiltinAdministratorsSid) failed: %s (%d)", strsyserror(err), err);

			ret.clear();
		}

		return ret;
	}();

	if (admin.empty()) {
		return false;
	}

	auto system = [&] {
		std::vector<BYTE> ret;

		DWORD size = 0;
		CreateWellKnownSid(WinLocalSystemSid, nullptr, nullptr, &size);
		ret.resize(size);

		if (!CreateWellKnownSid(WinLocalSystemSid, nullptr, ret.data(), &size)) {
			auto err = GetLastError();
			logger.log(logmsg::error, L"CreateWellKnownSid(WinLocalSystemSid) failed: %s (%d)", strsyserror(err), err);
			ret.clear();
		}

		return ret;
	}();

	if (system.empty()) {
		return false;
	}

	auto network = [&] {
		std::vector<BYTE> ret;

		DWORD size = 0;
		CreateWellKnownSid(WinNetworkServiceSid, nullptr, nullptr, &size);
		ret.resize(size);

		if (!CreateWellKnownSid(WinNetworkServiceSid, nullptr, ret.data(), &size)) {
			auto err = GetLastError();
			logger.log(logmsg::error, L"CreateWellKnownSid(WinNetworkServiceSid) failed: %s (%d)", strsyserror(err), err);
			ret.clear();
		}

		return ret;
	}();

	if (network.empty()) {
		return false;
	}

	auto trusted_installer = [&] {
		struct deleter {
			using pointer = PSID;
			void operator()(pointer p) {
				LocalFree(p);
			}
		};

		std::unique_ptr<PSID, deleter> ret;

		PSID sid{};
		if (!ConvertStringSidToSidW(L"S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464", &sid)) {
			auto err = GetLastError();
			logger.log(logmsg::error, L"ConvertStringSidToSidW(L\"S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464\") failed: %s (%d)", strsyserror(err), err);
		}

		ret.reset(sid);

		return ret;
	}();

	if (!trusted_installer) {
		return false;
	}

	auto check = [&](PSID sid) {
		if (EqualSid(sid, self[0].User.Sid)) {
			return true;
		}

		if (ownership == user_or_admin_ownership) {
			if (EqualSid(sid, admin.data()) || EqualSid(sid, system.data()) || EqualSid(sid, network.data()) || EqualSid(sid, trusted_installer.get())) {
				return true;
			}
		}

		return false;
	};

	static auto get_name_from_sid = [](PSID sid) -> std::wstring {
		DWORD name_size{}, domain_size{};
		SID_NAME_USE use;

		::LookupAccountSidW(nullptr, sid,
			nullptr, &name_size,
			nullptr, &domain_size,
			&use);

		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
			return {};
		}

		std::wstring name(name_size, L'\0');
		std::wstring domain(domain_size, L'\0');

		if (::LookupAccountSidW(nullptr, sid,
								name.data(), &name_size,
								domain.data(), &domain_size,
								&use)
		) {
			name.pop_back();
			domain.pop_back();

			return domain + L"\\" + name;
		}

		return {};
	};

	struct file_info {
		PSID owner{};
		bool is_link{};

		file_info(const fz::native_string &path, bool follow_link, logger_interface &logger) {
			DWORD flags = FILE_FLAG_BACKUP_SEMANTICS;
			if (!follow_link) {
				flags |= FILE_FLAG_OPEN_REPARSE_POINT;
			}

			HANDLE handle = CreateFileW(
				path.c_str(),
				READ_CONTROL | FILE_READ_ATTRIBUTES | FILE_READ_EA,
				FILE_SHARE_READ,
				nullptr,
				OPEN_EXISTING,
				flags,
				nullptr
			);

			if (handle == INVALID_HANDLE_VALUE) {
				auto err = GetLastError();
				logger.log(logmsg::error, L"CreateFileW(%s) failed: %s (%d)", path, strsyserror(err), err);

				return;
			}

			FZ_SCOPE_GUARD {
				CloseHandle(handle);
			};

			BY_HANDLE_FILE_INFORMATION info;

			if (!GetFileInformationByHandle(handle, &info)) {
				auto err = GetLastError();
				logger.log(logmsg::error, L"GetFileInformationByHandle(%s) failed: %s (%d)", path, strsyserror(err), err);
				return;
			}

			is_link = info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT;

			if (GetSecurityInfo(handle, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION, &owner, nullptr, nullptr, nullptr, &sd) != ERROR_SUCCESS) {
				auto err = GetLastError();
				logger.log(logmsg::error, L"GetSecurityInfo(%s) failed: %s (%d)", path, strsyserror(err), err);
				owner = {};
			}

			if (owner) {
				auto username = get_name_from_sid(owner);
				logger.log(logmsg::status, L"Owner of `%s': %s", path, username.empty() ? L"<could not retrieve>" : username);
			}
		}

		file_info &operator=(file_info &&) = delete;
		file_info() = delete;

		~file_info()
		{
			LocalFree(sd);
		}

	private:
		PSECURITY_DESCRIPTOR sd{};
	};

	auto info = file_info(string_, false, logger);
	if (!info.owner || !check(info.owner)) {
		return false;
	}

	if (info.is_link) {
		logger.log(logmsg::status, L"%s is a symlink.", string_);

		auto info = file_info(string_, true, logger);
		if (!info.owner || !check(info.owner)) {
			return false;
		}
	}

	return true;
#endif
}

template<typename Char, path_format Format, path_kind Kind>
local_filesys::type basic_path<Char, Format, Kind>::type(bool follow_links) const
{
	return local_filesys::get_file_type(fz::to_native(string_), follow_links);
}

template<typename Char, path_format Format, path_kind Kind>
basic_path<Char, Format, Kind> basic_path<Char, Format, Kind>::if_openable(file::mode mode, file::creation_flags flags) &&
{
	if (!open(mode, flags))
		string_.clear();

	return std::move(*this);
}

template<typename Char, path_format Format, path_kind Kind>
basic_path<Char, Format, Kind> basic_path<Char, Format, Kind>::if_base() &&
{
	if (!is_base())
		string_.clear();

	return std::move(*this);
}

FZ_UTIL_PARSER( ) drive_letter_followed_by_slash(Range &r)
{
	return (lit(r, 'A', 'Z') || lit(r, 'a', 'z')) && lit(r, ':') && (lit(r, '\\') || lit(r, '/'));
}

FZ_UTIL_PARSER( ) parse_dos_dev_path(Range &r, View &specifier, View &server, View &share, CharT &sep, bool &is_unc) {
	if (seq(r, View(fzS(CharT, "\\\\"))))
		sep = '\\';
	else
	if (seq(r, View(fzS(CharT, "//"))))
		sep = '/';
	else
		return false;

	if (!(parse_until_lit(r, server, {sep}) && lit(r, sep)) || server.empty())
		return false;

	if (!(parse_until_lit(r, share, {sep}, true)) || share.empty())
		return false;

	lit(r, sep);

	is_unc = true;

	if (server == View(fzS(CharT, ".")) || server == View(fzS(CharT, "?"))) {
		is_unc = false;
		specifier = server;
		server = {};

		if (fz::equal_insensitive_ascii(share, fzS(CharT, "UNC"))) {
			share = {};

			if (!(lit(r, sep) && parse_until_lit(r, server, {sep}) && lit(r, sep)) || server.empty())
				return false;

			if (!(parse_until_lit(r, share, {sep}, true)) || share.empty())
				return false;

			is_unc = true;
		}
	}

	return true;
}

template <typename Char, path_format Format, path_kind Kind>
template <auto F, std::enable_if_t<F == unix_format>*>
std::size_t basic_path<Char, Format, Kind>::first_after_root() const
{
	if (string_.empty())
		return String::npos;

	return string_[0] == '/' ? 1 : 0;
}

template <typename Char, path_format Format, path_kind Kind>
template <auto F, std::enable_if_t<F == windows_format>*>
std::size_t basic_path<Char, Format, Kind>::first_after_root(View *pspecifier, bool *pis_unc) const
{
	if (string_.empty())
		return string_.npos;

	// See: https://docs.microsoft.com/en-us/dotnet/standard/io/file-path-formats
	if (parseable_range r(string_); drive_letter_followed_by_slash(r))
		return std::size_t(std::addressof(*r.it) - std::addressof(*string_.cbegin()));

	View specifier, server, share;
	Char sep{}; bool is_unc;

	if (parseable_range r(string_); parse_dos_dev_path(r, specifier, server, share, sep, is_unc)) {
		if (pspecifier)
			*pspecifier = specifier;

		if (pis_unc)
			*pis_unc = is_unc;

		return std::size_t(std::addressof(*r.it) - std::addressof(*string_.cbegin()));
	}

	return 0;
}

template <typename Char, path_format Format, path_kind Kind>
bool basic_path<Char, Format, Kind>::is_absolute() const
{
	if constexpr (Kind == absolute_kind) {
		return !string_.empty();
	}
	else
	if constexpr (Kind == relative_kind) {
		return false;
	}
	else {
		auto pos = first_after_root();
		return pos != View::npos && pos > 0;
	}
}

template <typename Char, path_format Format, path_kind Kind>
void basic_path<Char, Format, Kind>::validate(path_format invalid_chars_within_path_elements_as_in_path_format [[maybe_unused]])
{
	auto is_valid = [&] {
		if (string_.find(typename View::value_type('\0')) != View::npos)
			return false;

		static auto check_chars = [](View str, View extra = {}, std::size_t start = 0) {
			for (
				std::size_t beg = start, end;
				beg < str.size();
				beg = end + 1
			) {
				auto it = std::find_if(&str[beg], &str.back()+1, Util::is_separator);
				end = std::size_t(it-&str[0]);

				View e(&str[beg], end-beg);

				if (e.empty()) {
					continue;
				}

				if (e == dot<Char> || e == dotdot<Char>) {
					continue;
				}

				if (e.back() == '.' || e.back() == ' ') {
					return false;
				}

				if (e.find_first_of(extra) != View::npos) {
					return false;
				}
			}

			return true;
		};

		static auto check_kind = [](std::size_t pos_after_root) {
			if (pos_after_root == String::npos) {
				return false;
			}

			if constexpr (Kind == absolute_kind) {
				if (pos_after_root == 0) {
					return false;
				}
			}
			else
			if constexpr (Kind == relative_kind) {
				if (pos_after_root != 0) {
					return false;
				}
			}

			return true;
		};

		if constexpr (Format == windows_format) {
			View specifier;	bool is_unc{};

			auto pos_after_root = first_after_root(&specifier, &is_unc);
			if (!check_kind(pos_after_root)) {
				return false;
			}

			if (!specifier.empty() && specifier != fzS(Char, ".")) {
				return false;
			}

			if (specifier == fzS(Char, ".") && !is_unc) {
				return false;
			}

			// Disallow colons in the whole string, spaces and dots only at the end of path elements.
			return check_chars(string_, fzS(Char, ":"), pos_after_root);
		}
		else {
			auto pos_after_root = first_after_root();
			if (!check_kind(pos_after_root)) {
				return false;
			}

			if (invalid_chars_within_path_elements_as_in_path_format == windows_format) {
				// Disallow backslashes and colons in the whole string, spaces and dots only at the end of path segments.
				return check_chars(string_, fzS(Char, "\\:"));
			}

			return true;
		}
	};

	if (!is_valid()) {
		string_.clear();
	}
}

template <typename Char, path_format Format, path_kind Kind>
void basic_path<Char, Format, Kind>::normalize()
{
	if constexpr (Format == windows_format) {
		replace_substrings(string_, '/', '\\');

		const static String unc = fzS(Char, "\\\\.\\UNC\\");
		const static String double_slashes = fzS(Char, "\\\\");

		if (fz::starts_with<true>(string_, unc)) {
			string_.replace(0, unc.size(), double_slashes);
		}
	}

	if (auto pos_after_root = first_after_root(); pos_after_root != String::npos) {
		// Remove .. and .
		bool is_relative = pos_after_root == 0;

		if constexpr (Kind != any_kind) {
			if (is_relative != (Kind == relative_kind)) {
				string_.clear();
				return;
			}
		}

		if constexpr (Format == windows_format) {
			// A backslash at the beginning of a windows relative path
			// refers to the root of the current drive, so we need to preserve it.
			// However, in this case the dot-dot's must be treated the same way as in absolute paths.
			if (is_relative && string_[0] == Char('\\')) {
				pos_after_root = 1;
				is_relative = false;
			}
		}

		auto beg = pos_after_root, wbeg = beg;

		do {
			auto end = std::min(string_.find(Util::separator(), beg), string_.size());
			View e(&string_[beg], end-beg);

			if (e == dotdot<Char>) {
				if (wbeg > pos_after_root+1) {
					wbeg -= 2;
					if (wbeg > 0) {
						wbeg = string_.rfind(Util::separator(), wbeg)+1;
					}
				}
				else
				// If the path is relative, do not remove the leading sequence of dot-dot's.
				if (is_relative) {
					pos_after_root = wbeg = end + (end < string_.size());
				}
			}
			else
			if (!e.empty() && e != dot<Char>) {
				if (wbeg < beg) {
					std::copy(e.begin(), e.end(), &string_[wbeg]);
					wbeg += e.size();

					if (end < string_.size())
						string_[wbeg++] = string_[end];
				}
				else {
					wbeg = end + (end < string_.size());
				}
			}

			beg = end + 1;
		} while (beg < string_.size());

		// Remove any trailing separator
		if (wbeg > pos_after_root && Util::is_separator(string_[wbeg-1])) {
			wbeg -= 1;
		}

		string_.resize(wbeg);
		if (string_.empty())
			string_ = dot<Char>;
	}
}

template <typename Char, path_format Format, path_kind Kind>
basic_path<Char, Format, relative_kind> basic_path<Char, Format, Kind>::base(bool remove_suffixes) const
{
	auto begin = std::find_if(string_.rbegin(), string_.rend(), is_separator).base();
	auto ret = std::basic_string_view<Char>(&*begin, std::size_t(string_.end()-begin));

	if (ret == dot<Char> || ret == dotdot<Char>) {
		ret = {};
	}
	else
	if (remove_suffixes) {
		ret = ret.substr(0, ret.find(dot<Char>));
	}

	return ret;
}

template <typename Char, path_format Format, path_kind Kind>
basic_path<Char, Format, Kind> &basic_path<Char, Format, Kind>::make_parent()
{
	auto it = string_.end();
	auto n = first_after_root();
	auto begin = n != string_.npos ? string_.begin() + std::string::difference_type(n) : string_.end();

	while (it != begin && Util::is_separator(*--it));
	while (it != begin && !Util::is_separator(*--it));
	for (;it != begin && Util::is_separator(*std::prev(it)); --it);

	string_.erase(it, string_.end());

	if (it == begin && (n == 0 || n == string_.npos))
		string_ = dot<Char>;

	return *this;
}

template <typename Char, path_format Format, path_kind Kind>
basic_path<Char, Format, Kind> basic_path<Char, Format, Kind>::parent() const &
{
	return basic_path(*this).make_parent();
}

template <typename Char, path_format Format, path_kind Kind>
basic_path<Char, Format, Kind> basic_path<Char, Format, Kind>::parent() &&
{
	return basic_path(std::move(*this)).make_parent();
}

template <typename Char, path_format Format, path_kind Kind>
bool basic_path<Char, Format, Kind>::is_base() const
{
	return !string_.empty() && string_ != dot<Char> && string_ != dotdot<Char> && std::find_if(string_.begin(), string_.end(), Util::is_separator) == string_.end();
}

template <typename Char, path_format Format, path_kind Kind>
basic_path_list<Char, Format, Kind> &basic_path_list<Char, Format, Kind>::operator+=(basic_path_list rhs)
{
	this->insert(this->end(), std::make_move_iterator(rhs.begin()), std::make_move_iterator(rhs.end()));
	return *this;
}

template <typename Char, path_format Format, path_kind Kind>
basic_path_list<Char, Format, Kind> basic_path_list<Char, Format, Kind>::operator +(basic_path_list rhs) const &
{
	return basic_path_list(*this) += std::move(rhs);
}


template <typename Char, path_format Format, path_kind Kind>
basic_path_list<Char, Format, Kind> basic_path_list<Char, Format, Kind>::operator +(basic_path_list rhs) &&
{
	return std::move(*this += std::move(rhs));
}

template class basic_path<std::string::value_type, unix_format, any_kind>;
template class basic_path_list<std::string::value_type, unix_format, any_kind>;
template class basic_path<std::wstring::value_type, unix_format, any_kind>;
template class basic_path_list<std::wstring::value_type, unix_format, any_kind>;;

template class basic_path<std::string::value_type, windows_format, any_kind>;
template class basic_path_list<std::string::value_type, windows_format, any_kind>;
template class basic_path<std::wstring::value_type, windows_format, any_kind>;
template class basic_path_list<std::wstring::value_type, windows_format, any_kind>;

template class basic_path<std::string::value_type, unix_format, absolute_kind>;
template class basic_path_list<std::string::value_type, unix_format, absolute_kind>;
template class basic_path<std::wstring::value_type, unix_format, absolute_kind>;
template class basic_path_list<std::wstring::value_type, unix_format, absolute_kind>;

template class basic_path<std::string::value_type, windows_format, absolute_kind>;
template class basic_path_list<std::string::value_type, windows_format, absolute_kind>;
template class basic_path<std::wstring::value_type, windows_format, absolute_kind>;
template class basic_path_list<std::wstring::value_type, windows_format, absolute_kind>;

template class basic_path<std::string::value_type, unix_format, relative_kind>;
template class basic_path_list<std::string::value_type, unix_format, relative_kind>;
template class basic_path<std::wstring::value_type, unix_format, relative_kind>;
template class basic_path_list<std::wstring::value_type, unix_format, relative_kind>;

template class basic_path<std::string::value_type, windows_format, relative_kind>;
template class basic_path_list<std::string::value_type, windows_format, relative_kind>;
template class basic_path<std::wstring::value_type, windows_format, relative_kind>;
template class basic_path_list<std::wstring::value_type, windows_format, relative_kind>;

template bool native_path::check_ownership(path_ownership, logger_interface &) const;
template bool absolute_native_path::check_ownership(path_ownership, logger_interface &) const;
template bool relative_native_path::check_ownership(path_ownership, logger_interface &) const;

}
