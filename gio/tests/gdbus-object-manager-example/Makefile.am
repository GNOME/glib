NULL =
BUILT_SOURCES =
CLEANFILES =

include $(top_srcdir)/Makefile.decl

INCLUDES = 			\
	-g 			\
	$(gio_INCLUDES)		\
	$(GLIB_DEBUG_FLAGS)	\
	-I$(top_builddir)/gio	\
	-I$(top_srcdir)/gio	\
	-DSRCDIR=\""$(srcdir)"\"

# ------------------------------------------------------------------------

GDBUS_GENERATED = 										\
	gdbus-example-objectmanager-generated.h 						\
	gdbus-example-objectmanager-generated.c 						\
	gdbus-example-objectmanager-generated-org.gtk.GDBus.Example.ObjectManager.Animal.xml    \
	gdbus-example-objectmanager-generated-org.gtk.GDBus.Example.ObjectManager.Cat.xml 	\
	$(NULL)

$(GDBUS_GENERATED) : gdbus-example-objectmanager.xml Makefile $(top_builddir)/gio/gdbus-2.0/codegen/gdbus-codegen
	$(AM_V_GEN) UNINSTALLED_GLIB_SRCDIR=$(top_srcdir) \
		UNINSTALLED_GLIB_BUILDDIR=$(top_builddir) \
		$(PYTHON) $(top_builddir)/gio/gdbus-2.0/codegen/gdbus-codegen \
		--interface-prefix org.gtk.GDBus.Example.ObjectManager. \
		--c-namespace Example \
		--c-generate-object-manager \
		--generate-c-code gdbus-example-objectmanager-generated \
		--generate-docbook gdbus-example-objectmanager-generated \
		$< \
		$(NULL)

BUILT_SOURCES += $(GDBUS_GENERATED)

noinst_LTLIBRARIES = libgdbus-example-objectmanager.la
libgdbus_example_objectmanager_la_SOURCES  = gdbus-example-objectmanager-generated.h gdbus-example-objectmanager-generated.c
libgdbus_example_objectmanager_la_LIBADD   = 		\
	$(top_builddir)/glib/libglib-2.0.la		\
	$(top_builddir)/gobject/libgobject-2.0.la	\
	$(top_builddir)/gmodule/libgmodule-2.0.la	\
	$(top_builddir)/gio/libgio-2.0.la		\
	$(NULL)

EXTRA_DIST += gdbus-example-objectmanager.xml

CLEANFILES += $(GDBUS_GENERATED)

