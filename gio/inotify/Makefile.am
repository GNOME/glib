include $(top_srcdir)/Makefile.decl

NULL =

noinst_LTLIBRARIES = libinotify.la

libinotify_la_SOURCES = 		\
	inotify-kernel.c 		\
	inotify-sub.c 			\
	inotify-path.c 			\
	inotify-missing.c 		\
	inotify-helper.c 		\
	inotify-diag.c			\
	inotify-diag.h			\
	inotify-kernel.h 		\
	inotify-missing.h		\
	inotify-path.h			\
	inotify-sub.h			\
	inotify-helper.h		\
	ginotifyfilemonitor.c		\
	ginotifyfilemonitor.h		\
	ginotifydirectorymonitor.c	\
	ginotifydirectorymonitor.h	\
	$(NULL)

libinotify_la_CFLAGS = \
	-DG_LOG_DOMAIN=\"GLib-GIO\"	\
	-I$(top_srcdir) 		\
	-I$(top_srcdir)/glib 		\
	-I$(top_srcdir)/gmodule		\
	-I$(top_srcdir)/gio 		\
	-I$(top_builddir)/gio 		\
	$(GLIB_DEBUG_FLAGS)		\
	-DGIO_MODULE_DIR=\"$(GIO_MODULE_DIR)\"	\
	-DGIO_COMPILATION		\
	-DG_DISABLE_DEPRECATED

