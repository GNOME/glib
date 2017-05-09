include $(top_srcdir)/glib.mk

noinst_LTLIBRARIES += libkqueue.la

libkqueue_la_SOURCES = \
       gkqueuefilemonitor.c \
       kqueue_fnm.c \
       kqueue_fnm.h \
       $(NULL)

libkqueue_la_CFLAGS = \
       $(GLIB_HIDDEN_VISIBILITY_CFLAGS) \
       -DG_LOG_DOMAIN=\"GLib-GIO\" \
       $(gio_INCLUDES) \
       $(GLIB_DEBUG_FLAGS) \
       -DGIO_MODULE_DIR=\"$(GIO_MODULE_DIR)\" \
       -DGIO_COMPILATION \
       -DG_DISABLE_DEPRECATED
