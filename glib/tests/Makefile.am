INCLUDES = -g -I$(top_srcdir) -I$(top_srcdir)/glib $(GLIB_DEBUG_FLAGS)

GTESTER		 = $(top_builddir)/glib/gtester

TEST_PROGS	=
noinst_PROGRAMS = $(TEST_PROGS)
progs_ldadd     = $(top_builddir)/glib/libglib-2.0.la


TEST_PROGS       += testing
testing_SOURCES	  = testing.c
testing_LDADD	  = $(progs_ldadd)

# exemplary unit test rules
test:
	${GTESTER} --verbose ${TEST_PROGS}
test-report:
	${GTESTER} --verbose -k -o testreport.xml ${TEST_PROGS}
.PHONY: test test-report
check-local: test


# some testing of gtester funcitonality
XMLLINT=xmllint
gtester-xmllint-check: # check testreport xml with xmllint if present
	${GTESTER} -k --quiet -o tmpsample.xml --test-arg=--gtester-selftest ${GTESTER}
	${XMLLINT} --version 2>/dev/null; test "$$?" != 0 || ${XMLLINT} --noout tmpsample.xml
check-am: gtester-xmllint-check
