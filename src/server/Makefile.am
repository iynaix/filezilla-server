bin_PROGRAMS = filezilla-server

SOURCES_H = \
	administration.hpp \
	administrator.hpp \
	administrator/debug.hpp \
	administrator/ftp_test_creator.hpp \
	administrator/notifier.hpp \
	administrator/update_checker.hpp \
	legacy_options.hpp \
	server_config_paths.hpp \
	server_settings.hpp

filezilla_server_SOURCES = \
    $(SOURCES_H) \
	administrator.cpp \
	administrator/acme.cpp \
	administrator/admin_options.cpp \
	administrator/certificates.cpp \
	administrator/ftp_options.cpp \
	administrator/ftp_test_creator.cpp \
	administrator/groups_and_users.cpp \
	administrator/ip_filters.cpp \
	administrator/logger_options.cpp \
	administrator/notifier.cpp \
	administrator/pkcs11_options.cpp \
	administrator/protocol_options.cpp \
	administrator/update_checker.cpp \
	administrator/webui_options.cpp \
	main.cpp \
	server_settings.cpp

filezilla_server_CXXFLAGS = -pthread -fno-exceptions

filezilla_server_CPPFLAGS = $(LIBFILEZILLA_CFLAGS) -DFZ_REL_TOP_SRCDIR='fzT("$(top_srcdir)")'

if FZ_WINDOWS
    filezilla_server_LDFLAGS = -municode
    EXTRA_LDADD = $(top_builddir)/res/filezilla-server.o
    EXTRA_LIBS =
endif

filezilla_server_LDADD = $(EXTRA_LDADD) ../filezilla/libfilezilla-common.a $(EXTRA_LIBS) $(LIBFILEZILLA_LIBS) $(PUGIXML_LIBS)

if ENABLE_FZ_WEBUI
filezilla_server_CPPFLAGS += $(LIBSQLITE3_CFLAGS)
filezilla_server_LDADD += $(LIBSQLITE3_LIBS)
endif
