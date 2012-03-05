include $(top_srcdir)/Makefile.decl

INCLUDES = -g $(gobject_INCLUDES) $(GLIB_DEBUG_FLAGS)

noinst_PROGRAMS  = $(TEST_PROGS)
LDADD = ../libgobject-2.0.la $(top_builddir)/gthread/libgthread-2.0.la $(top_builddir)/glib/libglib-2.0.la

TEST_PROGS += 		\
	boxed		\
	enums		\
	param		\
	signals		\
	threadtests	\
	dynamictests	\
	binding		\
	properties	\
	reference	\
	ifaceproperties	\
	valuearray

signals_SOURCES = signals.c marshalers.c

marshalers.h: Makefile.am marshalers.list
	$(AM_V_GEN) ../glib-genmarshal --prefix=test $(srcdir)/marshalers.list --header --valist-marshallers > marshalers.h

marshalers.c: Makefile.am marshalers.list
	$(AM_V_GEN) ../glib-genmarshal --prefix=test $(srcdir)/marshalers.list --body --valist-marshallers > marshalers.c

BUILT_SOURCES = marshalers.h marshalers.c
CLEANFILES = marshalers.h marshalers.c

ifaceproperties_SOURCES = ifaceproperties.c testcommon.h

EXTRA_DIST += marshalers.list
