include $(top_srcdir)/Makefile.decl

include $(GLIB_MAKEFILE)

AM_CPPFLAGS =					\
	$(gmodule_INCLUDES)			\
	-DGLIB_DISABLE_DEPRECATION_WARNINGS	\
	$(GLIB_DEBUG_FLAGS)

libglib = $(top_builddir)/glib/libglib-2.0.la
libgthread = $(top_builddir)/gthread/libgthread-2.0.la
libgmodule = $(top_builddir)/gmodule/libgmodule-2.0.la
libgobject = $(top_builddir)/gobject/libgobject-2.0.la


# libtool dependency tracking seems broken.  this is currently
# required to get the tests to dynamic link against the in-tree
# libglib instead of the system one
libgobject += $(libglib)

########################################################################

noinst_LTLIBRARIES = libtestgobject.la

libtestgobject_la_SOURCES =	\
	testcommon.h		\
	testmarshal.h		\
	testmarshal.c		\
	testmodule.c		\
	testmodule.h

GLIB_GENERATED = testmarshal.h testmarshal.c
BUILT_SOURCES = $(GLIB_GENERATED)
testmarshal_sources = $(libtestgobject_la_SOURCES)

########################################################################

LDADD = libtestgobject.la $(libgobject)

test_programs =					\
	deftype					\
	gvalue-test				\
	paramspec-test				\
	accumulator				\
	defaultiface				\
	dynamictype				\
	override				\
	singleton				\
	references

performance_programs = 				\
	performance				\
	performance-threaded

performance_LDADD = $(libgobject) $(libgthread)
performance_threaded_LDADD = $(libgobject) $(libgthread)
check_PROGRAMS = $(test_programs)
noinst_PROGRAMS = $(performance_programs)

TESTS = $(test_programs) $(performance_programs)
TESTS_ENVIRONMENT = srcdir=$(srcdir) \
	LIBCHARSET_ALIAS_DIR=$(top_builddir)/glib/libcharset \
	MALLOC_CHECK_=2	\
	MALLOC_PERTURB_=$$(($${RANDOM:-256} % 256))

if BUILDOPT_INSTALL_TESTS
insttestdir = $(pkglibexecdir)/installed-tests
insttest_PROGRAMS = $(test_programs)

testmetadir = $(datadir)/installed-tests/$(PACKAGE)
testmeta_DATA = $(test_programs:=.test)

%.test: %$(EXEEXT) Makefile
	$(AM_V_GEN) (echo '[Test]' > $@.tmp; \
	 echo 'Type=session' >> $@.tmp; \
	 echo 'Exec=env G_TEST_DATA=$(pkglibexecdir)/installed-tests $(pkglibexecdir)/installed-tests/$<' >> $@.tmp; \
	 mv $@.tmp $@)
endif

########################################################################

EXTRA_DIST += 		  \
	testmarshal.list
