## Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir) -DG_LOG_DOMAIN=g_log_domain_glib \
	@GLIB_DEBUG_FLAGS@ -DG_DISABLE_DEPRECATED -DGLIB_COMPILATION 

EXTRA_DIST = 			\
	gen-unicode-tables.pl

lib_LTLIBRARIES = libglib-1.3.la

libglib_1_3_la_SOURCES = 	\
	garray.c		\
	gasyncqueue.c		\
	gbacktrace.c		\
	gcache.c		\
	gcompletion.c		\
	gconvert.c		\
	gdataset.c		\
	gdate.c         	\
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
	gunidecomp.h		\
	gunidecomp.c		\
	gutils.c

EXTRA_libglib_1_3_la_SOURCES = \
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

install-data-local:
	@if test -f $(glibincludedir)/glist.h ; then					\
	  echo "*** Old headers found in $(glibincludedir). You should remove the" ;	\
	  echo "*** contents of this directory and type 'make install' again." ;	\
	  false ;									\
        fi

libglib_1_3_la_LIBADD = @GIO@ @GSPAWN@ @PLATFORMDEP@ @ICONV_LIBS@ @G_LIBS_EXTRA@
libglib_1_3_la_DEPENDENCIES = @GIO@ @GSPAWN@ @PLATFORMDEP@ 


if PLATFORM_WIN32
no_undefined = -no-undefined
endif
if OS_WIN32
# This requires a very new libtool
export_symbols = -export-symbols glib.def
endif

libglib_1_3_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-export-dynamic $(no_undefined) $(export_symbols)

if OS_WIN32
bin_PROGRAMS = gspawn-win32-helper
gspawn_win32_helper_LDADD = libglib-1.3.la
endif

