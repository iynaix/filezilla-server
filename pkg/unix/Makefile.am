dist_noinst_DATA = \
	control.in \
	templates.in \
	config \
	postinst \
	prerm \
	postrm \
    lintian-overrides \
	filezilla-server.service.in \
	filezilla-server-gui.desktop.in

PACKAGE_NAME = FileZilla_Server_$(PACKAGE_VERSION)_${DEB_TARGET_ARCH}
DEB_NAME = $(PACKAGE_NAME).deb
ZIP_NAME = $(PACKAGE_NAME).zip
SEVENZ_NAME = $(PACKAGE_NAME).7z

PKG_DESTDIR = $(abs_builddir)/files/$(PACKAGE_NAME)
INSTALL_PREFIX = $(PKG_DESTDIR)/opt/${PACKAGE}
PKG_BINDIR = $(INSTALL_PREFIX)/bin
PKG_LIBDIR = $(INSTALL_PREFIX)/lib
PKG_SHAREDIR = $(INSTALL_PREFIX)/share
PKG_DOCDIR = $(INSTALL_PREFIX)
PKG_DEBDIR = $(PKG_DESTDIR)/DEBIAN
PKG_SERVICEDIR = $(PKG_DESTDIR)/usr/lib/systemd/system
PKG_DESKTOPDIR = $(PKG_DESTDIR)/usr/share/applications
PKG_CLEAN = files ${abs_builddir}/$(DEB_NAME)

PKG_LINTIAN_OVERRIDES = $(PKG_DESTDIR)/usr/share/lintian/overrides/filezilla-server
PKG_COPYRIGHT = $(PKG_DESTDIR)/usr/share/doc/filezilla-server/copyright

include ../files.mkinc

pkg-deb-prepare: files/.prepared
pkg-deb-strip: files/.stripped
pkg-deb: ${abs_builddir}/$(DEB_NAME)
pkg-zip: ${abs_builddir}/$(ZIP_NAME)
pkg-7z: ${abs_builddir}/$(SEVENZ_NAME)

###########################################################

files/.prepared: $(ALL_EXES) Makefile
	${srcdir}/socopy.sh  $(PKG_BINDIR) $(PKG_LIBDIR)
	@$(GREEN); echo -n "Installation files have been prepared. You can find them here: "; $(YELLOW); echo $(PKG_DESTDIR); $(NORMAL); echo
	@touch $@

${abs_builddir}/$(DEB_NAME): files/.stripped \
	$(PKG_DEBDIR)/control \
	$(PKG_DOCDIR)/$(PKG_EULA) $(PKG_DOCDIR)/NEWS \
	$(PKG_SERVICEDIR)/filezilla-server.service \
	$(PKG_DESKTOPDIR)/filezilla-server-gui.desktop \
	$(PKG_DEBDIR)/postinst $(PKG_DEBDIR)/prerm $(PKG_DEBDIR)/config $(PKG_DEBDIR)/templates $(PKG_DEBDIR)/postrm \
	$(PKG_SHAREDIR)/icons \
	$(PKG_SHAREDIR)/filezilla-server/webui \
	$(PKG_LINTIAN_OVERRIDES) \
	$(PKG_COPYRIGHT)
	chmod -R og-w $(PKG_DESTDIR)
	dpkg-deb --build --root-owner-group $(PKG_DESTDIR) "$@"
	@$(GREEN); echo -n "Debian package has been created. You can find it here: "; $(YELLOW); echo $@; $(NORMAL); echo

$(PKG_DEBDIR)/control: control files/.stripped
	@$(MKDIR_P) $$(dirname "$@")
	@cat "$<" > "$@"
	@echo "Installed-Size: "$$(($$(du -s "$(PKG_DESTDIR)" | awk '{print $$1}')*512/1024)) >> "$@"
	@echo "Depends: debconf (>= 0.5), adduser, "$$(${srcdir}/depends.sh $(PKG_BINDIR)/* $(PKG_LIBDIR)/*) >> "$@"

$(PKG_DEBDIR)/templates: templates
	@$(MKDIR_P) $$(dirname "$@")
	@cp -v "$<" "$@"

$(PKG_DEBDIR)/config: ${srcdir}/config
	@$(MKDIR_P) $$(dirname "$@")
	@cp -v "$<" "$@"

$(PKG_DEBDIR)/postinst: ${srcdir}/postinst
	@$(MKDIR_P) $$(dirname "$@")
	@cp -v "$<" "$@"

$(PKG_DEBDIR)/prerm: ${srcdir}/prerm
	@$(MKDIR_P) $$(dirname "$@")
	@cp -v "$<" "$@"

$(PKG_DEBDIR)/postrm: ${srcdir}/postrm
	@$(MKDIR_P) $$(dirname "$@")
	@cp -v "$<" "$@"

$(PKG_SERVICEDIR)/filezilla-server.service: ${builddir}/filezilla-server.service
	@$(MKDIR_P) $(PKG_SERVICEDIR)
	@cp -v $< $@

$(PKG_SHAREDIR)/icons:
	@$(MAKE) $(AM_MAKEFLAGS) -C $(top_builddir)/res/share/icons install prefix=$(INSTALL_PREFIX)

$(PKG_SHAREDIR)/filezilla-server/webui:
	@$(MAKE) $(AM_MAKEFLAGS) -C $(top_builddir)/src/filezilla/webui/app install prefix=$(INSTALL_PREFIX)

$(PKG_DESKTOPDIR)/filezilla-server-gui.desktop: ${builddir}/filezilla-server-gui.desktop
	@$(MKDIR_P) $(PKG_DESKTOPDIR)
	@cp -v $< $@

$(PKG_LINTIAN_OVERRIDES): ${srcdir}/lintian-overrides
	@$(MKDIR_P) $$(dirname "$@")
	@cp -v "$<" "$@"

$(PKG_COPYRIGHT): $(PKG_DOCDIR)/$(PKG_EULA)
	@$(MKDIR_P) $$(dirname "$@")
	@cp -v "$<" "$@"

.PHONY: pkg-deb pkg-deb-prepare pkg-deb-strip pkg-zip pkg-7z

