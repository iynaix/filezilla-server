bin_PROGRAMS = filezilla-server-impersonator
    
SOURCES_H =

filezilla_server_impersonator_SOURCES = \
    $(SOURCES_H) \
    main.cpp

filezilla_server_impersonator_CXXFLAGS = -pthread -fno-exceptions $(LIBFILEZILLA_CFLAGS) 

filezilla_server_impersonator_LDADD    = ../../filezilla/libfilezilla-common.a $(LIBFILEZILLA_LIBS) 

