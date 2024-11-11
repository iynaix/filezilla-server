# Rules for the test code (use `make check` to execute)

TESTS = test
check_PROGRAMS = $(TESTS)

test_SOURCES = \
	basic_path.cpp \
	intrusive_list.cpp \
	parser.cpp \
	test.cpp \
	tvfs.cpp
	
test_CXXFLAGS = $(LIBFILEZILLA_CFLAGS)		
test_CPPFLAGS = $(AM_CPPFLAGS)
test_CPPFLAGS += $(CPPUNIT_CFLAGS)

test_LDFLAGS = $(AM_LDFLAGS)
test_LDFLAGS += -no-install

test_LDADD = ../src/filezilla/libfilezilla-common.a
test_LDADD += $(CPPUNIT_LIBS)
test_LDADD += $(LIBFILEZILLA_LIBS)
test_LDADD += $(libdeps)

test_DEPENDENCIES = ../src/filezilla/libfilezilla-common.a

noinst_HEADERS = test_utils.hpp

