## Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/gthread \
	-DG_LOG_DOMAIN=\"GThread\" @GTHREAD_COMPILE_IMPL_DEFINES@

EXTRA_DIST =				\
		makefile.mingw		\
		makefile.mingw.in	\
		gthread-posix.c		\
		gthread-solaris.c	\
		gthread-none.c		\
		gthread.def		\
		gthread.rc		\
		gthread.rc.in

libglib = $(top_builddir)/libglib.la # -lglib

top_builddir_full=`cd \$(top_builddir); pwd`

lib_LTLIBRARIES = libgthread-1.3.la

libgthread_1_3_la_SOURCES = gthread-impl.c
libgthread_1_3_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-export-dynamic

libgthread_1_3_la_LIBADD = @G_THREAD_LIBS_EXTRA@ @G_THREAD_LIBS@

gthread.rc: $(top_builddir)/config.status $(top_srcdir)/gthread/gthread.rc.in
	cd $(top_builddir) && CONFIG_FILES=gthread/$@ CONFIG_HEADERS= $(SHELL) ./config.status

makefile.mingw: $(top_builddir)/config.status $(top_srcdir)/gthread/makefile.mingw.in
	cd $(top_builddir) && CONFIG_FILES=gthread/$@ CONFIG_HEADERS= $(SHELL) ./config.status
