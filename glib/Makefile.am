## Process this file with automake to produce Makefile.in

SUBDIRS=libcharset

INCLUDES = -I$(top_srcdir) -DG_LOG_DOMAIN=\"GLib\" \
	@GLIB_DEBUG_FLAGS@ -DG_DISABLE_DEPRECATED -DGLIB_COMPILATION 

EXTRA_DIST = 			\
	makefile.mingw.in	\
	makefile.msc.in		\
	glib.rc.in		\
	gen-unicode-tables.pl	\
	glib.def

# These may be in the builddir too
BUILT_EXTRA_DIST = 		\
	makefile.msc		\
	makefile.mingw		\
	glib.rc

lib_LTLIBRARIES = libglib-2.0.la

if MS_LIB_AVAILABLE
noinst_DATA = glib-2.0.lib

install-ms-lib:
	$(INSTALL) glib-2.0.lib $(DESTDIR)$(libdir)

uninstall-ms-lib:
	-rm $(DESTDIR)$(libdir)/glib-2.0.lib
else
install-ms-lib:
uninstall-ms-lib:
endif

libglib_2_0_la_SOURCES = 	\
	garray.c		\
	gasyncqueue.c		\
	gbacktrace.c		\
	gbsearcharray.c		\
	gbsearcharray.h		\
	gcache.c		\
	gcompletion.c		\
	gconvert.c		\
	gdataset.c		\
	gdate.c         	\
	gdir.c			\
	gerror.c		\
	gfileutils.c		\
	ghash.c			\
	ghook.c			\
	giochannel.c    	\
	glibintl.h		\
	glist.c			\
	gmain.c         	\
	gmarkup.c		\
	gmem.c			\
	gmessages.c		\
	gnode.c			\
	gpattern.c		\
	gprimes.c		\
	gqsort.c		\
	gqueue.c		\
	grel.c			\
	grand.c			\
	gscanner.c		\
	gshell.c		\
	gslist.c		\
	gstrfuncs.c		\
	gstring.c		\
	gthread.c       	\
	gthreadpool.c   	\
	gtimer.c		\
	gtree.c			\
	guniprop.c		\
	gutf8.c			\
	gunibreak.h		\
	gunibreak.c		\
	gunichartables.h	\
	gunicollate.c		\
	gunicomp.h		\
	gunidecomp.h		\
	gunidecomp.c		\
	gutils.c		\
	gdebug.h

EXTRA_libglib_2_0_la_SOURCES = \
	giounix.c	\
	giowin32.c	\
	gspawn.c	\
	gspawn-win32.c	\
	gwin32.c

glibincludedir=$(includedir)/glib-2.0
glibinclude_HEADERS =   \
	glib-object.h	\
	glib.h

glibsubincludedir=$(includedir)/glib-2.0/glib
glibsubinclude_HEADERS =   \
	galloca.h	\
	garray.h	\
	gasyncqueue.h	\
	gbacktrace.h	\
	gcache.h	\
	gcompletion.h	\
	gconvert.h	\
	gdataset.h	\
	gdate.h		\
	gdir.h		\
	gerror.h	\
	gfileutils.h	\
	ghash.h		\
	ghook.h		\
	giochannel.h	\
	glist.h		\
	gmacros.h	\
	gmain.h		\
	gmarkup.h	\
	gmem.h		\
	gmessages.h	\
	gnode.h		\
	gpattern.h	\
	gprimes.h	\
	gqsort.h	\
	gquark.h	\
	gqueue.h	\
	grand.h		\
	grel.h		\
	gscanner.h	\
	gshell.h	\
	gslist.h	\
	gspawn.h	\
	gstrfuncs.h	\
	gstring.h	\
	gthread.h	\
	gthreadpool.h	\
	gtimer.h	\
	gtree.h		\
	gtypes.h	\
	gunicode.h	\
	gutils.h	\
	gwin32.h

install-data-local: install-ms-lib install-libtool-import-lib
	@if test -f $(glibincludedir)/glist.h ; then					\
	  echo "*** Old headers found in $(glibincludedir). You should remove the" ;	\
	  echo "*** contents of this directory and type 'make install' again." ;	\
	  false ;									\
        fi

uninstall-local: uninstall-ms-lib uninstall-libtool-import-lib

if PLATFORM_WIN32
no_undefined = -no-undefined
endif

if OS_WIN32
# This requires a very new libtool
export_symbols = -export-symbols glib.def

install-libtool-import-lib:
	$(INSTALL) .libs/libglib-2.0.dll.a $(DESTDIR)$(libdir)
uninstall-libtool-import-lib:
	-rm $(DESTDIR)$(libdir)/libglib-2.0.dll.a
else
install-libtool-import-lib:
uninstall-libtool-import-lib:
endif

libglib_2_0_la_LIBADD = libcharset/libcharset.la @GIO@ @GSPAWN@ @PLATFORMDEP@ @G_LIB_WIN32_RESOURCE@ @ICONV_LIBS@ @G_LIBS_EXTRA@ 
libglib_2_0_la_DEPENDENCIES = libcharset/libcharset.la @GIO@ @GSPAWN@ @PLATFORMDEP@ @G_LIB_WIN32_RESOURCE@ @GLIB_DEF@

libglib_2_0_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-export-dynamic $(no_undefined) $(export_symbols)

if OS_WIN32
bin_PROGRAMS = gspawn-win32-helper
gspawn_win32_helper_LDADD = libglib-2.0.la
gspawn_win32_helper_LDFLAGS = -mwindows

@G_LIB_WIN32_RESOURCE@ : glib.rc
	$(top_srcdir)/build/win32/lt-compile-resource glib.rc @G_LIB_WIN32_RESOURCE@
endif

glib-2.0.lib: libglib-2.0.la glib.def
	lib -name:libglib-2.0-@LT_CURRENT_MINUS_AGE@.dll -def:glib.def -out:$@

glib.rc: $(top_builddir)/config.status $(top_srcdir)/glib/glib.rc.in
	cd $(top_builddir) && CONFIG_FILES=glib/$@ CONFIG_HEADERS= $(SHELL) ./config.status

makefile.mingw: $(top_builddir)/config.status $(top_srcdir)/glib/makefile.mingw.in
	cd $(top_builddir) && CONFIG_FILES=glib/$@ CONFIG_HEADERS= $(SHELL) ./config.status

makefile.msc: $(top_builddir)/config.status $(top_srcdir)/glib/makefile.msc.in
	cd $(top_builddir) && CONFIG_FILES=glib/$@ CONFIG_HEADERS= $(SHELL) ./config.status

dist-hook: $(BUILT_EXTRA_DIST)
	files='$(BUILT_EXTRA_DIST)'; \
	for f in $$files; do \
	  if test -f $$f; then d=.; else d=$(srcdir); fi; \
	  cp $$d/$$f $(distdir) || exit 1; done
