include $(top_srcdir)/Makefile.decl

NULL =

SUBDIRS= 

if OS_UNIX
SUBDIRS += xdgmime
endif

gio.def: gio.symbols
	(echo -e EXPORTS; $(CPP) -P -DINCLUDE_VARIABLES -DINCLUDE_INTERNAL_SYMBOLS -DG_OS_WIN32 -DALL_FILES - <$(srcdir)/gio.symbols | sed -e '/^$$/d' -e 's/^/	/' -e 's/G_GNUC_[^ ]*//g' | sort) > gio.def

gioalias.h: gio.symbols
	$(PERL) $(srcdir)/makegioalias.pl < $(srcdir)/gio.symbols > gioalias.h

gioaliasdef.c: gio.symbols
	$(PERL) $(srcdir)/makegioalias.pl -def < $(srcdir)/gio.symbols > gioaliasdef.c

if OS_LINUX
if HAVE_GNUC_VISIBILITY
TESTS = abicheck.sh pltcheck.sh
endif
endif

AM_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"GLib-GIO\"			\
	-I$(top_builddir)				\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/glib				\
	-I$(top_srcdir)/gmodule				\
	$(GLIB_DEBUG_FLAGS)				\
	-DG_DISABLE_DEPRECATED				\
	-DGIO_MODULE_DIR=\"$(GIO_MODULE_DIR)\"	

lib_LTLIBRARIES = libgio-2.0.la

marshal_sources = \
        gio-marshal.h	\
        gio-marshal.c	\
	$(NULL)

if CROSS_COMPILING
  glib_genmarshal=$(GLIB_GENMARSHAL)
else
  glib_genmarshal=../gobject/glib-genmarshal
endif

gio-marshal.h: gio-marshal.list
	$(glib_genmarshal) --prefix=_gio_marshal $(srcdir)/gio-marshal.list --header > $@

gio-marshal.c: gio-marshal.h gio-marshal.list
	(echo "#include \"gio-marshal.h\""; \
	$(glib_genmarshal) --prefix=_gio_marshal $(srcdir)/gio-marshal.list --body) > $@

local_sources = \
	glocaldirectorymonitor.c 	\
	glocaldirectorymonitor.h 	\
	glocalfile.c 			\
	glocalfile.h 			\
	glocalfileenumerator.c 		\
	glocalfileenumerator.h 		\
	glocalfileinfo.c 		\
	glocalfileinfo.h 		\
	glocalfileinputstream.c 	\
	glocalfileinputstream.h 	\
	glocalfilemonitor.c 		\
	glocalfilemonitor.h 		\
	glocalfileoutputstream.c 	\
	glocalfileoutputstream.h 	\
	glocalvfs.c 			\
	glocalvfs.h 			\
	$(NULL)

platform_libadd =
appinfo_sources =

if HAVE_INOTIFY
SUBDIRS += inotify
platform_libadd += inotify/libinotify.la
endif

SUBDIRS += .

if HAVE_FAM
SUBDIRS += fam
endif

if OS_UNIX
appinfo_sources += gdesktopappinfo.c gdesktopappinfo.h
platform_libadd += xdgmime/libxdgmime.la
unix_sources = \
	gunixmount.c		\
	gunixmount.h		\
	gunixmounts.c 		\
	gunixmounts.h 		\
	gunixvolume.c 		\
	gunixvolume.h 		\
	gunixvolumemonitor.c 	\
	gunixvolumemonitor.h 	\
	gunixinputstream.c 	\
	gunixoutputstream.c 	\
	$(NULL)


giounixincludedir=$(includedir)/gio-unix-2.0/gio
giounixinclude_HEADERS = \
	gdesktopappinfo.h	\
	gunixmounts.h 		\
	gunixinputstream.h 	\
	gunixoutputstream.h 	\
	$(NULL)
endif

if OS_WIN32
appinfo_sources += gwin32appinfo.c gwin32appinfo.h
platform_libadd += -lshlwapi
endif

libgio_2_0_la_SOURCES =		\
	gappinfo.c 		\
	gasynchelper.c 		\
	gasynchelper.h 		\
	gasyncresult.c 		\
	gbufferedinputstream.c 	\
	gbufferedoutputstream.c \
	gcancellable.c 		\
	gcontenttype.c 		\
	gcontenttypeprivate.h 	\
	gdatainputstream.c 	\
	gdataoutputstream.c 	\
	gdirectorymonitor.c 	\
	gdrive.c 		\
	gdummyfile.h 		\
	gdummyfile.c 		\
	gfile.c 		\
	gfileattribute.c 	\
	gfileenumerator.c 	\
	gfileicon.c 		\
	gfileinfo.c 		\
	gfileinputstream.c 	\
	gfilemonitor.c 		\
	gfilenamecompleter.c 	\
	gfileoutputstream.c 	\
	gfilterinputstream.c 	\
	gfilteroutputstream.c 	\
	gicon.c 		\
	ginputstream.c 		\
	gioerror.c 		\
	giomodule.c 		\
	giomodule-priv.h	\
	gioscheduler.c 		\
	gloadableicon.c 	\
	gmount.c 		\
	gmemoryinputstream.c 	\
	gmemoryoutputstream.c 	\
	gmountoperation.c 	\
	gnativevolumemonitor.c 	\
	gnativevolumemonitor.h 	\
	goutputstream.c 	\
	gpollfilemonitor.c 	\
	gpollfilemonitor.h 	\
	gseekable.c 		\
	gsimpleasyncresult.c 	\
	gthemedicon.c 		\
	gunionvolumemonitor.c 	\
	gunionvolumemonitor.h 	\
	gvfs.c 			\
	gvolume.c 		\
	gvolumemonitor.c 	\
	gmountprivate.h 	\
	gioenumtypes.h		\
	gioenumtypes.c		\
	$(appinfo_sources) 	\
	$(unix_sources) 	\
	$(local_sources) 	\
	$(marshal_sources) 	\
	$(NULL)

$(libgio_2_0_la_OBJECTS): $(marshal_sources)

libgio_2_0_la_LIBADD = \
	$(top_builddir)/glib/libglib-2.0.la 		\
	$(top_builddir)/gobject/libgobject-2.0.la 	\
	$(top_builddir)/gmodule/libgmodule-2.0.la 	\
	$(platform_libadd) 				\
	$(SELINUX_LIBS) 				\
	$(GLIB_LIBS) 					\
	$(XATTR_LIBS) 					\
	$(NULL)

if OS_WIN32
no_undefined = -no-undefined
endif

libgio_2_0_la_LDFLAGS= -export-dynamic $(no_undefined) -export-symbols-regex '^g_.*'

gio_headers =			\
	gappinfo.h 		\
	gasyncresult.h 		\
	gbufferedinputstream.h 	\
	gbufferedoutputstream.h \
	gcancellable.h 		\
	gcontenttype.h 		\
	gdatainputstream.h 	\
	gdataoutputstream.h 	\
	gdirectorymonitor.h 	\
	gdrive.h 		\
	gfile.h 		\
	gfileattribute.h 	\
	gfileenumerator.h 	\
	gfileicon.h 		\
	gfileinfo.h 		\
	gfileinputstream.h 	\
	gfilemonitor.h 		\
	gfilenamecompleter.h 	\
	gfileoutputstream.h 	\
	gfilterinputstream.h 	\
	gfilteroutputstream.h 	\
	gicon.h 		\
	ginputstream.h 		\
	gio.h			\
	gioerror.h 		\
	giomodule.h 		\
	gioscheduler.h 		\
	gloadableicon.h 	\
	gmount.h 		\
	gmemoryinputstream.h 	\
	gmemoryoutputstream.h 	\
	gmountoperation.h 	\
	goutputstream.h 	\
	gseekable.h 		\
	gsimpleasyncresult.h 	\
	gthemedicon.h 		\
	gvfs.h 			\
	gvolume.h 		\
	gvolumemonitor.h 	\
	$(NULL)

gioincludedir=$(includedir)/glib-2.0/gio/
gioinclude_HEADERS = 		\
	$(gio_headers)		\
	gioenumtypes.h

# these sources (also mentioned above) are generated.
BUILT_SOURCES = 		\
	gio-marshal.h 		\
	gio-marshal.c 		\
	gioalias.h 		\
	gioaliasdef.c		\
	gioenumtypes.h		\
	gioenumtypes.c		\
	$(NULL)

EXTRA_DIST += 			\
	gio-marshal.list	\
	gio.symbols		\
	gioenumtypes.h.template	\
	gioenumtypes.c.template	\
	$(NULL)

CLEANFILES = 			\
	$(marshal_sources)	\
	$(NULL)

gioenumtypes.h: $(gio_headers) gioenumtypes.h.template
	( cd $(srcdir) && $(top_builddir)/gobject/glib-mkenums --template gioenumtypes.h.template $(gio_headers) ) > gioenumtypes.h
	
gioenumtypes.c: $(gio_headers) gioenumtypes.c.template
	( cd $(srcdir) && $(top_builddir)/gobject/glib-mkenums --template gioenumtypes.c.template $(gio_headers) ) > gioenumtypes.c
	
