## Process this file with automake to produce Makefile.in
include $(top_srcdir)/Makefile.decl

AM_CPPFLAGS = 				\
	-I$(top_srcdir) 		\
	-I$(top_srcdir)/glib 		\
	-I$(top_srcdir)/gthread 	\
	-DG_LOG_DOMAIN=\"GThread\" 	\
	@GTHREAD_COMPILE_IMPL_DEFINES@ 	\
	@GLIB_DEBUG_FLAGS@ 		\
	-DG_DISABLE_DEPRECATED

EXTRA_DIST +=				\
		makefile.msc.in		\
		gthread-posix.c		\
		gthread-win32.c		\
		gthread-none.c		\
		gthread.def		\
		gthread.rc.in

BUILT_EXTRA_DIST = 			\
		makefile.msc		\
		gthread.rc

libglib = $(top_builddir)/glib/libglib-2.0.la

top_builddir_full=`cd \$(top_builddir); pwd`

lib_LTLIBRARIES = libgthread-2.0.la

if MS_LIB_AVAILABLE
noinst_DATA = gthread-2.0.lib

install-ms-lib:
	$(INSTALL) gthread-2.0.lib $(DESTDIR)$(libdir)

uninstall-ms-lib:
	-rm $(DESTDIR)$(libdir)/gthread-2.0.lib
else
install-ms-lib:
uninstall-ms-lib:
endif

if PLATFORM_WIN32
no_undefined = -no-undefined
endif

if OS_WIN32
export_symbols = -export-symbols $(srcdir)/gthread.def

install-def-file:
	$(INSTALL) $(srcdir)/gthread.def $(DESTDIR)$(libdir)/gthread-2.0.def

uninstall-def-file:
	-rm $(DESTDIR)$(libdir)/gthread-2.0.def
else
install-def-file:
uninstall-def-file:
endif

if OS_WIN32
gthread_win32_res = gthread-win32-res.o
gthread_win32_res_ldflag = -Wl,$(gthread_win32_res)
endif

libgthread_2_0_la_SOURCES = gthread-impl.c
libgthread_2_0_la_LDFLAGS = \
	$(gthread_win32_res_ldflag) \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-export-dynamic $(no_undefined) $(export_symbols)

libgthread_2_0_la_LIBADD = $(G_THREAD_LIBS_EXTRA) $(G_THREAD_LIBS_FOR_GTHREAD) $(libglib)

libgthread_2_0_la_DEPENDENCIES = $(gthread_win32_res) $(GTHREAD_DEF)

if OS_WIN32
gthread-win32-res.o: gthread.rc
	$(WINDRES) gthread.rc $@
endif

gthread-2.0.lib: libgthread-2.0.la gthread.def
	lib -name:libgthread-2.0-$(LT_CURRENT_MINUS_AGE).dll -def:$(srcdir)/gthread.def -out:$@

dist-hook: $(BUILT_EXTRA_DIST)
	files='$(BUILT_EXTRA_DIST)'; \
	for f in $$files; do \
	  if test -f $$f; then d=.; else d=$(srcdir); fi; \
	  cp $$d/$$f $(distdir) || exit 1; done

install-data-local: install-ms-lib install-def-file

uninstall-local: uninstall-ms-lib uninstall-def-file

