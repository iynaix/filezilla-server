#ifndef LOCALE_HPP
#define LOCALE_HPP

#include <clocale>
#include <wx/intl.h>

#include "glue.hpp"

#ifdef _S
#   error macro _S has already been defined.
#endif

#ifdef _F
#   error macro _S has already been defined.
#endif

#ifdef _P
#   error macro _P has already been defined.
#endif

#ifndef FZ_LOCALE_STATIC_TRANSLATIONS
#	define FZ_LOCALE_STATIC_TRANSLATIONS 1
#endif

#ifndef FZ_LOCALE_FORMAT_WITH_SPRINTF
#	define FZ_LOCALE_FORMAT_WITH_SPRINTF 1
#endif

#if FZ_LOCALE_STATIC_TRANSLATIONS
#	define _S(x) ([]() -> const wxString & { static const wxString t = wxGetTranslation(wxS(x)); return t; }())
#else
#	define _S(x) wxGetTranslation(wxS(x))
#endif

#define _P(s, p, n) wxPLURAL(wxS(s), wxS(p), n)

#if FZ_LOCALE_FORMAT_WITH_SPRINTF
#	define _F(x, ...) fz::to_wxString(fz::sprintf(_S(x).ToStdWstring(), __VA_ARGS__))
#else
#	define _F(x, ...) wxString::Format(_S(x), __VA_ARGS__)
#endif

#endif // LOCALE_HPP
