include $(top_srcdir)/Makefile.decl

AM_CPPFLAGS = 					\
	-g					\
	-DG_LOG_DOMAIN=\"GLib-GObject\"		\
	$(gobject_INCLUDES)			\
	$(GLIB_DEBUG_FLAGS)

AM_CFLAGS = $(GLIB_WARN_CFLAGS)

if CROSS_COMPILING
  glib_genmarshal=$(GLIB_GENMARSHAL)
else
  glib_genmarshal=$(top_builddir)/gobject/glib-genmarshal
endif

noinst_PROGRAMS  = $(TEST_PROGS)
LDADD = ../libgobject-2.0.la $(top_builddir)/gthread/libgthread-2.0.la $(top_builddir)/glib/libglib-2.0.la

TEST_PROGS += 		\
	qdata		\
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
	$(AM_V_GEN) $(glib_genmarshal) --prefix=test $(srcdir)/marshalers.list --header --valist-marshallers > marshalers.h

marshalers.c: Makefile.am marshalers.list
	$(AM_V_GEN) (echo "#include \"marshalers.h\""; $(glib_genmarshal) --prefix=test $(srcdir)/marshalers.list --body --valist-marshallers) > $@.tmp && mv $@.tmp $@

BUILT_SOURCES = marshalers.h marshalers.c
CLEANFILES = marshalers.h marshalers.c

ifaceproperties_SOURCES = ifaceproperties.c testcommon.h

EXTRA_DIST += marshalers.list
