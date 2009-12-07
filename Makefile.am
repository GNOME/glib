include $(top_srcdir)/gcov.mak

GCOVSOURCES = $(libgirepository_1_0_la_SOURCES)

girepodir = $(includedir)/gobject-introspection-1.0/
girepo_HEADERS = girepository.h girffi.h

lib_LTLIBRARIES = libgirepository-1.0.la
noinst_LTLIBRARIES = libgirepository-parser.la

libgirepository_1_0_la_SOURCES =		\
	gdump.c					\
	gfield.c				\
	ginfo.c                                 \
	ginfo.h                                 \
	ginvoke.c				\
	girepository.c				\
	girffi.c                                \
	girffi.h                                \
	girffi-private.h                        \
	glib-compat.h                           \
	gtypelib.c				\
	gtypelib.h

libgirepository_1_0_la_CPPFLAGS = $(GIREPO_CFLAGS)
libgirepository_1_0_la_LIBADD = $(GIREPO_LIBS)
libgirepository_1_0_la_LDFLAGS = -no-undefined

libgirepository_parser_la_SOURCES =		\
	girmodule.c				\
	girmodule.h				\
	girnode.c				\
	girnode.h				\
	giroffsets.c				\
	girparser.c				\
	girparser.h
libgirepository_parser_la_CFLAGS = $(GIREPO_CFLAGS)

