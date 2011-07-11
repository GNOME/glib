include $(top_srcdir)/Makefile.decl
NULL =

SUBDIRS = gdbus-object-manager-example

# The name of the module.
DOC_MODULE=gio

# The top-level SGML file.
DOC_MAIN_SGML_FILE=gio-docs.xml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=--deprecated-guards="G_DISABLE_DEPRECATED"

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=$(top_srcdir)/gio

BUILT_HFILES=gioenumtypes.h
HFILE_GLOB=$(filter-out $(addprefix %/, $(BUILT_HFILES)), $(addprefix $(top_srcdir)/gio/,$(shell cat $(top_builddir)/gio/gio-public-headers.txt))) $(addprefix $(top_builddir)/gio/,$(BUILT_HFILES))
CFILE_GLOB=$(top_srcdir)/gio/*.c

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
INCLUDES = \
	$(gio_INCLUDES)			\
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
	migrating-posix.xml	\
	migrating-gnome-vfs.xml	\
	migrating-gconf.xml	\
	migrating-gdbus.xml	\
	gio-querymodules.xml	\
	glib-compile-schemas.xml\
	gsettings.xml		\
	gdbus.xml		\
	gdbus-codegen.xml	\
	$(NULL)

expand_content_files =		\
	overview.xml		\
	migrating-posix.xml	\
	migrating-gnome-vfs.xml	\
	migrating-gconf.xml	\
	migrating-gdbus.xml	\
	gdbus-codegen.xml	\
	$(NULL)

extra_files =			\
	version.xml.in		\
	gvfs-overview.odg

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=--extra-dir=$(srcdir)/../glib/html --extra-dir=$(srcdir)/../gobject/html

include $(top_srcdir)/gtk-doc.make

EXTRA_DIST +=			\
	version.xml.in

man_MANS =			\
	gio-querymodules.1	\
	glib-compile-schemas.1	\
	gsettings.1		\
	gdbus.1			\
	gdbus-codegen.1

if ENABLE_MAN

.xml.1:
	@XSLTPROC@ -nonet http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

dist-local-check-mans-enabled:
	if grep "Man generation disabled" $(man_MANS) >/dev/null; then $(RM) $(man_MANS); fi

else

$(man_MANS):
	echo Man generation disabled.  Creating dummy $@.  Configure with --enable-man to enable it.
	echo Man generation disabled.  Remove this file, configure with --enable-man, and rebuild > $@

dist-local-check-mans-enabled:
	echo "*** --enable-man must be used in order to make dist"
	false

endif

MAINTAINERCLEANFILES = $(man_MANS) $(BUILT_SOURCES)

EXTRA_DIST += $(man_MANS)

dist-hook-local: dist-local-check-mans-enabled gio-docs-clean all-local

gio-docs-clean: clean
	cd $(srcdir) && rm -rf xml html
