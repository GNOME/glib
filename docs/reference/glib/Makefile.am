## Process this file with automake to produce Makefile.in

# The name of the module.
DOC_MODULE=glib

# The top-level SGML file.
DOC_MAIN_SGML_FILE=glib-docs.sgml

# The directory containing the source code
DOC_SOURCE_DIR=../../..

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=

# Headers to ignore
IGNORE_HFILES=			\
	gobject 		\
	config.h 		\
	gmoduleconf.h		\
	gunidecomp.h		\
	gunichartables.h	\
	glibconfig-sysdefs.h

tmpl_sources =				\
	tmpl/allocators.sgml		\
	tmpl/arrays.sgml		\
	tmpl/arrays_byte.sgml		\
	tmpl/arrays_pointer.sgml	\
	tmpl/async_queues.sgml		\
	tmpl/byte_order.sgml		\
	tmpl/caches.sgml		\
	tmpl/completion.sgml		\
	tmpl/datalist.sgml		\
	tmpl/datasets.sgml		\
	tmpl/date.sgml			\
	tmpl/error_reporting.sgml	\
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
	tmpl/numerical.sgml		\
	tmpl/quarks.sgml		\
	tmpl/queue.sgml			\
	tmpl/random_numbers.sgml	\
	tmpl/relations.sgml		\
	tmpl/scanner.sgml		\
	tmpl/string_chunks.sgml		\
	tmpl/string_utils.sgml		\
	tmpl/strings.sgml		\
	tmpl/thread_pools.sgml		\
	tmpl/threads.sgml		\
	tmpl/timers.sgml		\
	tmpl/trash_stack.sgml		\
	tmpl/trees-binary.sgml		\
	tmpl/trees-nary.sgml		\
	tmpl/type_conversion.sgml	\
	tmpl/types.sgml			\
	tmpl/unicode.sgml		\
	tmpl/warnings.sgml		\
	tmpl/windows.sgml


####################################
# Everything below here is generic #
####################################

TARGET_DIR=$(HTML_DIR)/$(DOC_MODULE)

EXTRA_DIST = 				\
	$(DOC_MAIN_SGML_FILE)		\
	$(DOC_MODULE)-decl.txt		\
	$(DOC_MODULE)-sections.txt	\
	$(DOC_MODULE)-overrides.txt

if ENABLE_GTK_DOC
all-local: html/index.html

$(DOC_MODULE)-decl.txt: 
	$(MAKE) scan

$(tmpl_sources): $(DOC_MODULE)-decl.txt $(DOC_MODULE)-sections.txt $(DOC_MODULE)-overrides.txt
	$(MAKE) templates

sgml/$(DOC_MODULE)-doc.bottom: $(tmpl_sources) $(DOC_MODULE)-decl.txt
	$(MAKE) sgml

html/index.html: sgml/$(DOC_MODULE)-doc.bottom $(DOC_MAIN_SGML_FILE)
	$(MAKE) html
endif

scan:
	-(cd $(srcdir) \
	&& gtkdoc-scan --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR) --ignore-headers="$(IGNORE_HFILES)" )

templates: 
	cd $(srcdir) && gtkdoc-mktmpl --module=$(DOC_MODULE)

sgml:
	cd $(srcdir) \
	&& gtkdoc-mkdb --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR)

html:
	test -d $(srcdir)/html || mkdir $(srcdir)/html
	-cd $(srcdir)/html && gtkdoc-mkhtml $(DOC_MODULE) ../$(DOC_MAIN_SGML_FILE)
	@echo '-- Fixing Crossreferences' 
	cd $(srcdir) && gtkdoc-fixxref --module-dir=html --html-dir=$(HTML_DIR) $(FIXXREF_OPTIONS)

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

#
# Require gtk-doc when making dist
#
if ENABLE_GTK_DOC
dist-check-gtkdoc:
else
dist-check-gtkdoc:
	@echo "*** gtk-doc must be installed and enabled in order to make dist"
	@false
endif

dist-hook: dist-check-gtkdoc
	mkdir $(distdir)/tmpl
	mkdir $(distdir)/sgml
	mkdir $(distdir)/html
	-cp $(srcdir)/tmpl/*.sgml $(distdir)/tmpl
	-cp $(srcdir)/sgml/*.sgml $(distdir)/sgml
	-cp $(srcdir)/sgml/*.bottom $(srcdir)/sgml/*.top $(distdir)/sgml
	-cp $(srcdir)/html/index.sgml $(distdir)/html
	-cp $(srcdir)/html/*.html $(srcdir)/html/*.css $(distdir)/html

.PHONY : html sgml templates scan
