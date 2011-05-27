include $(top_srcdir)/glib.mk
include $(top_srcdir)/glib-build.mk

LDADD = $(top_builddir)/gobject/libgobject-2.0.la $(top_builddir)/glib/libglib-2.0.la
AM_CPPFLAGS = $(gmodule_INCLUDES) $(GLIB_DEBUG_FLAGS)
DEFS = -DGLIB_DISABLE_DEPRECATION_WARNINGS -DG_LOG_DOMAIN=\"GLib\"
AM_CFLAGS = -g

# So far, only two gtester-ified cases
test_programs = \
	gvalue-test				\
	paramspec-test				\
	$(NULL)

# These are not yet gtester-ified, so mark them as for 'installed' only so we
# don't run them under the framework.  We will handle them manually below.
installed_test_programs = \
	deftype					\
	accumulator				\
	defaultiface				\
	dynamictype				\
	override				\
	signals					\
	singleton				\
	references				\
	$(NULL)

# Don't install these ones, and keep them out of 'make check' because they take too long...
noinst_PROGRAMS += \
	performance				\
	performance-threaded			\
	$(NULL)

# Run the 'installed' tests manually in-tree.
# This will cause them to be built even if installed tests are disabled.
check_PROGRAMS += $(installed_test_programs) $(installed_test_extra_programs)
TESTS += $(installed_test_programs)
TESTS_ENVIRONMENT = \
	LIBCHARSET_ALIAS_DIR=$(top_builddir)/glib/libcharset    \
	MALLOC_CHECK_=2                                         \
	MALLOC_PERTURB_=$$(($${RANDOM:-256} % 256))

accumulator_SOURCES = accumulator.c testmarshal.c testmarshal.h
signals_SOURCES = signals.c
defaultiface_SOURCES = defaultiface.c testmodule.c testmodule.h
dynamictype_SOURCES = dynamictype.c testmodule.c testmodule.h

if ENABLE_TIMELOOP
installed_test_programs += timeloop-closure
endif

testmarshal_sources = $(accumulator_SOURCES) $(defaultiface_SOURCES) $(dynamictype_SOURCES)
DISTCLEANFILES += testmarshal.c testmarshal.h

EXTRA_DIST += \
	testcommon.h
