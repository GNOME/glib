## Process this file with automake to produce Makefile.in

# The name of the module.
DOC_MODULE=gobject

# The top-level SGML file.
DOC_MAIN_SGML_FILE=gobject-docs.sgml

# The directory containing the source code (if it contains documentation).
DOC_SOURCE_DIR=$(top_srcdir)/gobject

TARGET_DIR=$(HTML_DIR)/$(DOC_MODULE)

tmpl_sources =				\
	tmpl/enumerations_flags.sgml	\
	tmpl/generic_values.sgml	\
	tmpl/gobject-unused.sgml	\
	tmpl/objects.sgml		\
	tmpl/param_specs.sgml		\
	tmpl/standard_params.sgml	\
	tmpl/types.sgml			\
	tmpl/value_collection.sgml	\
	tmpl/value_types.sgml

gobject_docdir = $(HTML_DIR)
gobject_doc_DATA =			\
	gobject-sections.txt


EXTRA_DIST = 				\
	$(gobject_doc_DATA)

if ENABLE_GTK_DOC
all-local: html/index.html

gobject-decl.txt: 
	$(MAKE) scan

$(tmpl_sources): gobject-decl.txt gobject-sections.txt
	$(MAKE) templates

sgml/gobject-doc.bottom: $(tmpl_sources) gobject-decl.txt
	$(MAKE) sgml

html/index.html: sgml/gobject-doc.bottom
	$(MAKE) html
endif

scan:
	-(cd $(srcdir) \
	&& gtkdoc-scan --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR) )

templates:
	cd $(srcdir) && gtkdoc-mktmpl --module=$(DOC_MODULE)

sgml:
	cd $(srcdir) \
	&& gtkdoc-mkdb --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR)

html:
	test -d $(srcdir)/html || mkdir $(srcdir)/html
	-cd $(srcdir)/html && gtkdoc-mkhtml $(DOC_MODULE) ../$(DOC_MAIN_SGML_FILE)
	@echo '-- Fixing Crossreferences'
	gtkdoc-fixxref --module-dir=html --html-dir=$(HTML_DIR) --extra-dir=$(srcdir)/../glib/html

clean-local:
	rm -f *~ *.bak *.signals *-unused.txt

maintainer-clean-local: clean
	cd $(srcdir) && rm -rf sgml html $(DOC_MODULE)-decl-list.txt $(DOC_MODULE)-decl.txt

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(TARGET_DIR)
	(installfiles=`echo $(srcdir)/html/*.html`; \
	if test "$$installfiles" = '$(srcdir)/html/*.html'; \
	then echo '-- Nothing to install' ; \
	else \
	  for i in $$installfiles; do \
	    echo '-- Installing '$$i ; \
	    $(INSTALL_DATA) $$i $(DESTDIR)$(TARGET_DIR); \
	  done; \
	  echo '-- Installing $(srcdir)/html/index.sgml' ; \
	  $(INSTALL_DATA) $(srcdir)/html/index.sgml $(DESTDIR)$(TARGET_DIR); \
	fi)

dist-hook:
	mkdir $(distdir)/html
	mkdir $(distdir)/sgml
	mkdir $(distdir)/tmpl
	-cp $(srcdir)/html/*.html $(srcdir)/html/*.css $(distdir)/html
	-cp $(srcdir)/tmpl/*.sgml $(distdir)/tmpl
	-cp $(srcdir)/sgml/*.sgml $(distdir)/sgml
	-cp $(srcdir)/sgml/*.bottom $(srcdir)/sgml/*.top $(distdir)/sgml

.PHONY : html sgml templates scan
