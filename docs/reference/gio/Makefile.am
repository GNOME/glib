include $(top_srcdir)/Makefile.decl
NULL =

# The name of the module.
DOC_MODULE=gio

# The top-level SGML file.
DOC_MAIN_SGML_FILE=gio-docs.xml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=--deprecated-guards="G_DISABLE_DEPRECATED"

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=$(top_srcdir)/gio

HFILE_GLOB=$(top_srcdir)/gio/*.h
CFILE_GLOB=$(top_srcdir)/gio/*.c

# Headers to ignore
IGNORE_HFILES=				\
	asyncns.h			\
	fam-helper.h			\
	fen-data.h			\
	fen-helper.h			\
	fen-kernel.h			\
	fen-node.h			\
	gasynchelper.h			\
	gcontenttypeprivate.h		\
	gdummyfile.h			\
	gfamdirectorymonitor.h		\
	gfamfilemonitor.h		\
	gfileattribute-priv.h		\
	gfileinfo-priv.h		\
	ginotifydirectorymonitor.h	\
	ginotifyfilemonitor.h		\
	gio-marshal.h			\
	gio.h				\
	gioalias.h			\
	gioalias.h			\
	gioenumtypes.h			\
	giomodule-priv.h		\
	glocaldirectorymonitor.h	\
	glocalfile.h			\
	glocalfileenumerator.h		\
	glocalfileinfo.h		\
	glocalfileinputstream.h		\
	glocalfilemonitor.h		\
	glocalfileoutputstream.h	\
	glocalvfs.h			\
	gnativevolumemonitor.h		\
	gnetworkingprivate.h		\
	gpollfilemonitor.h		\
	gthreadedresolver.h		\
	gunionvolumemonitor.h		\
	gunixdrive.h			\
	gunixresolver.h			\
	gunixvolume.h			\
	gvolumeprivate.h		\
	gwin32appinfo.h			\
	gwin32resolver.h		\
	inotify-diag.h			\
	inotify-helper.h		\
	inotify-kernel.h		\
	inotify-missing.h		\
	inotify-path.h			\
	inotify-sub.h			\
	local_inotify.h			\
	local_inotify_syscalls.h	\
	xdgmime.h			\
	xdgmimealias.h			\
	xdgmimecache.h			\
	xdgmimeglob.h			\
	xdgmimeint.h			\
	xdgmimemagic.h			\
	xdgmimeparent.h			\
	xdgmimeicon.h			\
	winhttp.h			\
	$(NULL)

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
INCLUDES = \
	-I$(srcdir)			\
	-I$(top_srcdir)			\
	-I$(top_srcdir)/glib		\
	-I$(top_srcdir)/gobject		\
	-I$(top_srcdir)/gio		\
	-I$(top_builddir)		\
	-I$(top_builddir)/glib		\
	-I$(top_builddir)/gobject	\
	-I$(top_builddir)/gio		\
	$(GLIB_DEBUG_FLAGS)

GTKDOC_LIBS = \
	$(top_builddir)/glib/libglib-2.0.la		\
	$(top_builddir)/gobject/libgobject-2.0.la	\
	$(top_builddir)/gmodule/libgmodule-2.0.la	\
	$(top_builddir)/gio/libgio-2.0.la		\
	$(NULL)

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS = --output-format=xml --sgml-mode --name-space=g \
	--ignore-files=libasyncns

# Images to copy into HTML directory
HTML_IMAGES =			\
	gvfs-overview.png

content_files =			\
	version.xml		\
	overview.xml		\
	migrating.xml

expand_content_files =		\
	overview.xml		\
	migrating.xml

extra_files =			\
	version.xml.in		\
	gvfs-overview.odg

include $(top_srcdir)/gtk-doc.make

EXTRA_DIST +=			\
	version.xml.in
