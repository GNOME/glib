## Process this file with automake to produce Makefile.in

INCLUDES = -DGIREPO_DEFAULT_SEARCH_PATH="\"$(libdir)\""

lib_LTLIBRARIES = libgirepository.la

libgirepository_la_SOURCES =			\
	girepository.c				\
	gmetadata.h				\
	gmetadata.c				\
	ginfo.c					\
	ginvoke.c
libgirepository_la_CFLAGS = $(GIREPO_CFLAGS)

girepodir = $(includedir)/glib-2.0/gobject-introspection
girepo_HEADERS = girepository.h
