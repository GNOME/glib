include $(top_srcdir)/Makefile.decl

NULL =

module_flags = -export_dynamic -avoid-version -module -no-undefined -export-symbols-regex '^g_io_module_(load|unload)'

giomodule_LTLIBRARIES = libgiofam.la
giomoduledir = $(GIO_MODULE_DIR)

libgiofam_la_SOURCES = 		\
	fam-helper.c			\
	fam-helper.h			\
	fam-module.c			\
	gfamdirectorymonitor.c		\
	gfamdirectorymonitor.h		\
	gfamfilemonitor.c		\
	gfamfilemonitor.h		\
	$(NULL)

libgiofam_la_CFLAGS = \
	-DG_LOG_DOMAIN=\"GLib-GIO\"	\
	-I$(top_srcdir) 		\
	-I$(top_srcdir)/glib 		\
	-I$(top_srcdir)/gmodule		\
	-I$(top_srcdir)/gio 		\
	$(GLIB_DEBUG_FLAGS)		\
	-DGIO_MODULE_DIR=\"$(GIO_MODULE_DIR)\"	\
	-DGIO_COMPILATION		\
	-DG_DISABLE_DEPRECATED

libgiofam_la_LDFLAGS = $(module_flags)
libgiofam_la_LIBADD = \
		$(top_builddir)/gio/libgio-2.0.la \
		$(top_builddir)/gobject/libgobject-2.0.la \
		$(top_builddir)/glib/libglib-2.0.la \
		$(GLIB_LIBS) \
		$(FAM_LIBS) \
		$(NULL)

