NULL =

LDADD = \
	$(top_builddir)/gio/libgio-2.0.la               \
	$(top_builddir)/gobject/libgobject-2.0.la       \
	$(top_builddir)/gmodule/libgmodule-2.0.la       \
	$(top_builddir)/glib/libglib-2.0.la             \
	$(NULL)


AM_CPPFLAGS = \
	$(gio_INCLUDES) $(GLIB_DEBUG_FLAGS) 		\
	-I$(top_builddir)/gio 				\
	-I$(top_srcdir)/gio

testmoduledir = $(installed_testdir)/modules
testmodule_LTLIBRARIES = \
	libtestmodulea.la \
	libtestmoduleb.la

libtestmodulea_la_SOURCES = test-module-a.c
libtestmodulea_la_LIBADD = $(LDADD)
libtestmodulea_la_LDFLAGS = $(LDFLAGS) -no-undefined -avoid-version

libtestmoduleb_la_SOURCES = test-module-b.c
libtestmoduleb_la_LIBADD = $(LDADD)
libtestmoduleb_la_LDFLAGS =$(LDFLAGS) -no-undefined -avoid-version
