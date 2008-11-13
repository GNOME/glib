include $(top_srcdir)/gcov.mak

GCOVSOURCES = $(libgirepository_la_SOURCES)

girepodir = $(includedir)/gobject-introspection-1.0/
girepo_HEADERS = girepository.h

lib_LTLIBRARIES = libgirepository.la
noinst_LTLIBRARIES = libgirepository-parser.la

libgirepository_la_SOURCES =			\
	girepository.c				\
	gtypelib.h				\
	gtypelib.c				\
	ginfo.c					\
	girffi.c				\
	girffi.h				\
	gdump.c					\
	ginvoke.c

libgirepository_la_CPPFLAGS = $(GIREPO_CFLAGS)
libgirepository_la_LIBADD = $(GIREPO_LIBS)
libgirepository_la_LDFLAGS = -no-undefined

libgirepository_parser_la_SOURCES =		\
	girmodule.c				\
	girmodule.h				\
	girnode.c				\
	girnode.h				\
	giroffsets.c				\
	girparser.c				\
	girparser.h
libgirepository_parser_la_CFLAGS = $(GIREPO_CFLAGS)

