## Process this file with automake to produce Makefile.in

# The name of the module.
DOC_MODULE=glib

# The top-level SGML file.
DOC_MAIN_SGML_FILE=glib-docs.sgml

# The directory containing the source code (if it contains documentation).
DOC_SOURCE_DIR=$(top_srcdir)

TARGET_DIR=$(HTML_DIR)/$(DOC_MODULE)

tmpl_sources =				\
	tmpl/allocators.sgml		\
	tmpl/arrays.sgml		\
	tmpl/arrays_byte.sgml		\
	tmpl/arrays_pointer.sgml	\
	tmpl/byte_order.sgml		\
	tmpl/caches.sgml		\
	tmpl/completion.sgml		\
	tmpl/datalist.sgml		\
	tmpl/datasets.sgml		\
	tmpl/date.sgml			\
	tmpl/glib-unused.sgml		\
	tmpl/hash_tables.sgml		\
	tmpl/hooks.sgml			\
	tmpl/iochannels.sgml		\
	tmpl/limits.sgml		\
	tmpl/linked_lists_double.sgml	\
	tmpl/linked_lists_single.sgml	\
	tmpl/macros.sgml		\
	tmpl/macros_misc.sgml		\
	tmpl/main.sgml			\
	tmpl/memory.sgml		\
	tmpl/memory_chunks.sgml		\
	tmpl/messages.sgml		\
	tmpl/misc_utils.sgml		\
	tmpl/modules.sgml		\
	tmpl/quarks.sgml		\
	tmpl/relations.sgml		\
	tmpl/scanner.sgml		\
	tmpl/string_chunks.sgml		\
	tmpl/string_utils.sgml		\
	tmpl/strings.sgml		\
	tmpl/threads.sgml		\
	tmpl/timers.sgml		\
	tmpl/trees-binary.sgml		\
	tmpl/trees-nary.sgml		\
	tmpl/type_conversion.sgml	\
	tmpl/types.sgml			\
	tmpl/warnings.sgml		\
	tmpl/windows.sgml

glib_docdir = $(HTML_DIR)
glib_doc_DATA =				\
	glib-decl.txt			\
	glib-sections.txt		\
	glib-overrides.txt

EXTRA_DIST = 				\
	$(glib_doc_DATA)

html/book1.html: sgml/glib-doc.bottom
	$(MAKE) html

sgml/glib-doc.bottom: $(tmpl_sources)
	$(MAKE) sgml

scan:
	-(cd $(srcdir) \
	&& gtkdoc-scan --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR) --ignore-headers="config.h gmoduleconf.h gunidecomp.h gunichartables.h glibconfig-sysdefs.h" )

templates: scan
	cd $(srcdir) && gtkdoc-mktmpl --module=$(DOC_MODULE)

sgml:
	cd $(srcdir) \
	&& gtkdoc-mkdb --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR)

html:
	test -d $(srcdir)/html || mkdir $(srcdir)/html
	-cd $(srcdir)/html && gtkdoc-mkhtml $(DOC_MODULE) ../$(DOC_MAIN_SGML_FILE)

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
	  echo '-- Fixing Crossreferences' ; \
	  gtkdoc-fixxref --module=$(DOC_MODULE) --html-dir=$(HTML_DIR)|| true; \
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
