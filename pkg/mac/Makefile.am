PKG_NAME = $(PACKAGE_NAME_WITHOUT_SPACES)_$(PACKAGE_VERSION)
APP_NAME = $(PACKAGE_NAME_WITH_ESCAPED_SPACES).app

PKG_DESTDIR = $(abs_builddir)/files/$(APP_NAME)
PKG_PREFIX = $(PKG_DESTDIR)/Contents
PKG_BINDIR = $(PKG_PREFIX)/MacOS
PKG_LIBDIR = $(PKG_PREFIX)/Frameworks
PKG_DOCDIR = $(PKG_PREFIX)/SharedSupport
PKG_RESDIR = $(PKG_PREFIX)/Resources

PKG_STRIPOPTS = -S -x
PKG_CLEAN = files $(PKG_NAME).app.tar.bz2 $(PKG_NAME).pkg main.pkg service.pkg

include ../files.mkinc

pkg-app-prepare: files/.prepared
pkg-app-strip: files/.stripped
pkg-app: ${abs_builddir}/$(PKG_NAME).app.tar.bz2
pkg-app-pkg: ${abs_builddir}/$(PKG_NAME).pkg

dist_noinst_DATA = \
	scripts/main/preinstall \
	scripts/main/postinstall \
	scripts/service/postinstall.in

###########################################################

files/.prepared: $(ALL_EXES) Makefile $(PKG_DOCDIR)/$(PKG_EULA) $(PKG_DOCDIR)/NEWS $(PKG_RESDIR)/filezilla-server-gui.icns $(PKG_RESDIR)/org.filezilla-project.filezilla-server.service.plist $(PKG_PREFIX)/Info.plist
	${srcdir}/dylibcopy.sh $(PKG_DESTDIR) ""
	@$(GREEN); echo -n "Installation files have been prepared. You can find them here: "; $(YELLOW); echo $(PKG_DESTDIR); $(NORMAL); echo
	@touch $@

${abs_builddir}/$(PKG_NAME).app.tar.bz2: files/.stripped
	cd files && tar cvjSf $@ $(APP_NAME)
	@$(GREEN); echo -n "App bundle has been created. You can find it here: "; $(YELLOW); echo $@; $(NORMAL); echo

$(PKG_PREFIX)/Info.plist: Info.plist
	@$(MKDIR_P) "$$(dirname "$@")"
	@cp -v "$<" "$@"

$(PKG_RESDIR)/filezilla-server-gui.icns: ${top_srcdir}/res/filezilla-server-gui.icns
	@$(MKDIR_P) "$$(dirname "$@")"
	@cp -v "$<" "$@"

$(PKG_RESDIR)/org.filezilla-project.filezilla-server.service.plist: ${top_builddir}/res/org.filezilla-project.filezilla-server.service.plist
	@$(MKDIR_P) "$$(dirname "$@")"
	@cp -v "$<" "$@"

.PHONY: pkg-app pkg-app-prepare pkg-app-strip pkg-app-pkg

main.pkg: files/.stripped scripts/main/preinstall scripts/main/postinstall
	pkgbuild --compression latest --min-os-version 10.13.2 --scripts "${srcdir}/scripts/main" --root files --component-plist "component.plist" "$@"

service.pkg: scripts/service/postinstall
	pkgbuild --compression latest --min-os-version 10.13.2 --nopayload --scripts "${builddir}/scripts/service" --identifier org.filezilla-project.filezilla-server.service $@

${abs_builddir}/${PKG_NAME}.pkg: main.pkg service.pkg server.dist
	productbuild --distribution server.dist --package-path . $@
	@$(GREEN); echo -n "App package has been created. You can find it here: "; $(YELLOW); echo $@; $(NORMAL); echo

