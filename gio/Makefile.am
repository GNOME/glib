include $(top_srcdir)/Makefile.decl

NULL =

SUBDIRS=

if OS_UNIX
SUBDIRS += xdgmime
endif

if OS_WIN32_AND_DLL_COMPILATION
if MS_LIB_AVAILABLE
noinst_DATA = gio-2.0.lib

install_ms_lib_cmd = $(INSTALL) gio-2.0.lib $(DESTDIR)$(libdir)
uninstall_ms_lib_cmd = -rm $(DESTDIR)$(libdir)/gio-2.0.lib
endif
endif

install-ms-lib:
	$(install_ms_lib_cmd)

uninstall-ms-lib:
	$(uninstall_ms_lib_cmd)

gio.def: gio.symbols
	(echo -e EXPORTS; $(CPP) -P -DINCLUDE_VARIABLES -DINCLUDE_INTERNAL_SYMBOLS -DG_OS_WIN32 -DALL_FILES - <$(srcdir)/gio.symbols | sed -e '/^$$/d' -e 's/^/	/' -e 's/G_GNUC_[^ ]*//g' | sort) > gio.def.tmp && \
	  mv gio.def.tmp gio.def

gioalias.h: gio.symbols
	$(PERL) $(srcdir)/makegioalias.pl < $(srcdir)/gio.symbols > gioalias.h.tmp && \
	  mv gioalias.h.tmp gioalias.h

gioaliasdef.c: gio.symbols
	$(PERL) $(srcdir)/makegioalias.pl -def < $(srcdir)/gio.symbols > gioaliasdef.c.tmp && \
	  mv gioaliasdef.c.tmp gioaliasdef.c

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
	-DGIO_COMPILATION				\
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
	$(glib_genmarshal) --prefix=_gio_marshal $(srcdir)/gio-marshal.list --header --internal > $@.tmp && \
	  mv $@.tmp $@

gio-marshal.c: gio-marshal.h gio-marshal.list
	(echo "#include \"gio-marshal.h\""; \
	$(glib_genmarshal) --prefix=_gio_marshal $(srcdir)/gio-marshal.list --body --internal) > $@.tmp && \
	  mv $@.tmp $@

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
platform_deps =
appinfo_sources =

if HAVE_INOTIFY
SUBDIRS += inotify
platform_libadd += inotify/libinotify.la
platform_deps += inotify/libinotify.la
endif

if HAVE_FEN
AM_CPPFLAGS += -DHAVE_FEN
SUBDIRS += fen
platform_libadd += fen/libfen.la
platform_deps += fen/libfen.la
endif

if OS_WIN32
SUBDIRS += win32
platform_libadd += win32/libgiowin32.la
platform_deps += win32/libgiowin32.la
endif


SUBDIRS += .

if HAVE_FAM
SUBDIRS += fam
endif

if OS_UNIX
appinfo_sources += gdesktopappinfo.c gdesktopappinfo.h
platform_libadd += xdgmime/libxdgmime.la
platform_deps += xdgmime/libxdgmime.la
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
win32_sources = \
	gwin32mount.c \
	gwin32mount.h \
	gwin32volumemonitor.c \
	gwin32volumemonitor.h \
	$(NULL)

endif

SUBDIRS += tests

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
	gdrive.c 		\
	gdummyfile.h 		\
	gdummyfile.c 		\
	gemblem.h 		\
	gemblem.c 		\
	gemblemedicon.h		\
	gemblemedicon.c		\
	gfile.c 		\
	gfileattribute.c 	\
	gfileattribute-priv.h 	\
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
	gioenums.h		\
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
	gioalias.h 		\
	gioaliasdef.c		\
	$(appinfo_sources) 	\
	$(unix_sources) 	\
	$(win32_sources) 	\
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

if PLATFORM_WIN32
no_undefined = -no-undefined
endif

if OS_WIN32_AND_DLL_COMPILATION
export_symbols = -export-symbols gio.def
gio_def = gio.def

install-def-file:
	$(INSTALL) gio.def $(DESTDIR)$(libdir)/gio-2.0.def

uninstall-def-file:
	-rm $(DESTDIR)$(libdir)/gio-2.0.def
else
install-def-file:
uninstall-def-file:

export_symbols = -export-symbols-regex '^g_.*'
endif

install-data-local: install-ms-lib install-def-file
	$(mkinstalldirs) $(DESTDIR)$(GIO_MODULE_DIR)

uninstall-local: uninstall-ms-lib uninstall-def-file

libgio_2_0_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-export-dynamic $(no_undefined) $(export_symbols)

libgio_2_0_la_DEPENDENCIES = $(gio_def) $(platform_deps)

gio_headers =			\
	gappinfo.h 		\
	gasyncresult.h 		\
	gbufferedinputstream.h 	\
	gbufferedoutputstream.h \
	gcancellable.h 		\
	gcontenttype.h 		\
	gdatainputstream.h 	\
	gdataoutputstream.h 	\
	gdrive.h 		\
	gemblem.h 		\
	gemblemedicon.h		\
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
	giotypes.h		\
	gioenums.h		\
	gioerror.h 		\
	giomodule.h 		\
	gioscheduler.h 		\
	gloadableicon.h 	\
	gmount.h 		\
	gmemoryinputstream.h 	\
	gmemoryoutputstream.h 	\
	gmountoperation.h 	\
	gnativevolumemonitor.h 	\
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
	makefile.msc		\
	makegioalias.pl		\
	abicheck.sh		\
	pltcheck.sh		\
	$(NULL)

CLEANFILES = 			\
	$(marshal_sources)	\
	$(NULL)

gioenumtypes.h: $(gio_headers) gioenumtypes.h.template
	( top_builddir=`cd $(top_builddir) && pwd`; \
	  cd $(srcdir) && $$top_builddir/gobject/glib-mkenums --template gioenumtypes.h.template $(gio_headers) ) > \
	    gioenumtypes.h.tmp && mv gioenumtypes.h.tmp gioenumtypes.h

gioenumtypes.c: $(gio_headers) gioenumtypes.c.template
	( top_builddir=`cd $(top_builddir) && pwd`; \
	  cd $(srcdir) && $$top_builddir/gobject/glib-mkenums --template gioenumtypes.c.template $(gio_headers) ) > \
	    gioenumtypes.c.tmp && mv gioenumtypes.c.tmp gioenumtypes.c

gio-2.0.lib: libgio-2.0.la gio.def
	lib -machine:@LIB_EXE_MACHINE_FLAG@ -name:libgio-2.0-$(LT_CURRENT_MINUS_AGE).dll -def:gio.def -out:$@
