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

TEST_PROGS     	 += keyfile
keyfile_SOURCES	  = keyfile.c
keyfile_LDADD	  = $(progs_ldadd)

TEST_PROGS        += fileutils
fileutils_SOURCES  = fileutils.c
fileutils_LDADD	   = $(progs_ldadd)

TEST_PROGS        += printf
printf_SOURCES       = printf.c
printf_LDADD	   = $(progs_ldadd) -lm

TEST_PROGS        += rand
rand_SOURCES       = rand.c
rand_LDADD	   = $(progs_ldadd) -lm

TEST_PROGS        += strfuncs
strfuncs_SOURCES   = strfuncs.c
strfuncs_LDADD	   = $(progs_ldadd) -lm

TEST_PROGS        += string
string_SOURCES     = string.c
string_LDADD	   = $(progs_ldadd) -lm

TEST_PROGS               += markup-subparser
markup_subparser_LDADD    = $(progs_ldadd)

TEST_PROGS         += array-test
array_test_LDADD    = $(progs_ldadd)

TEST_PROGS         += hostutils
hostutils_LDADD     = $(progs_ldadd)

TEST_PROGS         += gvariant
gvariant_LDADD      = $(progs_ldadd)

TEST_PROGS         += mem-overflow
mem_overflow_LDADD  = $(progs_ldadd)

if OS_UNIX

# some testing of gtester funcitonality
XMLLINT=xmllint
gtester-xmllint-check: # check testreport xml with xmllint if present
	${GTESTER} -k --quiet -o tmpsample.xml --test-arg=--gtester-selftest ${GTESTER}
	${XMLLINT} --version 2>/dev/null; test "$$?" != 0 || ${XMLLINT} --noout tmpsample.xml
check-am: gtester-xmllint-check

endif

CLEANFILES = \
	tmpsample.xml

EXTRA_DIST += \
	4096-random-bytes
