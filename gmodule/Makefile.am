## Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/gmodule \
	-DG_LOG_DOMAIN=\"GModule\" @GLIB_DEBUG_FLAGS@

EXTRA_DIST =	\
		makefile.mingw.in \
		makefile.msc.in \
		gmoduleconf.h.in \
		gmodule.def	\
		gmodule-dl.c	\
		gmodule-dld.c	\
		gmodule-os2.c	\
		gmodule-win32.c	\
		gmodule-beos.c	\
		gmoduleconf.h.win32 \
		gmodule.rc.in

BUILT_EXTRA_DIST = \
		makefile.mingw	\
		makefile.msc	\
		gmodule.rc

BUILT_SOURCES = gmoduleconf.h
gmoduleconf.h:	gmoduleconf.h.in

glibincludedir=$(includedir)/glib-2.0
glibinclude_HEADERS = \
		gmodule.h

libglib = $(top_builddir)/libglib-1.3.la

top_builddir_full=`cd \$(top_builddir); pwd`

lib_LTLIBRARIES = libgmodule-1.3.la
noinst_LTLIBRARIES = libgplugin_a.la libgplugin_b.la

if PLATFORM_WIN32
no_undefined = -no-undefined
endif
if OS_WIN32
export_symbols = -export-symbols gmodule.def
endif

libgmodule_1_3_la_SOURCES = gmodule.c
libgmodule_1_3_la_LDFLAGS = \
	@G_MODULE_LDFLAGS@ \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-export-dynamic $(no_undefined) $(export_symbols)

libgmodule_1_3_la_LIBADD = @G_MODULE_LIBS_EXTRA@ @G_MODULE_LIBS@ $(libglib)

libgplugin_a_la_SOURCES = libgplugin_a.c
libgplugin_a_la_LDFLAGS = @G_MODULE_LDFLAGS@ -avoid-version -module $(no_undefined)
libgplugin_a_la_LIBADD = @G_MODULE_LIBS@ @G_MODULE_LIBS_EXTRA@ @G_MODULE_PLUGIN_LIBS@ libgmodule-1.3.la $(libglib)

libgplugin_b_la_SOURCES = libgplugin_b.c
libgplugin_b_la_LDFLAGS = @G_MODULE_LDFLAGS@ -avoid-version -module $(no_undefined)
libgplugin_b_la_LIBADD = @G_MODULE_LIBS@ @G_MODULE_LIBS_EXTRA@ @G_MODULE_PLUGIN_LIBS@ libgmodule-1.3.la $(libglib)

noinst_PROGRAMS = testgmodule
testgmodule_LDFLAGS += @G_MODULE_LDFLAGS@
testgmodule_LDADD = libgmodule-1.3.la $(libglib) @G_MODULE_LIBS@

gmodule.rc: $(top_builddir)/config.status $(top_srcdir)/gmodule/gmodule.rc.in
	cd $(top_builddir) && CONFIG_FILES=gmodule/$@ CONFIG_HEADERS= $(SHELL) ./config.status

makefile.mingw: $(top_builddir)/config.status $(top_srcdir)/gmodule/makefile.mingw.in
	cd $(top_builddir) && CONFIG_FILES=gmodule/$@ CONFIG_HEADERS= $(SHELL) ./config.status

makefile.msc: $(top_builddir)/config.status $(top_srcdir)/gmodule/makefile.msc.in
	cd $(top_builddir) && CONFIG_FILES=gmodule/$@ CONFIG_HEADERS= $(SHELL) ./config.status

.PHONY: files release

files:
	@files=`ls $(DISTFILES) 2> /dev/null `; for p in $$files; do \
	  echo $$p; \
	done

release:
	$(MAKE) dist distdir=$(PACKAGE)`date +"%y%m%d"`

dist-hook: $(BUILT_EXTRA_DIST)
	@files='$(BUILT_EXTRA_DIST)'; \
	for f in $$files; do \
	  if test -f $$f; then d=.; else d=$(srcdir); fi; \
	  cp $$d/$$f $(distdir) || exit 1; done

