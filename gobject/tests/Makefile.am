include $(top_srcdir)/Makefile.decl

INCLUDES = -g $(gobject_INCLUDES) $(GLIB_DEBUG_FLAGS)

noinst_PROGRAMS  = $(TEST_PROGS)
LDADD = ../libgobject-2.0.la $(top_builddir)/gthread/libgthread-2.0.la $(top_builddir)/glib/libglib-2.0.la

TEST_PROGS += threadtests dynamictests binding properties reference ifaceproperties
ifaceproperties_SOURCES = ifaceproperties.c testcommon.h
