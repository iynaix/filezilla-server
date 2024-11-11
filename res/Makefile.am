SUBDIRS = share

dist_noinst_DATA = \
    filezilla-server-gui.ico \
    filezilla-server-gui.icns \
    filezilla-server-gui.manifest.xml.in \
    filezilla-server-gui.rc \
    filezilla-server-gui.version.rc.in \
    filezilla-server.ico \
    filezilla-server.rc \
    filezilla-server.version.rc.in \
	org.filezilla-project.filezilla-server.service.plist \
    uninstall.ico

if FZ_WINDOWS

nodist_noinst_DATA = \
    filezilla-server.o \
    filezilla-server-gui.o

CLEANFILES = \
    $(nodist_noinst_DATA)

filezilla-server.o: $(srcdir)/filezilla-server.rc filezilla-server.version.rc
	@if [ -f $@~ ]; then rm $@~; fi
	@WINDRES@ --include-dir=. --include-dir=$(srcdir) $(WINDRESFLAGS) $(srcdir)/filezilla-server.rc $@~ || rm -f $@~
	@if [ -f $@ ]; then rm $@; fi
	mv $@~ $@

filezilla-server-gui.o: $(srcdir)/filezilla-server-gui.rc filezilla-server-gui.version.rc filezilla-server-gui.manifest.xml
	@if [ -f $@~ ]; then rm $@~; fi
	@WINDRES@ --include-dir=. --include-dir=$(srcdir) $(WINDRESFLAGS) $(srcdir)/filezilla-server-gui.rc $@~ || rm -f $@~
	@if [ -f $@ ]; then rm $@; fi
	mv $@~ $@

endif


