# GObject - GLib Type, Object, Parameter and Signal Library
# Copyright (C) 1997,98,99,2000 Tim Janik and Red Hat, Inc.
#
## Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir) -I$(top_builddir) @GLIB_DEBUG_FLAGS@

# libraries to compile and install
lib_LTLIBRARIES = libgobject-1.3.la

# provide g_logv() domain
AM_CFLAGS = -DG_LOG_DOMAIN=g_log_domain_gobject

# libtool stuff: set version and export symbols for resolving
libgobjectincludedir = $(includedir)/glib-2.0/gobject
libgobject_1_3_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-export-dynamic
libgobject_1_3_la_LIBADD = # $(libglib)

#
# setup source file variables
#
# GObject header files for public installation (non-generated)
gobject_public_h_sources = @STRIP_BEGIN@ \
	gboxed.h \
	gbsearcharray.h \
	gclosure.h \
	genums.h \
	gobject.h \
	gparam.h \
	gparamspecs.h \
	gsignal.h \
	gtype.h \
	gvalue.h \
	gvaluecollector.h \
	gvaluetypes.h \
@STRIP_END@

# private GObject header files
gobject_private_h_sources = 

# GObject C sources to build the library from
gobject_c_sources = @STRIP_BEGIN@ \
	gboxed.c \
	gbsearcharray.c \
	gclosure.c \
	genums.c \
	gobject.c \
	gparam.c \
	gparamspecs.c \
	gsignal.c \
	gtype.c \
	gvalue.c \
	gvaluetypes.c \
@STRIP_END@

# non-header sources (headers should be specified in the above variables)
# that don't serve as direct make target sources, i.e. they don't have
# their own .lo rules and don't get publically installed
gobject_extra_sources = 

#
# setup GObject sources and their dependancies
#
gobject_h_sources = $(gobject_private_h_sources) $(gobject_public_h_sources) # $(gobject_built_public_sources)
libgobjectinclude_HEADERS = $(gobject_public_h_sources) # $(gobject_built_public_sources)
libgobject_1_3_la_SOURCES = $(gobject_c_sources)
MAINTAINERCLEANFILES += 
# $(gobject_built_public_sources) $(gobject_built_sources)
EXTRA_HEADERS +=
EXTRA_DIST += $(gobject_private_h_sources)
EXTRA_DIST += $(gobject_extra_sources) 
# $(gobject_built_sources) $(gobject_built_public_sources)

#
# programs to compile and install
#
bin_PROGRAMS = gobject-query glib-genmarshal
# source files
gobject_query_SOURCES = gobject-query.c
glib_genmarshal_SOURCES = glib-genmarshal.c
# link programs against libgobject
progs_LDADD = ../libglib-1.3.la libgobject-1.3.la
gobject_query_LDADD = $(progs_LDADD)
glib_genmarshal_LDADD = $(progs_LDADD)

#
# manual pages to install
#
man_MANS = glib-genmarshal.1

#
# auxillary files
#
EXTRA_DIST += 			\
	makefile.mingw.in 	\
	makefile.msc.in 	\
	gobject.def		\
	gobject.rc.in

BUILT_EXTRA_DIST = \
	makefile.mingw		\
	makefile.msc		\
	gobject.rc

gobject.rc: $(top_builddir)/config.status $(top_srcdir)/gobject/gobject.rc.in
	cd $(top_builddir) && CONFIG_FILES=gobject/$@ CONFIG_HEADERS= $(SHELL) ./config.status

makefile.mingw: $(top_builddir)/config.status $(top_srcdir)/gobject/makefile.mingw.in
	cd $(top_builddir) && CONFIG_FILES=gobject/$@ CONFIG_HEADERS= $(SHELL) ./config.status

makefile.msc: $(top_builddir)/config.status $(top_srcdir)/gobject/makefile.msc.in
	cd $(top_builddir) && CONFIG_FILES=gobject/$@ CONFIG_HEADERS= $(SHELL) ./config.status

dist-hook: $(BUILT_EXTRA_DIST)
	files='$(BUILT_EXTRA_DIST)'; \
	for f in $$files; do \
	  if test -f $$f; then d=.; else d=$(srcdir); fi; \
	  cp $$d/$$f $(distdir) || exit 1; done

