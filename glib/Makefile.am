## Process this file with automake to produce Makefile.in

if HAVE_GOOD_PRINTF
else
PRINTF_SUBDIR = gnulib
printf_la = gnulib/libgnulib.la
endif 

SUBDIRS = libcharset $(PRINTF_SUBDIR)

DIST_SUBDIRS = libcharset gnulib

INCLUDES = -I$(top_srcdir) -DG_LOG_DOMAIN=\"GLib\" \
	$(GLIB_DEBUG_FLAGS) -DG_DISABLE_DEPRECATED -DGLIB_COMPILATION 

glib.def: glib.symbols
	(echo -e EXPORTS; $(CPP) -P -DINCLUDE_VARIABLES -DINCLUDE_INTERNAL_SYMBOLS -DG_OS_WIN32 -DALL_FILES - <$(srcdir)/glib.symbols | sed -e '/^$$/d' -e 's/^/	/' -e 's/G_GNUC_[^ ]*//g') > glib.def

galias.h: glib.symbols
	  $(PERL) $(srcdir)/makegalias.pl < $(srcdir)/glib.symbols > galias.h

galiasdef.c: glib.symbols
	  $(PERL) $(srcdir)/makegalias.pl -def < $(srcdir)/glib.symbols > galiasdef.c

if OS_LINUX
if HAVE_GNUC_VISIBILITY
TESTS = abicheck.sh
endif
endif

BUILT_SOURCES = galias.h galiasdef.c

MIRRORING_TAB_SOURCES = 			\
	glib-mirroring-tab/Makefile 		\
	glib-mirroring-tab/gen-mirroring-tab.c	\
	glib-mirroring-tab/packtab.h		\
	glib-mirroring-tab/packtab.c

EXTRA_DIST = 			\
	makefile.msc.in		\
	glib.rc.in		\
	gen-unicode-tables.pl	\
	makegalias.pl		\
	abicheck.sh		\
	glib.symbols		\
	$(MIRRORING_TAB_SOURCES)

# These may be in the builddir too
BUILT_EXTRA_DIST = 		\
	makefile.msc		\
	glib.rc			\
	galias.h		\
	galiasdef.c

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
	gatomic.c		\
	gbacktrace.c		\
	gbsearcharray.h		\
	gcache.c		\
	gcompletion.c		\
	gconvert.c		\
	gdataset.c		\
	gdatasetprivate.h	\
	gdate.c         	\
	gdir.c			\
	gerror.c		\
	gfileutils.c		\
	ghash.c			\
	ghook.c			\
	giochannel.c    	\
	gkeyfile.c        	\
	glibintl.h		\
	glist.c			\
	gmain.c         	\
	gmappedfile.c		\
	gmarkup.c		\
	gmem.c			\
	gmessages.c		\
	gmirroringtable.h	\
	gnode.c			\
	goption.c		\
	gpattern.c		\
	gprimes.c		\
	gqsort.c		\
	gqueue.c		\
	grel.c			\
	grand.c			\
	gscanner.c		\
	gshell.c		\
	gslice.c		\
	gslist.c		\
	gstdio.c		\
	gstrfuncs.c		\
	gstring.c		\
	gthread.c       	\
	gthreadinit.h		\
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
	gunicodeprivate.h	\
	gutils.c		\
	gdebug.h		\
	gprintf.c		\
	gprintfint.h

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
	gatomic.h	\
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
	gi18n.h		\
	gi18n-lib.h	\
	giochannel.h	\
	gkeyfile.h 	\
	glist.h		\
	gmacros.h	\
	gmain.h		\
	gmappedfile.h	\
	gmarkup.h	\
	gmem.h		\
	gmessages.h	\
	gnode.h		\
	goption.h	\
	gpattern.h	\
	gprimes.h	\
	gqsort.h	\
	gquark.h	\
	gqueue.h	\
	grand.h		\
	grel.h		\
	gscanner.h	\
	gshell.h	\
	gslice.h	\
	gslist.h	\
	gspawn.h	\
	gstdio.h	\
	gstrfuncs.h	\
	gstring.h	\
	gthread.h	\
	gthreadpool.h	\
	gtimer.h	\
	gtree.h		\
	gtypes.h	\
	gunicode.h	\
	gutils.h	\
	gwin32.h	\
	gprintf.h

install-data-local: install-ms-lib install-def-file
	@if test -f $(glibincludedir)/glist.h ; then					\
	  echo "*** Old headers found in $(glibincludedir). You should remove the" ;	\
	  echo "*** contents of this directory and type 'make install' again." ;	\
	  false ;									\
        fi

uninstall-local: uninstall-ms-lib uninstall-def-file

if PLATFORM_WIN32
no_undefined = -no-undefined
endif

if OS_WIN32
# This requires a very new libtool
export_symbols = -export-symbols glib.def

install-def-file:
	$(INSTALL) glib.def $(DESTDIR)$(libdir)/glib-2.0.def

uninstall-def-file:
	-rm $(DESTDIR)$(libdir)/glib-2.0.def
else
install-def-file:
uninstall-def-file:

export_symbols = $(LIBTOOL_EXPORT_OPTIONS)
endif

if OS_WIN32
glib_win32_res = glib-win32-res.o
glib_win32_res_ldflag = -Wl,$(glib_win32_res)
endif

libglib_2_0_la_LIBADD = libcharset/libcharset.la $(printf_la) @GIO@ @GSPAWN@ @PLATFORMDEP@ @ICONV_LIBS@ @G_LIBS_EXTRA@ 
libglib_2_0_la_DEPENDENCIES = libcharset/libcharset.la $(printf_la) @GIO@ @GSPAWN@ @PLATFORMDEP@ $(glib_win32_res) @GLIB_DEF@

libglib_2_0_la_LDFLAGS = \
	 $(glib_win32_res_ldflag) \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-export-dynamic $(no_undefined) $(export_symbols)

if OS_WIN32
bin_PROGRAMS = gspawn-win32-helper gspawn-win32-helper-console
gspawn_win32_helper_LDADD = libglib-2.0.la
gspawn_win32_helper_LDFLAGS = -mwindows
gspawn_win32_helper_console_LDADD = libglib-2.0.la

glib-win32-res.o: glib.rc
	$(WINDRES) glib.rc $@
endif

gspawn-win32-helper-console.c:
	echo '#include "gspawn-win32-helper.c"' >$@

glib-2.0.lib: libglib-2.0.la glib.def
	lib -name:libglib-2.0-$(LT_CURRENT_MINUS_AGE).dll -def:glib.def -out:$@

dist-hook: $(BUILT_EXTRA_DIST)
	files='$(BUILT_EXTRA_DIST)'; \
	for f in $$files; do \
	  if test -f $$f; then d=.; else d=$(srcdir); fi; \
	  cp $$d/$$f $(distdir) || exit 1; done
