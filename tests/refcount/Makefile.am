include $(top_srcdir)/Makefile.decl

AM_CPPFLAGS =					\
	$(gmodule_INCLUDES) 			\
	-DGLIB_DISABLE_DEPRECATION_WARNINGS	\
	$(GLIB_DEBUG_FLAGS)

libglib = $(top_builddir)/glib/libglib-2.0.la
libgmodule = $(top_builddir)/gmodule/libgmodule-2.0.la
libgobject = $(top_builddir)/gobject/libgobject-2.0.la

LDADD = $(libglib) $(libgobject)

test_programs = 				\
	closures				\
	objects					\
	objects2				\
	properties				\
	properties2				\
	properties3				\
	properties4				\
	signal1					\
	signal2					\
	signal3					\
	signal4



signal1_SOURCES = signals.c
signal1_CFLAGS = -DTESTNUM=1 $(AM_CFLAGS)
signal2_SOURCES = signals.c
signal2_CFLAGS = -DTESTNUM=2 $(AM_CFLAGS)
signal3_SOURCES = signals.c
signal3_CFLAGS = -DTESTNUM=3 $(AM_CFLAGS)
signal4_SOURCES = signals.c
signal4_CFLAGS = -DTESTNUM=4 $(AM_CFLAGS)

check_PROGRAMS = $(test_programs)

all: $(check_PROGRAMS)

TESTS = $(test_programs)
TESTS_ENVIRONMENT = srcdir=$(srcdir) \
	LIBCHARSET_ALIAS_DIR=$(top_builddir)/glib/libcharset
