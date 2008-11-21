## Process this file with automake to produce Makefile.in
include $(top_srcdir)/Makefile.decl

AUTOMAKE_OPTIONS = 1.6

# The name of the module.
DOC_MODULE=gobject

# The top-level SGML file.
DOC_MAIN_SGML_FILE=gobject-docs.sgml

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=../../../gobject

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=--deprecated-guards="G_DISABLE_DEPRECATED" \
	--ignore-decorators=G_GNUC_INTERNAL

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--sgml-mode --output-format=xml --name-space=g

# Used for dependencies
HFILE_GLOB=$(top_srcdir)/gobject/*.h
CFILE_GLOB=$(top_srcdir)/gobject/*.c

# Headers to ignore
IGNORE_HFILES=gobjectalias.h

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
INCLUDES = \
	-I$(srcdir) 			\
	-I$(top_srcdir) 		\
	-I$(top_srcdir)/glib 		\
	-I$(top_builddir) 		\
	-I$(top_builddir)/glib 		\
	$(GLIB_DEBUG_FLAGS)
GTKDOC_LIBS = \
	$(top_builddir)/glib/libglib-2.0.la	\
	$(top_builddir)/gobject/libgobject-2.0.la

# Images to copy into HTML directory
HTML_IMAGES =  \
	$(srcdir)/images/glue.png

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE)
content_files = version.xml 		\
		glib-mkenums.xml 	\
		glib-genmarshal.xml	\
		gobject-query.xml \
		tut_gobject.xml \
		tut_gsignal.xml \
		tut_gtype.xml \
		tut_howto.xml \
		tut_intro.xml \
		tut_tools.xml

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=--extra-dir=$(srcdir)/../glib/html

include $(top_srcdir)/gtk-doc.make

# Other files to distribute
EXTRA_DIST += 		\
	gobject.cI	\
	version.xml.in

########################################################################

man_MANS = glib-mkenums.1 glib-genmarshal.1 gobject-query.1

if ENABLE_MAN

%.1 : %.xml 
	@XSLTPROC@ -nonet http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

endif

BUILT_EXTRA_DIST = $(man_MANS)

dist-hook-local: $(BUILT_EXTRA_DIST)
	files='$(BUILT_EXTRA_DIST)';				\
	for f in $$files; do					\
	  if test -f $$f; then d=.; else d=$(srcdir); fi;	\
	  cp $$d/$$f $(distdir) || exit 1; done
