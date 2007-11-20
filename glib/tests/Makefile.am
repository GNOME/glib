INCLUDES = -g -I$(top_srcdir) -I$(top_srcdir)/glib $(GLIB_DEBUG_FLAGS)

TESTS	=
noinst_PROGRAMS = $(TESTS)
progs_ldadd     = $(top_builddir)/glib/libglib-2.0.la


TESTS            += testing
testing_SOURCES	  = testing.c
testing_LDADD	  = $(progs_ldadd)


test:
	@set -e \
	&& for tst in ${TESTS} ; do \
	  echo -n "TEST: ./$$tst...  " ; \
	  ./$$tst ; \
	  echo "OK" ; \
	done
.PHONY: test
