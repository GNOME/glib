include $(top_srcdir)/gcov.mak

GCOVSOURCES = $(libgirepository_la_SOURCES)

INCLUDES = -DGIREPO_DEFAULT_SEARCH_PATH="\"$(libdir)\""

girepodir = $(includedir)/gobject-introspection-1.0/
girepo_HEADERS = girepository.h

lib_LTLIBRARIES = libgirepository.la
noinst_LTLIBRARIES = libgirepository-parser.la

libgirepository_la_SOURCES =			\
	girepository.c				\
	gtypelib.h				\
	gtypelib.c				\
	ginfo.c					\
	ginvoke.c
libgirepository_la_CPPFLAGS = $(GIREPO_CFLAGS)
libgirepository_la_LIBADD = $(GIREPO_LIBS)

libgirepository_parser_la_SOURCES =		\
	girmodule.c				\
	girmodule.h				\
	girnode.c				\
	girnode.h				\
	girparser.c				\
	girparser.h
libgirepository_parser_la_CFLAGS = $(GIREPO_CFLAGS)

