noinst_PROGRAMS = \
    administration_client/administration_client \
    echo/echo \
    filetransfer/filetransfer \
    httpget/httpget

if ENABLE_FZ_WEBUI
noinst_PROGRAMS += httpserve/httpserve

httpserve_httpserve_SOURCES = \
    httpserve/httpserve.cpp
endif

administration_client_administration_client_SOURCES = \
    administration_client/administration_client.cpp

echo_echo_SOURCES = \
    echo/echo.cpp

filetransfer_filetransfer_SOURCES = \
    filetransfer/filetransfer.cpp

httpget_httpget_SOURCES = \
    httpget/httpget.cpp

AM_CXXFLAGS = $(LIBFILEZILLA_CFLAGS) $(WX_CXXFLAGS) -fno-exceptions
LIBS     = ../src/filezilla/libfilezilla-common.a $(LIBFILEZILLA_LIBS) $(PUGIXML_LIBS) $(EXTRA_LIBS)

if ENABLE_FZ_WEBUI
AM_CXXFLAGS += $(LIBSQLITE3_CFLAGS)
LIBS += $(LIBSQLITE3_LIBS)
endif

