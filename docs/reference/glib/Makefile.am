## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS = 1.6

# The name of the module.
DOC_MODULE=glib

# The top-level SGML file.
DOC_MAIN_SGML_FILE=glib-docs.sgml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=--deprecated-guards="G_DISABLE_DEPRECATED"

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=../../..

# Used for dependencies
HFILE_GLOB=$(top_srcdir)/glib/*.h $(top_srcdir)/gmodule/*.h
CFILE_GLOB=$(top_srcdir)/glib/*.c $(top_srcdir)/gmodule/*.c

# Headers to ignore
IGNORE_HFILES=			\
	build			\
	gobject 		\
	config.h 		\
	glibintl.h		\
	gbsearcharray.h		\
	gmoduleconf.h		\
	gunibreak.h		\
	gunicomp.h		\
	gunidecomp.h		\
	gunichartables.h	\
	glibconfig-sysdefs.h	\
	gdebug.h		\
	gprintfint.h		\
	gnulib

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--sgml-mode --output-format=xml --ignore-files=trio

# Images to copy into HTML directory
HTML_IMAGES =  				\
	mainloop-states.gif

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE)
content_files =				\
	running.sgml			\
	building.sgml			\
	changes.sgml			\
	compiling.sgml			\
	resources.sgml			\
	version.xml			\
	glib-gettextize.xml

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=

man_MANS = glib-gettextize.1 

if ENABLE_MAN

%.1 : %.xml 
	@XSLTPROC@ -nonet http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

endif

# include common portion ...
include $(top_srcdir)/gtk-doc.make

# Other files to distribute
EXTRA_DIST +=				\
	mainloop-states.fig		\
	mainloop-states.png		\
	mainloop-states.eps		\
	version.xml.in
