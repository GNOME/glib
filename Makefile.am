## Process this file with automake to produce Makefile.in

include $(top_srcdir)/gcov.mak

INCLUDES = -DGIREPO_DEFAULT_SEARCH_PATH="\"$(libdir)\""

noinst_LTLIBRARIES = libgirepository.la

libgirepository_la_SOURCES =			\
	girepository.c				\
	gtypelib.h				\
	gtypelib.c				\
	ginfo.c					\
	ginvoke.c
libgirepository_la_CPPFLAGS = $(GIREPO_CFLAGS)
libgirepository_la_LIBADD = $(GIREPO_LIBS)

#girepodir = $(includedir)/gobject-introspection-1.0/
#girepo_HEADERS = girepository.h
noinst_HEADERS = girepository.h

GCOVSOURCES = $(libgirepository_la_SOURCES)
