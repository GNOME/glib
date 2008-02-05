include $(top_srcdir)/Makefile.decl

INCLUDES = -g -I$(top_srcdir) -I$(top_srcdir)/glib $(GLIB_DEBUG_FLAGS)

noinst_PROGRAMS  = $(TEST_PROGS)
libgobject_LDADD = ../libgobject-2.0.la $(top_builddir)/gthread/libgthread-2.0.la $(top_builddir)/glib/libglib-2.0.la

TEST_PROGS             += threadtests
threadtests_SOURCES	= threadtests.c
threadtests_LDADD	= $(libgobject_LDADD)
