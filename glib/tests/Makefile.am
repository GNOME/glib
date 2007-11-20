INCLUDES = -g -I$(top_srcdir) -I$(top_srcdir)/glib $(GLIB_DEBUG_FLAGS)

GTESTER		 = $(top_builddir)/glib/gtester

TEST_PROGS	=
noinst_PROGRAMS = $(TEST_PROGS)
progs_ldadd     = $(top_builddir)/glib/libglib-2.0.la


TEST_PROGS       += testing
testing_SOURCES	  = testing.c
testing_LDADD	  = $(progs_ldadd)


test:
	${GTESTER} --verbose ${TEST_PROGS}
.PHONY: test
check-local: test
