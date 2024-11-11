#ifndef FZ_STRRESULT_HPP
#define FZ_STRRESULT_HPP

#include <string_view>

#ifdef FZ_WINDOWS
#	include <winerror.h>
#else
#	include <cerrno>
#endif

#include <libfilezilla/fsresult.hpp>

namespace fz {

std::string_view strresult(result r);
std::string_view strresult(rwresult r);

}

#ifdef FZ_RESULT_RAW
#	error "FZ_RESULT_RAW already defined"
#else
#	ifdef FZ_WINDOWS
#		define FZ_RESULT_RAW(win, nix) win
#		define FZ_RESULT_RAW_CASE_WIN(win) case win:
#		define FZ_RESULT_RAW_CASE_NIX(nix)
#	else
#		define FZ_RESULT_RAW(win, nix) nix
#		define FZ_RESULT_RAW_CASE_WIN(win)
#		define FZ_RESULT_RAW_CASE_NIX(nix) case nix:
#	endif
#endif

#define FZ_RESULT_RAW_ALREADY_EXISTS   FZ_RESULT_RAW(ERROR_ALREADY_EXISTS, EEXIST)
#define FZ_RESULT_RAW_TIMEOUT          FZ_RESULT_RAW(ERROR_TIMEOUT, ETIMEDOUT)
#define FZ_RESULT_RAW_NOT_IMPLEMENTED  FZ_RESULT_RAW(ERROR_CALL_NOT_IMPLEMENTED, ENOSYS)

#endif // FZ_STRRESULT_HPP
