bin_PROGRAMS = filezilla-server-crypt
    
SOURCES_H =
    
filezilla_server_crypt_SOURCES = \
    $(SOURCES_H) \
    main.cpp

filezilla_server_crypt_CXXFLAGS = -pthread -fno-exceptions $(LIBFILEZILLA_CFLAGS) 

if FZ_WINDOWS
    filezilla_server_crypt_LDFLAGS = -municode
endif

filezilla_server_crypt_LDADD    = ../../filezilla/libfilezilla-common.a $(EXTRA_LIBS) $(LIBFILEZILLA_LIBS) 

