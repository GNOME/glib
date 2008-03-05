## Process this file with automake to produce Makefile.in

include $(top_srcdir)/gcov.mak

INCLUDES = -DGIREPO_DEFAULT_SEARCH_PATH="\"$(libdir)\""

lib_LTLIBRARIES = libgirepository.la

libgirepository_la_SOURCES =			\
	girepository.c				\
	gmetadata.h				\
	gmetadata.c				\
	ginfo.c					\
	ginvoke.c
libgirepository_la_CPPFLAGS = $(GIREPO_CFLAGS)
libgirepository_la_LIBADD = $(GIREPO_LIBS)

girepodir = $(includedir)/glib-2.0/gobject-introspection
girepo_HEADERS = girepository.h

GCOVSOURCES = $(libgirepository_la_SOURCES)
