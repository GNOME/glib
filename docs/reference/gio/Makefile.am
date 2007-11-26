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
IGNORE_HFILES= 		           \
	fam-helper.h			\
	gasynchelper.h			\
	gdesktopappinfo.h		\
	gdummyfile.h			\
	gfamdirectorymonitor.h		\
	gfamfilemonitor.h		\
	ginotifydirectorymonitor.h	\
	ginotifyfilemonitor.h		\
	glocaldirectorymonitor.h	\
	glocalfile.h			\
	glocalfileenumerator.h		\
	glocalfileinfo.h		\
	glocalfileinputstream.h		\
	glocalfilemonitor.h		\
	glocalfileoutputstream.h	\
	glocalvfs.h			\
	gnativevolumemonitor.h		\
	gpollfilemonitor.h		\
	gunionvolumemonitor.h		\
	gunixdrive.h			\
	gunixvolume.h			\
	gunixvolumemonitor.h		\
	gvolumeprivate.h		\
	gwin32appinfo.h			\
	inotify-kernel.h		\
	local_inotify.h			\
	local_inotify_syscalls.h	\
	xdgmime.h			\
	xdgmimealias.h		   	\
	xdgmimecache.h		   	\
	xdgmimeglob.h		   	\
	xdgmimeint.h		   	\
	xdgmimemagic.h		   	\
	xdgmimeparent.h		   	\
	$(NULL)

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
INCLUDES = \
	-I$(srcdir) 			\
	-I$(top_srcdir) 		\
	-I$(top_srcdir)/glib 		\
	-I$(top_srcdir)/gobject		\
	-I$(top_builddir) 		\
	-I$(top_builddir)/glib 		\
	-I$(top_builddir)/gobject	\
	$(GLIB_DEBUG_FLAGS)

GTKDOC_LIBS = \
	$(top_builddir)/glib/libglib-2.0.la		\
	$(top_builddir)/gobject/libgobject-2.0.la	\
	$(top_builddir)/gmodule/libgmodule-2.0.la	\
	$(top_builddir)/gio/libgio-2.0.la		\
	$(NULL)

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS = --output-format=xml --sgml-mode

# Images to copy into HTML directory
HTML_IMAGES =

content_files = 		\
	version.xml

extra_files = version.xml.in

include $(top_srcdir)/gtk-doc.make

EXTRA_DIST +=				\
	version.xml.in
