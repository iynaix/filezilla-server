INSTALLER_FILES = \
	install.nsi.in \
	adminsettings.nsdinc \
	adminsettings.nsddef \
	adminsettings.extra.nsh \
	serversettings.nsdinc \
	serversettings.nsddef \
	serversettings.extra.nsh \
	servicesettings.nsdinc \
	servicesettings.nsddef \
	servicesettings.extra.nsh \
	lang_en.nsh \
	lang_it.nsh \
	macros.nsh \
	process_running.nsh \
	ShellExecAsUser.dll \
	ShellExecAsUser.dll.txt

GENERATORS_FILES = \
	dllcopy.sh \
	deps_intersection.sh \
	deps_difference.sh \
	deps_union.sh \
	generate_nsh_from_deps.sh
	
dist_noinst_DATA = \
	$(INSTALLER_FILES) \
	$(GENERATORS_FILES)

PKG_NAME = $(PACKAGE_STRING_WITHOUT_SPACES)_win${FZ_BITS}
INSTALLER_NAME = $(PKG_NAME)-setup.exe
ZIP_NAME = $(PKG_NAME).zip
SEVENZ_NAME = $(PKG_NAME).7z

PKG_DESTDIR = $(abs_builddir)/files
PKG_BINDIR = $(PKG_DESTDIR)
PKG_LIBDIR = $(PKG_DESTDIR)
PKG_DOCDIR = $(PKG_DESTDIR)
PKG_CLEAN = *_install.nsh *_uninstall.nsh

include ../files.mkinc

intersection = "$(abs_srcdir)/deps_intersection.sh"
union        = "$(abs_srcdir)/deps_union.sh"
difference   = "$(abs_srcdir)/deps_difference.sh"
generate     = "$(abs_srcdir)/generate_nsh_from_deps.sh"

pkg-exe-prepare: files/.prepared
pkg-exe-strip: files/.stripped
pkg-exe: $(INSTALLER_NAME)
pkg-zip: $(ZIP_NAME)
pkg-7z: $(SEVENZ_NAME)

###########################################################


files/.prepared: $(ALL_EXES) $(GENERATORS_FILES)
	"$(srcdir)/dllcopy.sh" "$(PKG_DESTDIR)" "true" "$(OBJDUMP)" "$(CXX)" "$$PATH" "$(WX_LIBS) $(LIBFILEZILLA_LIBS)" $(ALL_EXES)
	$(generate) common <($(intersection) <($(union) $(SERVER_EXES)) <($(union) $(GUI_EXES)))
	$(generate) server <($(difference)   <($(union) $(SERVER_EXES)) <($(union) $(GUI_EXES)))
	$(generate) gui    <($(difference)   <($(union) $(GUI_EXES))    <($(union) $(SERVER_EXES)))
	@$(GREEN); echo -n "Installation files have been prepared. You can find them here: "; $(YELLOW); echo $(PKG_DESTDIR); $(NORMAL); echo
	@touch $@

if HAS_MAKENSIS

if ENABLE_FZ_WEBUI
    DENABLE_FZ_WEBUI=-DENABLE_FZ_WEBUI
else
    DENABLE_FZ_WEBUI=
endif

$(INSTALLER_NAME): files/.stripped $(INSTALLER_FILES) install.nsi
	'$(MAKENSIS)' -DINSTALLER_NAME=$(INSTALLER_NAME) $(DENABLE_FZ_WEBUI) install.nsi

else

$(INSTALLER_NAME):
	@$(RED); echo -n makensis is not available, installer cannot be generated.; $(NORMAL); echo && false

EXTRA_PHONY = $(INSTALLER_NAME)
endif

files/.list: $(PKG_DOCDIR)/NEWS $(PKG_DOCDIR)/$(PKG_EULA) files/.prepared
	@{ echo 'File "files\$(PKG_EULA)"'; echo 'File "files\NEWS"'; cat common_install.nsh server_install.nsh gui_install.nsh; } | sed -n 's,^File "files\\\([^"]*\)",files/\1,p' > $@

$(ZIP_NAME): files/.stripped files/.list Makefile
	@rm -f $@
	@cat files/.list | zip "$@" -@
	@cat files/.list | sed -n 's%^files/\(.*\)%@ files/\1\n@=$(PKG_NAME)/\1\n@%p' | zipnote -w "$@"
	@$(GREEN); echo -n "ZIP package has been produced: "; $(YELLOW); echo -n "$(PWD)/$@"; $(NORMAL); echo
	
$(SEVENZ_NAME): files/.stripped files/.list Makefile
	@rm -f $@
	@7z a $@ @files/.list
	@7z rn $@ files "$(PKG_NAME)"

.PHONY: pkg-exe pkg-exe-prepare pkg-exe-strip pkg-zip pkg-7z $(EXTRA_PHONY) .maybe_rebuild

