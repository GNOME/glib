## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

SUBDIRS = . gobject gmodule gthread docs tests build

INCLUDES = -DG_LOG_DOMAIN=g_log_domain_glib @GLIB_DEBUG_FLAGS@ \
	-DG_DISABLE_DEPRECATED -DGLIB_COMPILATION

EXTRA_DIST = 			\
	ChangeLog.pre-1-2	\
	glib-2.0.m4		\
	glib.spec.in		\
	NEWS.pre-1-3		\
	acglib.m4		\
	sanity_check		\
	README.win32		\
	TODO.xml		\
	glib.def		\
	makefile.msc.in		\
	makefile.mingw.in	\
	glibconfig.h.win32.in	\
	config.h.win32.in	\
	build-dll		\
	glib.rc.in		\
	glib-2.0.pc.in 		\
	gobject-2.0.pc.in 	\
	gmodule-2.0.pc.in	\
	gthread-2.0.pc.in	\
	gen-unicode-tables.pl

# These may be in the builddir too
BUILT_EXTRA_DIST = \
	makefile.msc		\
	makefile.mingw		\
	glibconfig.h.win32	\
	config.h.win32		\
	glib.rc			\
	glib.spec

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
	glib.h 		\
	glib-object.h	\
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

configexecincludedir = $(libdir)/glib-2.0/include
#configexecinclude_DATA = glibconfig.h

install-exec-local: glibconfig.h
	$(mkinstalldirs) $(DESTDIR)$(configexecincludedir)
	file=$(DESTDIR)$(configexecincludedir)/glibconfig.h; \
	if test -r $$file && cmp -s glibconfig.h $$file; then :; \
	else $(INSTALL_DATA) glibconfig.h $$file; fi

CONFIGURE_DEPENDENCIES = acglib.m4

BUILT_SOURCES = stamp-gc-h #note: not glibconfig.h
glibconfig.h: stamp-gc-h
	@if test -f glibconfig.h; then :; \
	else rm -f stamp-gc-h; $(MAKE) stamp-gc-h; fi
stamp-gc-h: config.status
	CONFIG_FILES= CONFIG_HEADERS= CONFIG_OTHER=glibconfig.h ./config.status
	echo timestamp > stamp-gc-h

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
if ENABLE_TIMELOOP
timeloop = timeloop
endif
noinst_PROGRAMS = testglib testgdate testgdateparser $(timeloop)
testglib_LDADD = libglib-1.3.la
testgdate_LDADD = libglib-1.3.la
testgdateparser_LDADD = libglib-1.3.la
if ENABLE_TIMELOOP
timeloop_LDADD = libglib-1.3.la
endif
m4datadir = $(datadir)/aclocal
m4data_DATA = glib-2.0.m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = glib-2.0.pc gobject-2.0.pc gmodule-2.0.pc gthread-2.0.pc

glibconfig.h.win32: $(top_builddir)/config.status $(top_srcdir)/glibconfig.h.win32.in
	cd $(top_builddir) && CONFIG_FILES=$@ CONFIG_HEADERS= $(SHELL) ./config.status

config.h.win32: $(top_builddir)/config.status $(top_srcdir)/config.h.win32.in
	cd $(top_builddir) && CONFIG_FILES=$@ CONFIG_HEADERS= $(SHELL) ./config.status

makefile.msc: $(top_builddir)/config.status $(top_srcdir)/makefile.msc.in
	cd $(top_builddir) && CONFIG_FILES=$@ CONFIG_HEADERS= $(SHELL) ./config.status

makefile.mingw: $(top_builddir)/config.status $(top_srcdir)/makefile.mingw.in
	cd $(top_builddir) && CONFIG_FILES=$@ CONFIG_HEADERS= $(SHELL) ./config.status

glib.rc: $(top_builddir)/config.status $(top_srcdir)/glib.rc.in
	cd $(top_builddir) && CONFIG_FILES=$@ CONFIG_HEADERS= $(SHELL) ./config.status

bin_SCRIPTS=glib-config-2.0
BUILT_SOURCES=glib-config-2.0
glib-config-2.0: glib-config-2.0.in

.PHONY: files release sanity snapshot

files:
	@files=`ls $(DISTFILES) 2> /dev/null `; for p in $$files; do \
	  echo $$p; \
	done

release:
	rm -rf .deps */.deps
	cd docs && make glib.html
	$(MAKE) distcheck

sanity:
	./sanity_check $(VERSION)

snapshot:
	$(MAKE) dist distdir=$(PACKAGE)-snap`date +"%Y%m%d"`

snapcheck:
	$(MAKE) distcheck distdir=$(PACKAGE)-snap`date +"%Y%m%d"`

dist-hook: $(BUILT_EXTRA_DIST)
	files='$(BUILT_EXTRA_DIST)'; \
	for f in $$files; do \
	  if test -f $$f; then d=.; else d=$(srcdir); fi; \
	  cp $$d/$$f $(distdir) || exit 1; done
