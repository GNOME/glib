# GObject - GLib Type, Object, Parameter and Signal Library
# Copyright (C) 1997,98,99,2000 Tim Janik and Red Hat, Inc.
#
## Process this file with automake to produce Makefile.in

SUBDIRS =

INCLUDES = -I$(top_srcdir) @GLIB_DEBUG_FLAGS@

# libraries to compile and install
lib_LTLIBRARIES = libgobject.la

# provide g_logv() domain
DEFS += -DG_LOG_DOMAIN=g_log_domain_gobject

# libtool stuff: set version and export symbols for resolving
libgobjectincludedir = $(includedir)/gobject
libgobject_la_LDFLAGS = @STRIP_BEGIN@ \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-release $(LT_RELEASE) \
	-export-dynamic \
@STRIP_END@
libgobject_la_LIBADD = # $(libglib)

#
# setup source file variables
#
# GObject header files for public installation (non-generated)
gobject_public_h_sources = @STRIP_BEGIN@ \
	gvalue.h \
	gparam.h \
	gparamspecs.h \
	genums.h \
	gobject.h \
	gtype.h \
	gvaluecollector.h \
@STRIP_END@
# private GObject header files
gobject_private_h_sources = @STRIP_BEGIN@ \
@STRIP_END@

# GObject C sources to build the library from
gobject_c_sources = @STRIP_BEGIN@ \
	gvalue.c \
	gparam.c \
	gparamspecs.c \
	genums.c \
	gobject.c \
	gtype.c \
@STRIP_END@

# non-header sources (headers should be specified in the above variables)
# that don't serve as direct make target sources, i.e. they don't have
# their own .lo rules and don't get publically installed
gobject_extra_sources = @STRIP_BEGIN@ \
@STRIP_END@

#
# setup GObject sources and their dependancies
#
gobject_h_sources = $(gobject_private_h_sources) $(gobject_public_h_sources) # $(gobject_built_public_sources)
libgobjectinclude_HEADERS = $(gobject_public_h_sources) # $(gobject_built_public_sources)
libgobject_la_SOURCES = $(gobject_c_sources)
MAINTAINERCLEANFILES += # $(gobject_built_public_sources) $(gobject_built_sources)
EXTRA_HEADERS +=
EXTRA_DIST += $(gobject_private_h_sources)
EXTRA_DIST += $(gobject_extra_sources) # $(gobject_built_sources) $(gobject_built_public_sources)

#
# programs to compile and install
#
bin_PROGRAMS = gobject-query
# source files
gobject_query_SOURCES = gobject-query.c
# link programs against libgobject
progs_LDADD = ../libglib.la libgobject.la
gobject_query_LDADD = $(progs_LDADD)

#
# auxillary files
#
EXTRA_DIST += \
	TODO
