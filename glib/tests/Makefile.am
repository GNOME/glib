include $(top_srcdir)/Makefile.decl

INCLUDES = -g -I$(top_srcdir) -I$(top_srcdir)/glib $(GLIB_DEBUG_FLAGS)

noinst_PROGRAMS = $(TEST_PROGS)
progs_ldadd     = $(top_builddir)/glib/libglib-2.0.la


TEST_PROGS       += testing
testing_SOURCES	  = testing.c
testing_LDADD	  = $(progs_ldadd)

TEST_PROGS       	 += option-context
option_context_SOURCES	  = option-context.c
option_context_LDADD	  = $(progs_ldadd)

# some testing of gtester funcitonality
XMLLINT=xmllint
gtester-xmllint-check: # check testreport xml with xmllint if present
	${GTESTER} -k --quiet -o tmpsample.xml --test-arg=--gtester-selftest ${GTESTER}
	${XMLLINT} --version 2>/dev/null; test "$$?" != 0 || ${XMLLINT} --noout tmpsample.xml
check-am: gtester-xmllint-check

CLEANFILES = \
	tmpsample.xml
