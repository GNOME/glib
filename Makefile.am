## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

SUBDIRS = . m4macros glib gobject gmodule gthread docs tests build

INCLUDES = -DG_LOG_DOMAIN=g_log_domain_glib @GLIB_DEBUG_FLAGS@ \
	-DG_DISABLE_DEPRECATED -DGLIB_COMPILATION

EXTRA_DIST = 			\
	ChangeLog.pre-1-2	\
	glib.spec.in		\
	NEWS.pre-1-3		\
	acglib.m4		\
	sanity_check		\
	README.win32		\
	TODO.xml		\
	makefile.msc.in		\
	makefile.mingw.in	\
	glibconfig.h.win32.in	\
	config.h.win32.in	\
	build-dll		\
	glib.rc.in		\
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
	makefile.msc		\
	makefile.mingw		\
	glibconfig.h.win32	\
	config.h.win32		\
	glib.rc			\
	glib.spec

configexecincludedir = $(libdir)/glib-2.0/include
#configexecinclude_DATA = glibconfig.h

install-exec-local: glibconfig.h
	$(mkinstalldirs) $(DESTDIR)$(configexecincludedir)
	file=$(DESTDIR)$(configexecincludedir)/glibconfig.h; \
	if test -r $$file && cmp -s glibconfig.h $$file; then :; \
	else $(INSTALL_DATA) glibconfig.h $$file; fi

CONFIGURE_DEPENDENCIES = acglib.m4

BUILT_SOURCES = stamp-gc-h #note: not glibconfig.h
glibconfig.h: stamp-gc-h
	@if test -f glibconfig.h; then :; \
	else rm -f stamp-gc-h; $(MAKE) stamp-gc-h; fi
stamp-gc-h: config.status
	CONFIG_FILES= CONFIG_HEADERS= CONFIG_OTHER=glibconfig.h ./config.status
	echo timestamp > stamp-gc-h


pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = glib-2.0.pc gobject-2.0.pc gmodule-2.0.pc gthread-2.0.pc

glibconfig.h.win32: $(top_builddir)/config.status $(top_srcdir)/glibconfig.h.win32.in
	cd $(top_builddir) && CONFIG_FILES=$@ CONFIG_HEADERS= $(SHELL) ./config.status

config.h.win32: $(top_builddir)/config.status $(top_srcdir)/config.h.win32.in
	cd $(top_builddir) && CONFIG_FILES=$@ CONFIG_HEADERS= $(SHELL) ./config.status

makefile.msc: $(top_builddir)/config.status $(top_srcdir)/makefile.msc.in
	cd $(top_builddir) && CONFIG_FILES=$@ CONFIG_HEADERS= $(SHELL) ./config.status

makefile.mingw: $(top_builddir)/config.status $(top_srcdir)/makefile.mingw.in
	cd $(top_builddir) && CONFIG_FILES=$@ CONFIG_HEADERS= $(SHELL) ./config.status

glib.rc: $(top_builddir)/config.status $(top_srcdir)/glib.rc.in
	cd $(top_builddir) && CONFIG_FILES=$@ CONFIG_HEADERS= $(SHELL) ./config.status

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
	cd $(top_builddir) && CONFIG_FILES="INSTALL README" CONFIG_HEADERS= $(SHELL) ./config.status
	files='$(BUILT_EXTRA_DIST) INSTALL README'; \
	for f in $$files; do \
	  if test -f $$f; then d=.; else d=$(srcdir); fi; \
	  rm -f $(distdir)/$$f && cp $$d/$$f $(distdir) || exit 1; done
