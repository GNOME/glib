## Process this file with automake to produce Makefile.in

# The name of the module.
DOC_MODULE=glib

# The top-level SGML file.
DOC_MAIN_SGML_FILE=glib-docs.sgml


TARGET_DIR=$(HTML_DIR)/$(DOC_MODULE)

scan:
	gtkdoc-scan --module=$(DOC_MODULE) `glib-config --prefix`/include/glib.h `glib-config --prefix`/include/gmodule.h `glib-config --prefix`/lib/glib/include/glibconfig.h 

templates: scan
	gtkdoc-mktmpl --module=$(DOC_MODULE)

sgml:
	gtkdoc-mkdb --module=$(DOC_MODULE)

html:
	if ! test -d html ; then mkdir html ; fi
	-cd html && gtkdoc-mkhtml $(DOC_MODULE) ../$(DOC_MAIN_SGML_FILE)

clean-local:
	rm -f *~ *.bak *.hierarchy *.signals *.args *-unused.txt

maintainer-clean-local: clean
	rm -rf sgml html $(DOC_MODULE)-decl-list.txt $(DOC_MODULE)-decl.txt 

install-data-local:
	install -d -m 0755 $(TARGET_DIR)
	install -m 0644 html/*.html $(TARGET_DIR)
	install -m 0644 html/index.sgml $(TARGET_DIR)
	gtkdoc-fixxref --module=$(DOC_MODULE) --html-dir=$(HTML_DIR)

EXTRA_DIST = \
	$(DOC_MAIN_SGML_FILE) \
	$(DOC_MODULE)-sections.txt \
	glib-decl.txt \
	glib-overrides.txt

dist-hook:
	mkdir $(distdir)/tmpl
	cp -p tmpl/*.sgml $(distdir)/tmpl

.PHONY : html sgml templates scan
