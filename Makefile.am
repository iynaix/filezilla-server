ACLOCAL_AMFLAGS = -I m4

if HAVE_CPPUNIT
MAYBE_TESTS = tests
endif

if !DIST_ONLY
SUBDIRS = \
    res \
    src \
    pkg \
    demos \
    $(MAYBE_TESTS)
endif

if FZ_WINDOWS
PKG_DIR = windows
endif

if FZ_UNIX
PKG_DIR = unix
endif

if FZ_MAC
PKG_DIR = mac
endif

PKG_TARGETS = \
	pkg-deb pkg-deb-prepare pkg-deb-strip \
	pkg-exe pkg-exe-prepare pkg-exe-strip \
	pkg-app pkg-app-prepare pkg-app-strip pkg-app-pkg \
	pkg-zip pkg-7z

$(PKG_TARGETS):
	cd pkg/$(PKG_DIR) && $(MAKE) $(AM_MAKEFLAGS) $@

.PHONY: $(PKG_TARGETS)

