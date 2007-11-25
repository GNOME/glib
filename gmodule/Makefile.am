## Process this file with automake to produce Makefile.in
include $(top_srcdir)/Makefile.decl

AM_CPPFLAGS = 				\
	-I$(top_srcdir) 		\
	-I$(top_srcdir)/glib 		\
	-I$(top_srcdir)/gmodule 	\
	-DG_LOG_DOMAIN=\"GModule\" 	\
	@GLIB_DEBUG_FLAGS@ 		\
	-DG_DISABLE_DEPRECATED

EXTRA_DIST +=				\
		makefile.msc.in 	\
		gmoduleconf.h.in 	\
		gmodule.def		\
		gmodule-dl.c		\
		gmodule-dld.c		\
		gmodule-dyld.c		\
		gmodule-os2.c		\
		gmodule-win32.c		\
		gmodule-beos.c		\
		gmodule-ar.c		\
		gmoduleconf.h.win32 	\
		gmodule.rc.in

BUILT_EXTRA_DIST = \
		makefile.msc	\
		gmodule.rc

BUILT_SOURCES = gmoduleconf.h
gmoduleconf.h:	gmoduleconf.h.in

glibincludedir=$(includedir)/glib-2.0
glibinclude_HEADERS = \
		gmodule.h

libglib = $(top_builddir)/glib/libglib-2.0.la

top_builddir_full=`cd \$(top_builddir); pwd`

lib_LTLIBRARIES = libgmodule-2.0.la

if MS_LIB_AVAILABLE
noinst_DATA = gmodule-2.0.lib

install-ms-lib:
	$(INSTALL) gmodule-2.0.lib $(DESTDIR)$(libdir)

uninstall-ms-lib:
	-rm $(DESTDIR)$(libdir)/gmodule-2.0.lib
else
install-ms-lib:
uninstall-ms-lib:
endif

if PLATFORM_WIN32
no_undefined = -no-undefined
endif

if OS_WIN32
export_symbols = -export-symbols $(srcdir)/gmodule.def

install-def-file:
	$(INSTALL) $(srcdir)/gmodule.def $(DESTDIR)$(libdir)/gmodule-2.0.def

uninstall-def-file:
	-rm $(DESTDIR)$(libdir)/gmodule-2.0.def
else
install-def-file:
uninstall-def-file:
endif

if OS_WIN32
gmodule_win32_res = gmodule-win32-res.o
gmodule_win32_res_ldflag = -Wl,$(gmodule_win32_res)
endif

libgmodule_2_0_la_SOURCES = gmodule.c
libgmodule_2_0_la_LDFLAGS = \
	$(gmodule_win32_res_ldflag) \
	$(G_MODULE_LDFLAGS) \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-export-dynamic $(no_undefined) $(export_symbols)

libgmodule_2_0_la_LIBADD = $(G_MODULE_LIBS_EXTRA) $(G_MODULE_LIBS) $(libglib)

libgmodule_2_0_la_DEPENDENCIES = $(gmodule_win32_res) $(GMODULE_DEF)

if OS_WIN32
gmodule-win32-res.o: gmodule.rc
	$(WINDRES) gmodule.rc $@
endif

gmodule-2.0.lib: libgmodule-2.0.la gmodule.def
	lib -name:libgmodule-2.0-$(LT_CURRENT_MINUS_AGE).dll -def:$(srcdir)/gmodule.def -out:$@

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


install-data-local: install-ms-lib install-def-file

uninstall-local: uninstall-ms-lib uninstall-def-file
