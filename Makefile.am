## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

if HAVE_THREADS
gthread = gthread
else
gthread =
endif

SUBDIRS = . gobject gmodule $(gthread) docs tests build

bin_SCRIPTS=glib-config-2.0
BUILT_SOURCES=glib-config-2.0
glib-config-2.0: glib-config-2.0.in

INCLUDES = -DG_LOG_DOMAIN=g_log_domain_glib @GLIB_DEBUG_FLAGS@

EXTRA_DIST = 			\
	ChangeLog.pre-1-2	\
	glib-2.0.m4		\
	glib.spec.in		\
	acglib.m4		\
	sanity_check		\
	README.win32		\
	TODO.xml		\
	glib.def		\
	makefile.msc		\
	makefile.msc.in		\
	makefile.mingw		\
	makefile.mingw.in	\
	giowin32.c		\
	glibconfig.h.win32	\
	glibconfig.h.win32.in	\
	gwin32.c		\
	config.h.win32		\
	config.h.win32.in	\
	build-dll		\
	glib.rc			\
	glib.rc.in		\
	glib-2.0.pc.in 		\
	gobject-2.0.pc.in 	\
	gmodule-2.0.pc.in	\
	gthread-2.0.pc.in

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
	ghash.c			\
	ghook.c			\
	giochannel.c    	\
	giounix.c       	\
	glist.c			\
	gmain.c         	\
	gmem.c			\
	gmessages.c		\
	gnode.c			\
	gprimes.c		\
	gqueue.c		\
	grel.c			\
	grand.c			\
	gscanner.c		\
	gslist.c		\
	gstrfuncs.c		\
	gstring.c		\
	gthread.c       	\
	gthreadpool.c   	\
	gtimer.c		\
	gtree.c			\
	guniprop.c		\
	gutf8.c			\
	gunichartables.h	\
	gunidecomp.h		\
	gunidecomp.c		\
	gutils.c		\
	@ALLOCA@

glibincludedir=$(includedir)/glib-2.0
glibinclude_HEADERS =   \
	gerror.h	\
	glib.h 		\
	glib-object.h	\
	gunicode.h

configexecincludedir = $(libdir)/glib-2.0/include
#configexecinclude_DATA = glibconfig.h

install-exec-local: glibconfig.h
	$(mkinstalldirs) $(DESTDIR)$(configexecincludedir)
	file=$(DESTDIR)$(configexecincludedir)/glibconfig.h; \
	if test -r $$file && cmp -s glibconfig.h $$file; then :; \
	else $(INSTALL_DATA) glibconfig.h $$file; fi

configure: configure.in acglib.m4		

BUILT_SOURCES = stamp-gc-h #note: not glibconfig.h
glibconfig.h: stamp-gc-h
	@if test -f glibconfig.h; then :; \
	else rm -f stamp-gc-h; $(MAKE) stamp-gc-h; fi
stamp-gc-h: config.status
	CONFIG_FILES= CONFIG_HEADERS= CONFIG_OTHER=glibconfig.h ./config.status
	echo timestamp > stamp-gc-h

libglib_1_3_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-export-dynamic

noinst_PROGRAMS = testglib testgdate testgdateparser
testglib_LDADD = libglib-1.3.la
testgdate_LDADD = libglib-1.3.la
testgdateparser_LDADD = libglib-1.3.la

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

dist-hook:
	cp glib.spec $(distdir)
