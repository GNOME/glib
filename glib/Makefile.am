## Process this file with automake to produce Makefile.in

configincludedir = $(pkglibdir)/include

bin_SCRIPTS=glib-config

EXTRA_DIST =	\
		glib.m4 \
		glib.spec

lib_LTLIBRARIES = libglib-1.1.la

libglib_1_1_la_SOURCES = \
		garray.c	\
		gcache.c	\
		gcompletion.c	\
		gdataset.c	\
		gerror.c	\
		ghash.c		\
		glist.c		\
		gmem.c		\
		gmessages.c	\
		gprimes.c	\
		gslist.c	\
		gtimer.c	\
		gtree.c		\
		grel.c		\
		gstring.c	\
		gstrfuncs.c	\
		gscanner.c	\
		gutils.c

include_HEADERS = \
		glib.h

configinclude_DATA = \
		glibconfig.h

libglib_1_1_la_LDFLAGS = -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

INCLUDES = -I$(top_srcdir)

noinst_PROGRAMS = testglib
testglib_LDADD = libglib-@LT_RELEASE@.la

m4datadir = $(datadir)/aclocal
m4data_DATA = glib.m4

.PHONY: files release

files:
	@files=`ls $(DISTFILES) 2> /dev/null `; for p in $$files; do \
	  echo $$p; \
	done

release:
	$(MAKE) dist distdir=$(PACKAGE)`date +"%y%m%d"`
