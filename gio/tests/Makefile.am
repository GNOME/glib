
NULL =
BUILT_SOURCES =

include $(top_srcdir)/Makefile.decl

insttestdir = $(pkglibexecdir)/installed-tests

SUBDIRS = gdbus-object-manager-example services

AM_CPPFLAGS = 				\
	-DG_LOG_DOMAIN=\"GLib-GIO\" 	\
	$(gio_INCLUDES)			\
	$(GLIB_DEBUG_FLAGS)		\
	-I$(top_builddir)/gio		\
	-I$(top_srcdir)/gio		\
	$(DBUS1_CFLAGS)			\
	-DSRCDIR=\""$(srcdir)"\" 	\
	-DTEST_SERVICES=\""$(abs_top_builddir)/gio/tests/services"\"

AM_CFLAGS = $(GLIB_WARN_CFLAGS)

noinst_PROGRAMS = $(TEST_PROGS) $(SAMPLE_PROGS)
noinst_DATA = $(MISC_STUFF)

LDADD     = 					\
	$(top_builddir)/glib/libglib-2.0.la 		\
	$(top_builddir)/gthread/libgthread-2.0.la	\
	$(top_builddir)/gobject/libgobject-2.0.la	\
	$(top_builddir)/gmodule/libgmodule-2.0.la	\
	$(top_builddir)/gio/libgio-2.0.la

test_progs =	 		\
	io-stream		\
	memory-input-stream 	\
	memory-output-stream 	\
	readwrite		\
	g-file 			\
	g-file-info 		\
	converter-stream	\
	data-input-stream 	\
	data-output-stream 	\
	g-icon			\
	buffered-input-stream	\
	buffered-output-stream	\
	sleepy-stream		\
	filter-streams		\
	volumemonitor		\
	simple-async-result	\
	srvtarget		\
	contexts		\
	gsettings		\
	gschema-compile		\
	async-close-output-stream \
	gdbus-addresses		\
	network-address		\
	gdbus-message		\
	socket			\
	pollable		\
	tls-certificate		\
	tls-interaction		\
	cancellable		\
	vfs			\
	network-monitor		\
	fileattributematcher	\
	resources		\
	proxy-test		\
	simple-proxy		\
	inet-address		\
	permission		\
	task			\
	credentials		\
	$(NULL)

TEST_PROGS += $(test_progs)

if HAVE_DBUS_DAEMON
TEST_PROGS +=			\
	actions 		\
	gdbus-connection	\
	gdbus-connection-loss	\
	gdbus-connection-slow	\
	gdbus-names		\
	gdbus-proxy		\
	gdbus-proxy-threads	\
	gdbus-proxy-well-known-name \
	gdbus-introspection	\
	gdbus-threading		\
	gdbus-export		\
	gdbus-error		\
	gdbus-bz627724		\
	gmenumodel		\
	$(NULL)
endif


if OS_UNIX
TEST_PROGS +=			\
	gdbus-close-pending	\
	gdbus-connection-flush	\
	gdbus-peer		\
	gdbus-exit-on-close	\
	gdbus-non-socket	\
	gdbus-peer-object-manager \
	appinfo			\
	contenttype		\
	mimeapps		\
	file			\
	$(NULL)
endif

SAMPLE_PROGS = 				\
	resolver			\
	socket-server			\
	socket-client			\
	echo-server			\
	httpd				\
	send-data			\
	filter-cat			\
	gdbus-example-export		\
	gdbus-example-own-name		\
	gdbus-example-watch-name	\
	gdbus-example-watch-proxy	\
	gdbus-example-server		\
	gdbus-example-subtree		\
	gdbus-example-peer		\
	gdbus-example-proxy-subclass	\
	gdbus-connection-flush-helper	\
	proxy				\
	gapplication-example-open	\
	gapplication-example-cmdline	\
	gapplication-example-cmdline2	\
	gapplication-example-cmdline3	\
	gapplication-example-actions	\
	gapplication-example-dbushooks	\
	gdbus-daemon			\
	gdbus-testserver		\
	$(NULL)

if OS_UNIX
TEST_PROGS += 			\
	live-g-file		\
	desktop-app-info	\
	unix-fd 		\
	unix-streams 		\
	gapplication 		\
	basic-application	\
	gdbus-test-codegen 	\
	socket-address		\
	$(NULL)
SAMPLE_PROGS +=					\
	gdbus-example-unix-fd-client 		\
	gdbus-example-objectmanager-server 	\
	gdbus-example-objectmanager-client 	\
	gdbus-test-fixture			\
	appinfo-test				\
	$(NULL)
endif

if OS_WIN32
TEST_PROGS += win32-streams
endif

if PLATFORM_WIN32
no_undefined = -no-undefined
endif

actions_SOURCES   = actions.c gdbus-sessionbus.c gdbus-sessionbus.h

unix_streams_LDADD       = $(LDADD) \
	$(top_builddir)/gthread/libgthread-2.0.la

win32_streams_LDADD      = $(LDADD) \
	$(top_builddir)/gthread/libgthread-2.0.la

resolver_LDADD		  = $(LDADD) \
	$(top_builddir)/gthread/libgthread-2.0.la

socket_server_LDADD	  = $(LDADD) \
	$(top_builddir)/gthread/libgthread-2.0.la

socket_client_SOURCES	  = socket-client.c \
                            gtlsconsoleinteraction.c \
                            gtlsconsoleinteraction.h
socket_client_LDADD	  = $(LDADD) \
	$(top_builddir)/gthread/libgthread-2.0.la

echo_server_LDADD	  = $(LDADD) \
	$(top_builddir)/gthread/libgthread-2.0.la

httpd_LDADD		  = $(LDADD) \
	$(top_builddir)/gthread/libgthread-2.0.la

send_data_LDADD		  = $(LDADD) \
	$(top_builddir)/gthread/libgthread-2.0.la

contexts_LDADD		  = $(LDADD) \
	$(top_builddir)/gthread/libgthread-2.0.la

gdbus_daemon_SOURCES = gdbus-daemon.c $(top_srcdir)/gio/gdbusdaemon.c $(top_builddir)/gio/gdbus-daemon-generated.c

gdbus_testserver_SOURCES = gdbus-testserver.c

if HAVE_DBUS1
TEST_PROGS += gdbus-serialization
gdbus_serialization_SOURCES = gdbus-serialization.c gdbus-tests.h gdbus-tests.c
gdbus_serialization_CFLAGS = $(AM_CFLAGS) $(DBUS1_CFLAGS)
gdbus_serialization_LDADD = $(LDADD) $(DBUS1_LIBS)
endif

TEST_PROGS += gdbus-auth
gdbus_auth_SOURCES = gdbus-auth.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c
gdbus_auth_LDADD = $(LDADD)

gdbus_bz627724_SOURCES = gdbus-bz627724.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c

gdbus_close_pending_SOURCES = gdbus-close-pending.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c

if OS_UNIX
gdbus-test-codegen-generated.h gdbus-test-codegen-generated.c : test-codegen.xml Makefile $(top_builddir)/gio/gdbus-2.0/codegen/gdbus-codegen
	$(AM_V_GEN) UNINSTALLED_GLIB_SRCDIR=$(top_srcdir) \
		UNINSTALLED_GLIB_BUILDDIR=$(top_builddir) \
		$(PYTHON) $(top_builddir)/gio/gdbus-2.0/codegen/gdbus-codegen \
		--interface-prefix org.project. \
		--generate-c-code gdbus-test-codegen-generated \
		--c-generate-object-manager \
		--c-namespace Foo_iGen \
		--generate-docbook gdbus-test-codegen-generated-doc \
		--annotate "org.project.Bar" Key1 Value1 \
		--annotate "org.project.Bar" org.gtk.GDBus.Internal Value2 \
		--annotate "org.project.Bar.HelloWorld()" Key3 Value3 \
		--annotate "org.project.Bar::TestSignal" Key4 Value4 \
		--annotate "org.project.Bar:ay" Key5 Value5 \
		--annotate "org.project.Bar.TestPrimitiveTypes()[val_int32]" Key6 Value6 \
		--annotate "org.project.Bar.TestPrimitiveTypes()[ret_uint32]" Key7 Value7 \
		--annotate "org.project.Bar::TestSignal[array_of_strings]" Key8 Value8 \
		$(srcdir)/test-codegen.xml \
		$(NULL)

BUILT_SOURCES += gdbus-test-codegen-generated.c gdbus-test-codegen-generated.h

gdbus_test_codegen_SOURCES  = gdbus-test-codegen.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c
gdbus_test_codegen_SOURCES += gdbus-test-codegen-generated.c gdbus-test-codegen-generated.h

endif # OS_UNIX

gdbus_connection_SOURCES = gdbus-connection.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c

gdbus_connection_flush_SOURCES = \
	gdbus-connection-flush.c \
	test-io-stream.c \
	test-io-stream.h \
	test-pipe-unix.c \
	test-pipe-unix.h \
	$(NULL)

gdbus_connection_loss_SOURCES = gdbus-connection-loss.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c

gdbus_connection_slow_SOURCES = gdbus-connection-slow.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c

gdbus_names_SOURCES = gdbus-names.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c

gdbus_proxy_SOURCES = gdbus-proxy.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c

gdbus_proxy_threads_SOURCES = gdbus-proxy-threads.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c

gdbus_proxy_well_known_name_SOURCES = gdbus-proxy-well-known-name.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c

gdbus_introspection_SOURCES = gdbus-introspection.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c

gdbus_threading_SOURCES = gdbus-threading.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c

gdbus_export_SOURCES = gdbus-export.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c

gdbus_error_SOURCES = gdbus-error.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c

gdbus_non_socket_SOURCES = \
	gdbus-non-socket.c \
	gdbus-tests.c \
	gdbus-tests.h \
	test-io-stream.c \
	test-io-stream.h \
	test-pipe-unix.c \
	test-pipe-unix.h \
	$(NULL)

gdbus_exit_on_close_SOURCES = gdbus-exit-on-close.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c

resources_SOURCES = resources.c test_resources.c test_resources2.c test_resources2.h
resources_DEPENDENCIES = test.gresource

gapplication_SOURCES = gapplication.c gdbus-sessionbus.h gdbus-sessionbus.c gdbus-tests.h gdbus-tests.c

gmenumodel_SOURCES = gmenumodel.c gdbus-sessionbus.h gdbus-sessionbus.c

TESTS_ENVIRONMENT = \
	GLIB_MKENUMS=../../gobject/glib-mkenums \
	GLIB_COMPILE_SCHEMAS=../glib-compile-schemas

schema_tests = \
	array-default-not-in-choices.gschema.xml	\
	bad-choice.gschema.xml				\
	bad-key.gschema.xml				\
	bad-key2.gschema.xml				\
	bad-key3.gschema.xml				\
	bad-key4.gschema.xml				\
	bad-type.gschema.xml				\
	bare-alias.gschema.xml				\
	choice-alias.gschema.xml			\
	choice-bad.gschema.xml				\
	choice-badtype.gschema.xml			\
	choice-invalid-alias.gschema.xml		\
	choice-missing-value.gschema.xml		\
	choice-shadowed-alias.gschema.xml		\
	choice-upside-down.gschema.xml			\
	choice.gschema.xml				\
	choices-wrong-type.gschema.xml			\
	default-in-aliases.gschema.xml			\
	default-not-in-choices.gschema.xml		\
	default-out-of-range.gschema.xml		\
	empty-key.gschema.xml				\
	enum-with-aliases.gschema.xml			\
	enum-with-bad-default.gschema.xml		\
	enum-with-chained-alias.gschema.xml		\
	enum-with-choice.gschema.xml			\
	enum-with-invalid-alias.gschema.xml		\
	enum-with-repeated-alias.gschema.xml		\
	enum-with-repeated-nick.gschema.xml		\
	enum-with-repeated-value.gschema.xml		\
	enum-with-shadow-alias.gschema.xml		\
	enum.gschema.xml				\
	flags-aliased-default.gschema.xml		\
	flags-bad-default.gschema.xml			\
	flags-more-than-one-bit.gschema.xml		\
	flags-with-enum-attr.gschema.xml		\
	flags-with-enum-tag.gschema.xml			\
	extend-and-shadow-indirect.gschema.xml		\
	extend-and-shadow.gschema.xml			\
	extend-missing.gschema.xml			\
	extend-nonlist.gschema.xml			\
	extend-self.gschema.xml				\
	extend-wrong-list-indirect.gschema.xml		\
	extend-wrong-list.gschema.xml			\
	extending.gschema.xml				\
	from-docs.gschema.xml				\
	incomplete-list.gschema.xml			\
	inherit-gettext-domain.gschema.xml		\
	invalid-path.gschema.xml			\
	key-in-list-indirect.gschema.xml		\
	key-in-list.gschema.xml				\
	list-of-missing.gschema.xml			\
	missing-quotes.gschema.xml				\
	no-default.gschema.xml				\
	overflow.gschema.xml				\
	override-missing.gschema.xml			\
	override-range-error.gschema.xml		\
	override-then-key.gschema.xml			\
	override-twice.gschema.xml			\
	override-type-error.gschema.xml			\
	override.gschema.xml				\
	range-badtype.gschema.xml			\
	range-default-high.gschema.xml			\
	range-default-low.gschema.xml			\
	range-high-default.gschema.xml			\
	range-low-default.gschema.xml			\
	range-missing-max.gschema.xml			\
	range-missing-min.gschema.xml			\
	range-parse-error.gschema.xml			\
	range-wrong-type.gschema.xml			\
	range.gschema.xml				\
	wrong-category.gschema.xml			\
	$(NULL)

schema_test_files = $(addprefix schema-tests/,$(schema_tests))

proxy_LDADD   = $(LDADD) \
	$(top_builddir)/gthread/libgthread-2.0.la

tls_certificate_SOURCES = tls-certificate.c gtesttlsbackend.c gtesttlsbackend.h

cert_tests = 			\
	cert1.pem		\
	cert2.pem		\
	cert3.pem		\
	cert-key.pem		\
	cert-list.pem		\
	key8.pem		\
	key-cert.pem		\
	key.pem			\
	nothing.pem		\
	$(NULL)

cert_test_files = $(addprefix cert-tests/,$(cert_tests))

# -----------------------------------------------------------------------------

if OS_UNIX
gdbus_example_objectmanager_server_CFLAGS   = $(AM_CFLAGS) -I$(top_builddir)/gio/tests/gdbus-object-manager-example
gdbus_example_objectmanager_server_LDADD    = $(top_builddir)/gio/tests/gdbus-object-manager-example/libgdbus-example-objectmanager.la $(LDADD)

gdbus_example_objectmanager_client_CFLAGS   = $(AM_CFLAGS) -I$(top_builddir)/gio/tests/gdbus-object-manager-example
gdbus_example_objectmanager_client_LDADD    = $(top_builddir)/gio/tests/gdbus-object-manager-example/libgdbus-example-objectmanager.la $(LDADD)

gdbus_peer_SOURCES = gdbus-peer.c gdbus-tests.h gdbus-tests.c
gdbus_peer_CFLAGS  = $(AM_CFLAGS) -I$(top_builddir)/gio/tests/gdbus-object-manager-example
gdbus_peer_LDADD   = $(top_builddir)/gio/tests/gdbus-object-manager-example/libgdbus-example-objectmanager.la $(LDADD)

gdbus_test_fixture_SOURCES = gdbus-test-fixture.c
gdbus_test_fixture_CFLAGS  = $(AM_CFLAGS) -I$(top_builddir)/gio/tests/gdbus-object-manager-example
gdbus_test_fixture_LDADD   = $(top_builddir)/gio/tests/gdbus-object-manager-example/libgdbus-example-objectmanager.la $(LDADD)

endif OS_UNIX

# -----------------------------------------------------------------------------

EXTRA_DIST += \
	socket-common.c			\
	org.gtk.test.gschema		\
	org.gtk.test.gschema.xml	\
	testenum.h			\
	enums.xml.template		\
	de.po				\
	$(schema_test_files)		\
	org.gtk.schemasourcecheck.gschema.xml \
	appinfo-test.desktop		\
	appinfo-test2.desktop		\
	appinfo-test-gnome.desktop	\
	appinfo-test-notgnome.desktop	\
	test-codegen.xml		\
	$(cert_test_files)		\
	test.gresource.xml		\
	test1.txt			\
	test2.gresource.xml		\
	test2.txt			\
	test3.gresource.xml		\
	test3.txt			\
	test4.gresource.xml		\
	$(NULL)

MISC_STUFF = test.mo test.gresource

test.mo: de.po
	$(MSGFMT) -o test.mo $(srcdir)/de.po; \
	$(MKDIR_P) de/LC_MESSAGES; \
	cp -f test.mo de/LC_MESSAGES

if CROSS_COMPILING
  glib_compile_resources=$(GLIB_COMPILE_RESOURCES)
else
  glib_compile_resources=$(top_builddir)/gio/glib-compile-resources
endif

BUILT_SOURCES += test_resources.c test_resources2.c test_resources2.h
test_resources.c: test2.gresource.xml Makefile $(shell $(glib_compile_resources) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/test2.gresource.xml)
	$(AM_V_GEN) $(glib_compile_resources) --target=$@ --sourcedir=$(srcdir) --generate-source --c-name _g_test1 $<

test_resources2.h test_resources2.c: test3.gresource.xml Makefile $(shell $(glib_compile_resources) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/test3.gresource.xml)
	$(AM_V_GEN) $(glib_compile_resources) --target=$@ --sourcedir=$(srcdir) --generate --c-name _g_test2 --manual-register $<

plugin_resources.c: test4.gresource.xml Makefile $(shell $(glib_compile_resources) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/test4.gresource.xml)
	$(AM_V_GEN) $(glib_compile_resources) --target=$@ --sourcedir=$(srcdir) --generate-source --c-name _g_plugin $<

test.gresource: test.gresource.xml Makefile $(shell $(glib_compile_resources) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/test.gresource.xml)
	$(AM_V_GEN) $(glib_compile_resources) --target=$@ --sourcedir=$(srcdir) $<

if BUILDOPT_INSTALL_TESTS
insttest_LTLIBRARIES = libresourceplugin.la
else
noinst_LTLIBRARIES = libresourceplugin.la
endif

libresourceplugin_la_SOURCES = resourceplugin.c plugin_resources.c
libresourceplugin_la_LDFLAGS = -avoid-version -module $(no_undefined) -rpath $(insttestdir)
libresourceplugin_la_LIBADD = $(LDADD)

CLEANFILES = gdbus-test-codegen-generated.[ch] gdbus-test-codegen-generated-doc-*.xml test_resources2.[ch] plugin_resources.c

DISTCLEANFILES = \
	applications/mimeinfo.cache	\
	org.gtk.test.enums.xml		\
	de/LC_MESSAGES/test.mo		\
	test.mo				\
	test.gresource			\
	test_resources.c		\
	gsettings.store			\
	gschemas.compiled 		\
	schema-source/gschemas.compiled

distclean-local:
	rm -rf xdgdatahome xdgdatadir

test_files = 					\
	contexts.c				\
	g-icon.c				\
	enums.xml.template			\
	testenum.h				\
	org.gtk.test.gschema.xml		\
	org.gtk.schemasourcecheck.gschema.xml 	\
	test.gresource				\
	$(NULL)

if BUILDOPT_INSTALL_TESTS
insttest_PROGRAMS = $(test_progs)

schematestdir = $(insttestdir)/schema-tests
schematest_DATA = $(schema_test_files)

certtestdir = $(insttestdir)/cert-tests
certtest_DATA = $(cert_test_files)

testdatadir = $(insttestdir)
testdata_DATA = $(test_files)

testmetadir = $(datadir)/installed-tests/$(PACKAGE)
testmeta_DATA = $(test_progs:=.test)

%.test: % Makefile
	$(AM_V_GEN) (echo '[Test]' > $@.tmp; \
	 echo 'Type=session' >> $@.tmp; \
	 echo 'Exec=env G_TEST_DATA=$(pkglibexecdir)/installed-tests $(pkglibexecdir)/installed-tests/$<' >> $@.tmp; \
	 mv $@.tmp $@)

endif
