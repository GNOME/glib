## Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/glib -I$(top_srcdir)/gthread \
	-DG_LOG_DOMAIN=\"GThread\" @GTHREAD_COMPILE_IMPL_DEFINES@ \
	@GLIB_DEBUG_FLAGS@

EXTRA_DIST =				\
		makefile.mingw.in	\
		makefile.msc.in		\
		gthread-posix.c		\
		gthread-solaris.c	\
		gthread-win32.c		\
		gthread-none.c		\
		gthread.def		\
		gthread.rc.in

BUILT_EXTRA_DIST = \
		makefile.mingw		\
		makefile.msc		\
		gthread.rc

libglib = $(top_builddir)/glib/libglib-1.3.la

top_builddir_full=`cd \$(top_builddir); pwd`

lib_LTLIBRARIES = libgthread-1.3.la

if PLATFORM_WIN32
no_undefined = -no-undefined
endif
if OS_WIN32
export_symbols = -export-symbols gthread.def
endif

libgthread_1_3_la_SOURCES = gthread-impl.c
libgthread_1_3_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-export-dynamic $(no_undefined) $(export_symbols)

libgthread_1_3_la_LIBADD = @G_THREAD_WIN32_RESOURCE@ @G_THREAD_LIBS_EXTRA@ @G_THREAD_LIBS@ $(libglib)

libgthread_1_3_la_DEPENDENCIES = @G_THREAD_WIN32_RESOURCE@

if OS_WIN32
@G_THREAD_WIN32_RESOURCE@ : gthread.rc
	$(top_srcdir)/build/win32/lt-compile-resource gthread.rc @G_THREAD_WIN32_RESOURCE@
endif

gthread.rc: $(top_builddir)/config.status $(top_srcdir)/gthread/gthread.rc.in
	cd $(top_builddir) && CONFIG_FILES=gthread/$@ CONFIG_HEADERS= $(SHELL) ./config.status

makefile.mingw: $(top_builddir)/config.status $(top_srcdir)/gthread/makefile.mingw.in
	cd $(top_builddir) && CONFIG_FILES=gthread/$@ CONFIG_HEADERS= $(SHELL) ./config.status

makefile.msc: $(top_builddir)/config.status $(top_srcdir)/gthread/makefile.msc.in
	cd $(top_builddir) && CONFIG_FILES=gthread/$@ CONFIG_HEADERS= $(SHELL) ./config.status

dist-hook: $(BUILT_EXTRA_DIST)
	files='$(BUILT_EXTRA_DIST)'; \
	for f in $$files; do \
	  if test -f $$f; then d=.; else d=$(srcdir); fi; \
	  cp $$d/$$f $(distdir) || exit 1; done

