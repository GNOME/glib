GENERATED_ITEMS = \
	glib.vcproj		\
	gobject.vcproj		\
	gio.vcproj		\
	glib-compile-schemas.vcproj	\
	glib-compile-resources.vcproj	\
	glib-gen-srcs.vsprops	\
	glib-install.vsprops

EXTRA_DIST = \
	README.txt		\
	glib.sln		\
	glib.vcprojin		\
	glib-genmarshal.vcproj	\
	gspawn-win32-helper-console.vcproj 	\
	gspawn-win32-helper.vcproj	\
	gmodule.vcproj		\
	gobject.vcprojin	\
	gthread.vcproj		\
	gio.vcprojin		\
	gsettings.vcproj	\
	gresource.vcproj	\
	gio-querymodules.vcproj	\
	gdbus.vcproj		\
	glib-compile-schemas.vcprojin	\
	glib-compile-resources.vcprojin	\
	glib-install.vcproj	\
	glib-build-defines.vsprops	\
	glib-version-paths.vsprops	\
	glib-gen-srcs.vsprops.in	\
	glib-install.vspropsin	\
	$(GENERATED_ITEMS)

glib-install.vsprops: $(top_srcdir)/build/win32/vs9/glib-install.vspropsin glib.headers gobject.headers gio.headers
	$(CPP) -P - <$(top_srcdir)/build/win32/vs9/glib-install.vspropsin >$@
	rm glib.headers
	rm gobject.headers
	rm gio.headers

DISTCLEANFILES = $(GENERATED_ITEMS)
