define(COPYRIGHT_NOTICE, [Copyright (C) 2019-2024 Tim Kosse and Business Follows srl])

AC_INIT(
  [FileZilla Server],
  [1.9.4],
  [Tim Kosse <tim.kosse@filezilla-project.org>],,[https://filezilla-project.org/])
AC_COPYRIGHT(COPYRIGHT_NOTICE)

PACKAGE_NAME_WITHOUT_SPACES=${PACKAGE_NAME// /_}
PACKAGE_NAME_WITH_ESCAPED_SPACES=${PACKAGE_NAME// /\\ }
PACKAGE_STRING_WITHOUT_SPACES=${PACKAGE_STRING// /_}

AC_SUBST(PACKAGE_NAME_WITHOUT_SPACES)
AC_SUBST(PACKAGE_NAME_WITH_ESCAPED_SPACES)
AC_SUBST(PACKAGE_STRING_WITHOUT_SPACES)
AC_DEFINE_UNQUOTED([PACKAGE_COPYRIGHT], ["COPYRIGHT_NOTICE"], [The copyright notice])

AH_TOP(/* COPYRIGHT_NOTICE */
[
#ifndef FZ_CONFIG_HPP
#define FZ_CONFIG_HPP
])
AH_BOTTOM([
#endif // !FZ_CONFIG_HPP
])
AC_CONFIG_HEADERS([src/config.hpp src/config_modules.hpp:src/config_modules.hpp.in])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR(config)
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects dist-xz no-dist-gzip check-news tar-ustar])
AC_CANONICAL_HOST
AC_PROG_CXX
OBJCXXFLAGS="${OBJCXXFLAGS:-$CXXFLAGS}"
AC_PROG_OBJCXX
AM_PROG_AR
LT_INIT

AC_ARG_ENABLE(dist-only, AS_HELP_STRING([--enable-dist-only],[Just configure to allow 'make dist', does not check any dependencies and does not allow building the server.]), \
	[distonly="$enableval"], [distonly="no"])

if test "$distonly" != "yes"; then

AC_ARG_ENABLE(build-type, AS_HELP_STRING([--enable-build-type],[Enable a specific build type. Currently must be one of: official, nightly. Default is: official]),
    [build_type="$enableval"], [build_type="official"])

AC_MSG_CHECKING([which build type to use])
case "$build_type" in
	official);;
	nightly);;
	*) AC_MSG_ERROR([build type "$build_type" not supported]);;
esac

AC_MSG_RESULT([$build_type])

AC_DEFINE_UNQUOTED([FZ_BUILD_TYPE], [$build_type],
	[Define to one of the supported build types. See build_type.hpp.])

AC_SYS_LARGEFILE

AC_CHECK_TOOL(OBJDUMP, objdump, false)
AC_CHECK_TOOL(STRIP, strip, false)

AX_CXX_COMPILE_STDCXX(17,[noext],[mandatory])
CHECK_LIBCXX

PKG_CHECK_MODULES([LIBFILEZILLA], [libfilezilla >= 0.45.0],, [
    AC_MSG_ERROR([libfilezilla 0.45.0 or greater was not found. You can get it from https://lib.filezilla-project.org/])
])

###

AC_SUBST(EXEC_OBJDIR)

AC_DEFINE_UNQUOTED([FZ_BUILD_HOST], ["$host"], [The canonicalized host type])

AC_ARG_ENABLE([update-checker],
    [AS_HELP_STRING([--enable-update-checker], [enable the update checker @<:@default=yes@:>@])],
    [],
    [enable_update_checker=yes]
)

AS_IF([test "x$enable_update_checker" == xyes], [
	AC_DEFINE([ENABLE_FZ_UPDATE_CHECKER], [1], [Define if you want to enable the update checker])
])

AC_ARG_ENABLE([webui],
    [AS_HELP_STRING([--enable-webui], [enable the webui @<:@default=no@:>@])],
    [],
    [enable_webui=no]
)

AS_IF([test "x$enable_webui" == xyes], [
  AC_DEFINE([ENABLE_FZ_WEBUI], [1], [Define if you want to enable the webui])
  DEB_EXECSTART_WEBUI_ROOT="--webui-app-root /opt/filezilla-server/share/filezilla-server/webui"
  AC_SUBST(DEB_EXECSTART_WEBUI_ROOT)

  # SQLite3
  # -------

  PKG_CHECK_MODULES(LIBSQLITE3, sqlite3 >= 3.7,, [
    AC_CHECK_HEADER(sqlite3.h,,
    [
      AC_MSG_ERROR([sqlite3.h not found which is part of SQLite3.])
    ])

    AC_CHECK_LIB(sqlite3, sqlite3_version, LIBSQLITE3_LIBS="-lsqlite3", [
      AC_MSG_ERROR([SQLite3 not found.])
    ])
  ])

  AC_SUBST(LIBSQLITE3_LIBS)
  AC_SUBST(LIBSQLITE3_CFLAGS)
])

# Detect HOST OS
# --------------

FZ_WINDOWS=no
FZ_MAC=no
FZ_UNIX=no

# Detect the target system
case "${host_os}" in
    cygwin*|mingw*)
        FZ_WINDOWS=yes
        ;;
    darwin*)
        FZ_MAC=yes
        ;;
    *)
        # Assume compilation on UNIX, if nothing else matches
        FZ_UNIX=yes
        ;;
esac

if test "$FZ_MAC" == "yes"; then
	if test "x$OBJCXX" == "x"; then
		AC_MSG_ERROR([The Obj-C++ was not found. Cannot continue.])
	fi

	OBJCXX="$OBJCXX -std=c++17"
	LIBS="$LIBS -framework Cocoa"
fi

# Detect CPU word size
# --------------------

AC_MSG_CHECKING([for CPU word size])
AC_COMPUTE_INT(FZ_BITS, [sizeof(size_t)*CHAR_BIT], [
	#include <stddef.h>
	#include <limits.h>
], [
	AC_MSG_ERROR([cannot get it])
])
AC_MSG_RESULT([${FZ_BITS} bits])
AC_SUBST(FZ_BITS)

# Detect wxWidgets
# ----------------

AM_OPTIONS_WXCONFIG

MIN_WX_VERSION="3.0.4" # Do not downgrade this. wx 3.0.3 and older have too many known and fixed bugs affecting FileZilla Server.
AM_PATH_WXCONFIG([$MIN_WX_VERSION], [wxWin=1], , [adv,core,base], [--unicode=yes --universal=no])
if test "$wxWin" != 1; then
    AC_MSG_ERROR([
        wxWidgets must be installed on your system
        but either the wx-config script couldn't be found or
        no compatible wxWidgets configuration has been installed.

        Compatible wxWidgets configurations are the unicode builds
        of wxGTK, wxMac and wxMSW.

        Please check that wx-config is in path, the directory
        where wxWidgets libraries are installed (returned by
        'wx-config --libs' command) is in LD_LIBRARY_PATH or
        equivalent variable and wxWidgets version is $MIN_WX_VERSION or above.
    ])
fi

#
# We now support also 3.1, but let's keep these check here, commented out, for reference sake.
#
#if test "${WX_VERSION_MAJOR}.${WX_VERSION_MINOR}" = "3.1"; then
#    AC_MSG_ERROR([You must use wxWidgets 3.0.x, development versions of wxWidgets are not supported.])
#elif test "${WX_VERSION_MAJOR}.${WX_VERSION_MINOR}" != "3.0"; then
#    AC_MSG_ERROR([You must use wxWidgets 3.0.x, wxWidgets 3.2 or higher is not yet supported.])
#fi

# --universal=no doesn't work correctly, it still accepts universal builds. Reject it manually here.
if echo "$WX_CPPFLAGS" | grep __WXUNIVERSAL__ > /dev/null; then
    AC_MSG_ERROR([wxUniversal is not acceptable as flavour of wxWidgets. Please recompile wxWidgets with --disable-universal])
fi
if echo "$WX_CPPFLAGS" | grep __WXMOTIF__ > /dev/null; then
    AC_MSG_ERROR([wxMotif is not acceptable as flavour of wxWidgets. Please recompile wxWidgets with the GTK flavour])
fi

AC_LANG_PUSH(C++)
CPPFLAGS_OLD="$CPPFLAGS"
CXXFLAGS_OLD="$CXXFLAGS"
CPPFLAGS="$CPPFLAGS $WX_CPPFLAGS"
CXXFLAGS="$CXXFLAGS $WX_CXXFLAGS_ONLY"

AC_MSG_CHECKING([positional printf argument support in wxWidgets])
AC_COMPILE_IFELSE(
[AC_LANG_PROGRAM([[
      #include <wx/wx.h>
      #if !wxUSE_PRINTF_POS_PARAMS
        #error wx compiled without wxUSE_PRINTF_POS_PARAMS
      #endif
    ]])],
  AC_MSG_RESULT([yes]),
  [
    AC_MSG_RESULT([no])
    AC_MSG_FAILURE([Positional printf argument support in wxWidgets is required by FileZilla Server. Please rebuild wxWidgets and pass --enable-printfposparam to its configure script.])
  ]
)

AC_MSG_CHECKING([GUI support in wxWidgets])
AC_COMPILE_IFELSE(
[AC_LANG_PROGRAM([[
      #include <wx/wx.h>
      #if !wxUSE_GUI
        #error wx compiled without wxUSE_GUI
      #endif
    ]])],
  AC_MSG_RESULT([yes]),
  [
    AC_MSG_RESULT([no])
    AC_MSG_FAILURE([GUI support in wxWidgets is required by FileZilla Server. Please rebuild wxWidgets and pass --enable-gui to its configure script.])
  ]
)
CPPFLAGS="$CPPFLAGS_OLD"
CXXFLAGS="$CXXFLAGS_OLD"

# Enable DEP and ASLR linker flags
# --------------------------------

AX_CHECK_LINK_FLAG([-Wl,--dynamicbase], [AX_APPEND_FLAG([-Wl,--dynamicbase], LDFLAGS)])
AX_CHECK_LINK_FLAG([-Wl,--high-entropy-va], [AX_APPEND_FLAG([-Wl,--high-entropy-va], LDFLAGS)])
AX_CHECK_LINK_FLAG([-Wl,--nxcompat], [AX_APPEND_FLAG([-Wl,--nxcompat], LDFLAGS)])
if test "${FZ_WINDOWS}" = "yes"; then
  if echo $host_cpu | grep "x86_64" > /dev/null 2>&1; then
    # Base address >4GB to some compatibility mode the kernel uses for old programs
    AX_CHECK_LINK_FLAG([-Wl,--image-base -Wl,0x120000000], [AX_APPEND_FLAG([-Wl,--image-base -Wl,0x120000000], LDFLAGS)])
  fi
fi

if test "${FZ_WINDOWS}" = "yes"; then
    AX_CHECK_LINK_FLAG([-municode], [], [AC_MSG_ERROR([Support for -municode is necessary on Windows])], [],
        [AC_LANG_SOURCE([
            #include <cwchar>
            int wmain(int argc, wchar_t *argv[]){ return 0; }
        ])]
    )

  # Set Windows-specific linker flags
  # ---------------------------------

  # We currently require 8.1 as minimum.
  AX_CHECK_LINK_FLAG([-Wl,--major-subsystem-version -Wl,6], [AX_APPEND_FLAG([-Wl,--major-subsystem-version -Wl,6], LDFLAGS)])
  AX_CHECK_LINK_FLAG([-Wl,--minor-subsystem-version -Wl,1], [AX_APPEND_FLAG([-Wl,--minor-subsystem-version -Wl,1], LDFLAGS)])
  AX_CHECK_LINK_FLAG([-Wl,--major-os-version -Wl,6], [AX_APPEND_FLAG([-Wl,--major-os-version -Wl,6], LDFLAGS)])
  AX_CHECK_LINK_FLAG([-Wl,--minor-os-version -Wl,3], [AX_APPEND_FLAG([-Wl,--minor-os-version -Wl,3], LDFLAGS)])

  # Set the Large Address Aware flag on 32bit Windows
  if ! echo $host_cpu | grep "x86_64" > /dev/null 2>&1; then
    AX_CHECK_LINK_FLAG([-Wl,--large-address-aware], [AX_APPEND_FLAG([-Wl,--large-address-aware], LDFLAGS)])
  fi
fi

AC_LANG_POP

# CppUnit tests
# -------------

PKG_CHECK_MODULES(CPPUNIT, [cppunit >= 1.13.0], [have_cppunit=yes],[:])

###############


AC_ARG_WITH([makensis],
    [AS_HELP_STRING([--with-makensis=[path/to/]exe], [path to the makensis executable])],
    [],
    [with_makensis=makensis]
)

case "$with_makensis" in
    /*)
        MAKENSIS="$with_makensis"
        ;;
    no)
        MAKENSIS=""
        ;;
    *)
        AC_PATH_PROG([MAKENSIS], [$with_makensis], [])
        ;;
esac

if test "${FZ_WINDOWS}" = "yes"; then
    AC_MSG_CHECKING([whether makensis is available])
    if test -n "$MAKENSIS"; then
        AC_MSG_RESULT([yes])
    else
        AC_MSG_RESULT([no])
        AC_MSG_WARN([makensis is not available or has been explictly disabled. You won't be able to generate a windows installer])
    fi
fi

if test "${FZ_UNIX}" = "yes"; then
    AC_PATH_PROG([DPKG_ARCHITECTURE], [dpkg-architecture])
    AC_PATH_PROG([DPKG_DEB], [dpkg-deb])

    AC_MSG_CHECKING([whether we can build deb packages])

    if test "x$DPKG_ARCHITECTURE" = "x" || test "x$DPKG_DEB" = "x"; then
    	AC_MSG_RESULT([no])
        AC_MSG_WARN([Couldn't find the tools to build a debian package. This functionality will be disabled])
    else
    	DEB_TARGET_ARCH=$("$DPKG_ARCHITECTURE" --query DEB_TARGET_ARCH)

    	if test "x$DEB_TARGET_ARCH" = "x"; then
    		AC_MSG_RESULT([no])
    		AC_MSG_FAILURE([dpkg-architecture was found, but the query for DEB_TARGET_ARCH didn't return a proper result])
    	else
    		AC_MSG_RESULT([yes])
    	fi
    fi
fi

AC_ARG_ENABLE(mac-sandbox, AS_HELP_STRING([--enable-mac-sandbox],[Enables use of OS X's App Sandbox.]),
    [use_mac_sandbox="$enableval"], [use_mac_sandbox="no"])
if test "$use_mac_sandbox" = "yes"; then
    AC_DEFINE([USE_MAC_SANDBOX], [1],
       [Define to 1 if the App Sandbox on OS X should be used.])
    CFBUNDLEIDSUFFIX=".sandbox"
else
    CFBUNDLEIDSUFFIX=""
fi

AC_SUBST(CFBUNDLEIDSUFFIX)

# Check for windres on MinGW builds
# ---------------------------------

# Small function to filter the flags for windres,
# so that only includes and defines are passed to windres.
parseflags() {
    WINDRESFLAGS=""
    for opt in "$@"
    do
        case "$opt" in
        -I*)
            WINDRESFLAGS="$WINDRESFLAGS --include-dir=${opt#-I}"
            ;;
        -D*)
            WINDRESFLAGS="$WINDRESFLAGS $opt"
            ;;
        esac
    done
}

WINDRESFLAGS=
use_resourcefile=

AC_MSG_CHECKING([whether to use windres])
if test "${FZ_WINDOWS}" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_CHECK_TOOL([WINDRES], [windres])

    if test "x$WINDRES" = "x"; then
        AC_MSG_ERROR([windres could not be found, please make sure this program is within your path.])
    fi

    use_resourcefile=true
    parseflags $WX_CPPFLAGS
else
    AC_MSG_RESULT([no])
fi

AC_SUBST(WINDRES)
AC_SUBST(WINDRESFLAGS)

AC_SUBST(HAS_MAKENSIS)
AC_SUBST(MAKENSIS)
AC_SUBST(WX_CONFIG_WITH_ARGS)

AC_SUBST(DEB_TARGET_ARCH)

FZ_CHECK_PUGIXML()

#if !dist-only
fi

# Set conditionals unconditionally
# Pass the conditionals to automake
AM_CONDITIONAL([ENABLE_FZ_UPDATE_CHECKER], [test "x$enable_update_checker" == xyes])
AM_CONDITIONAL([ENABLE_FZ_WEBUI], [test "x$enable_webui" == xyes])
AM_CONDITIONAL([DIST_ONLY], [test "$distonly" = "yes"])
AM_CONDITIONAL([FZ_UNIX], [test "$FZ_UNIX" = "yes"])
AM_CONDITIONAL([FZ_WINDOWS], [test "$FZ_WINDOWS" = "yes"])
AM_CONDITIONAL([FZ_MAC], [test "$FZ_MAC" = "yes"])

AM_CONDITIONAL(USE_RESOURCEFILE, test "x$use_resourcefile" = "xtrue")
AM_CONDITIONAL(HAVE_LIBPUGIXML, [test "x$with_pugixml" = "xsystem"])

AM_CONDITIONAL(HAVE_CPPUNIT, [test "$have_cppunit" = "yes"])

AM_CONDITIONAL([HAS_MAKENSIS], [test -n "$MAKENSIS"])

# Set version macros
# ------------------

vnumber=${PACKAGE_VERSION%-*}
PACKAGE_VERSION_MAJOR=${vnumber%%.*}
vnumber=${vnumber#$PACKAGE_VERSION_MAJOR}
vnumber=${vnumber#.}
PACKAGE_VERSION_MINOR=${vnumber%%.*}
vnumber=${vnumber#$PACKAGE_VERSION_MINOR}
vnumber=${vnumber#.}
PACKAGE_VERSION_MICRO=${vnumber%%.*}
vnumber=${vnumber#$PACKAGE_VERSION_MICRO}
vnumber=${vnumber#.}
PACKAGE_VERSION_NANO=${vnumber%%.*}

if test -z "$PACKAGE_VERSION_MAJOR"; then
    PACKAGE_VERSION_MAJOR=0
fi
if test -z "$PACKAGE_VERSION_MINOR"; then
    PACKAGE_VERSION_MINOR=0
fi
if test -z "$PACKAGE_VERSION_MICRO"; then
    PACKAGE_VERSION_MICRO=0
fi
if test -z "$PACKAGE_VERSION_NANO"; then
    PACKAGE_VERSION_NANO=0
fi

AC_SUBST(PACKAGE_VERSION_MAJOR)
AC_SUBST(PACKAGE_VERSION_MINOR)
AC_SUBST(PACKAGE_VERSION_MICRO)
AC_SUBST(PACKAGE_VERSION_NANO)

PACKAGE_VERSION_SHORT=${PACKAGE_VERSION_MAJOR}.${PACKAGE_VERSION_MINOR}.${PACKAGE_VERSION_MICRO}
AC_SUBST(PACKAGE_VERSION_SHORT)
AC_DEFINE_UNQUOTED([PACKAGE_VERSION_SHORT], ["$PACKAGE_VERSION_SHORT"], [Shortened version number])

###

AC_CACHE_SAVE

###

AC_CONFIG_FILES([
    Makefile
    src/Makefile
    src/filezilla/Makefile
    src/filezilla/webui/app/Makefile
    src/server/Makefile
    src/gui/Makefile
    src/tools/Makefile
    src/tools/configconverter/Makefile
    src/tools/crypt/Makefile
    src/tools/impersonator/Makefile
    res/Makefile
    res/filezilla-server-gui.manifest.xml
    res/filezilla-server-gui.version.rc
    res/filezilla-server.version.rc
    res/org.filezilla-project.filezilla-server.service.plist
    res/share/Makefile
    res/share/icons/Makefile
    res/share/icons/hicolor/Makefile
    pkg/Makefile
    pkg/windows/Makefile
    pkg/windows/install.nsi
    pkg/unix/Makefile
    pkg/unix/control
    pkg/unix/templates
    pkg/unix/filezilla-server-gui.desktop
    pkg/unix/filezilla-server.service
    pkg/mac/Makefile
    pkg/mac/Info.plist
    pkg/mac/server.dist
    pkg/mac/component.plist
    demos/Makefile
    tests/Makefile
])

AC_CONFIG_FILES([
    pkg/mac/scripts/service/postinstall
],[
    chmod a+x pkg/mac/scripts/service/postinstall
])

AC_CONFIG_COMMANDS(
	[determine-execobjdir],
	[
		AC_MSG_CHECKING([Which directory are executables built in])
		EXEC_OBJDIR=$(./libtool --tag=CXX -n --mode=link fakecxx -o fakeoutprog fakeoutprog.o | grep -o '\.libs/fakeoutprog' >/dev/null && echo .libs || echo .)
		AC_MSG_RESULT(${EXEC_OBJDIR})
		sed -e "s/^EXEC_OBJDIR[ \t]*=.*/EXEC_OBJDIR = ${EXEC_OBJDIR}/" -i.bak pkg/windows/Makefile pkg/unix/Makefile pkg/mac/Makefile && rm pkg/windows/Makefile.bak pkg/unix/Makefile.bak pkg/mac/Makefile.bak 
	]
)
AC_CONFIG_COMMANDS(
	[determine-build-datetime],
	[
        mkdir -p $ac_pwd/src/filezilla/
        echo "#define FZ_BUILD_DATETIME" $(date -u +'"%Y%m%d%H%M%S"') > $ac_pwd/src/filezilla/build_datetime.hpp
	]
)

AC_OUTPUT

