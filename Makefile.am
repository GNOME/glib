## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.7

SUBDIRS = . m4macros glib gobject gmodule gthread tests build po docs

bin_SCRIPTS = glib-gettextize

INCLUDES = -DG_LOG_DOMAIN=g_log_domain_glib @GLIB_DEBUG_FLAGS@ \
	-DG_DISABLE_DEPRECATED -DGLIB_COMPILATION

EXTRA_DIST = 			\
	ChangeLog.pre-2-2	\
	ChangeLog.pre-2-0	\
	ChangeLog.pre-1-2	\
	glib-zip.in		\
	NEWS.pre-1-3		\
	acglib.m4		\
	sanity_check		\
	README.in		\
	INSTALL.in		\
	README.win32		\
	makefile.msc		\
	glibconfig.h.win32.in	\
	msvc_recommended_pragmas.h \
	config.h.win32.in	\
        po/po2tbl.sed.in        \
	glib-2.0.pc.in 		\
	gobject-2.0.pc.in 	\
	gmodule-2.0.pc.in	\
	gthread-2.0.pc.in	\
	glib-2.0-uninstalled.pc.in 	\
	gobject-2.0-uninstalled.pc.in 	\
	gmodule-2.0-uninstalled.pc.in	\
	gthread-2.0-uninstalled.pc.in

# These may be in the builddir too
BUILT_EXTRA_DIST = 		\
	README			\
	INSTALL			\
	glibconfig.h.win32	\
	config.h.win32		\
	gtk-doc.make

configexecincludedir = $(libdir)/glib-2.0/include
#configexecinclude_DATA = glibconfig.h

install-exec-local: glibconfig.h
	$(mkinstalldirs) $(DESTDIR)$(configexecincludedir)
	file=$(DESTDIR)$(configexecincludedir)/glibconfig.h; \
	if test -r $$file && cmp -s glibconfig.h $$file; then :; \
	else $(INSTALL_DATA) glibconfig.h $$file; fi

uninstall-local:
	rm -f $(DESTDIR)$(configexecincludedir)/glibconfig.h

CONFIGURE_DEPENDENCIES = acglib.m4

BUILT_SOURCES = stamp-gc-h #note: not glibconfig.h
glibconfig.h: stamp-gc-h
	@if test -f glibconfig.h; then :; \
	else rm -f stamp-gc-h; $(MAKE) stamp-gc-h; fi
stamp-gc-h: config.status
	cd $(top_builddir) && $(SHELL) ./config.status glibconfig.h
	echo timestamp > stamp-gc-h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = glib-2.0.pc gobject-2.0.pc gmodule-2.0.pc gthread-2.0.pc

$(pkgconfig_DATA): config.status

# build documentation when doing distcheck
DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc

DISTCLEANFILES = glibconfig-sysdefs.h glibconfig.h stamp-gc-h

distclean-local:
	if test $(srcdir) = .; then :; else \
	    rm -f $(BUILT_EXTRA_DIST); \
	fi

.PHONY: files release sanity snapshot

files:
	@files=`ls $(DISTFILES) 2> /dev/null `; for p in $$files; do \
	  echo $$p; \
	done

release:
	rm -rf .deps */.deps
	cd docs && make glib.html
	$(MAKE) distcheck

sanity:
	./sanity_check $(VERSION)

snapshot:
	$(MAKE) dist distdir=$(PACKAGE)-snap`date +"%Y%m%d"`

snapcheck:
	$(MAKE) distcheck distdir=$(PACKAGE)-snap`date +"%Y%m%d"`

dist-hook: $(BUILT_EXTRA_DIST)
	files='$(BUILT_EXTRA_DIST)'; \
	for f in $$files; do \
	  if test -f $$f; then d=.; else d=$(srcdir); fi; \
	  rm -f $(distdir)/$$f && cp $$d/$$f $(distdir) || exit 1; done
