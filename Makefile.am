girepodir = $(includedir)/gobject-introspection-1.0/
girepo_HEADERS =				\
    giarginfo.h					\
    gibaseinfo.h				\
    gicallableinfo.h				\
    giconstantinfo.h				\
    gienuminfo.h				\
    gierrordomaininfo.h				\
    gifieldinfo.h				\
    gifunctioninfo.h				\
    giinterfaceinfo.h				\
    giobjectinfo.h				\
    gipropertyinfo.h				\
    giregisteredtypeinfo.h			\
    girepository.h				\
    girffi.h					\
    gisignalinfo.h				\
    gistructinfo.h				\
    gitypeinfo.h				\
    gitypelib.h					\
    gitypes.h					\
    giunioninfo.h				\
    givfuncinfo.h

lib_LTLIBRARIES = libgirepository-1.0.la
noinst_LTLIBRARIES = libgirepository-parser.la

libgirepository_1_0_la_SOURCES =		\
	gdump.c					\
	giarginfo.c				\
	gibaseinfo.c				\
	gicallableinfo.c			\
	giconstantinfo.c			\
	gienuminfo.c				\
	gierrordomaininfo.c			\
	gifieldinfo.c				\
	gifunctioninfo.c			\
	ginvoke.c				\
	giinterfaceinfo.c			\
	giobjectinfo.c				\
	gipropertyinfo.c			\
	giregisteredtypeinfo.c			\
	girepository.c				\
	girepository-private.h			\
	girffi.c                                \
	girffi.h                                \
	girffi-private.h                        \
	gisignalinfo.c				\
	gistructinfo.c				\
	gitypeinfo.c				\
	gitypelib.c				\
	gitypelib-internal.h			\
	glib-compat.h				\
	giunioninfo.c				\
	givfuncinfo.c

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

