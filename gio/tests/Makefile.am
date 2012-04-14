
NULL =
BUILT_SOURCES =

include $(top_srcdir)/Makefile.decl

SUBDIRS = gdbus-object-manager-example

INCLUDES = 			\
	-g 			\
	$(gio_INCLUDES)		\
	$(GLIB_DEBUG_FLAGS)	\
	-I$(top_builddir)/gio	\
	-I$(top_srcdir)/gio	\
	$(DBUS1_CFLAGS)		\
	-DSRCDIR=\""$(srcdir)"\"

noinst_PROGRAMS = $(TEST_PROGS) $(SAMPLE_PROGS)
noinst_DATA = $(MISC_STUFF)

progs_ldadd     = 					\
	$(top_builddir)/glib/libglib-2.0.la 		\
	$(top_builddir)/gthread/libgthread-2.0.la	\
	$(top_builddir)/gobject/libgobject-2.0.la	\
	$(top_builddir)/gmodule/libgmodule-2.0.la	\
	$(top_builddir)/gio/libgio-2.0.la

TEST_PROGS +=	 		\
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
	$(NULL)

if OS_UNIX
TEST_PROGS +=			\
	actions 		\
	gdbus-close-pending	\
	gdbus-connection	\
	gdbus-connection-flush	\
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
	gdbus-peer		\
	gdbus-exit-on-close	\
	gdbus-non-socket	\
	gdbus-bz627724		\
	gmenumodel		\
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
	appinfo-test			\
	proxy				\
	gapplication-example-open	\
	gapplication-example-cmdline	\
	gapplication-example-cmdline2	\
	gapplication-example-cmdline3	\
	gapplication-example-actions	\
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
	$(NULL)
SAMPLE_PROGS +=					\
	gdbus-example-unix-fd-client 		\
	gdbus-example-objectmanager-server 	\
	gdbus-example-objectmanager-client 	\
	$(NULL)
endif

if OS_WIN32
TEST_PROGS += win32-streams
endif

io_stream_SOURCES = io-stream.c
io_stream_LDADD   = $(progs_ldadd)

actions_SOURCES   = actions.c gdbus-sessionbus.c gdbus-sessionbus.h
actions_LDADD	  = $(progs_ldadd)

memory_input_stream_SOURCES	  = memory-input-stream.c
memory_input_stream_LDADD	  = $(progs_ldadd)

memory_output_stream_SOURCES	  = memory-output-stream.c
memory_output_stream_LDADD	  = $(progs_ldadd)

g_file_SOURCES	  = g-file.c
g_file_LDADD	  = $(progs_ldadd)

readwrite_SOURCES	  = readwrite.c
readwrite_LDADD	  = $(progs_ldadd)

g_file_info_SOURCES	= g-file-info.c
g_file_info_LDADD	= $(progs_ldadd)

converter_stream_SOURCES	= converter-stream.c
converter_stream_LDADD		= $(progs_ldadd)

data_input_stream_SOURCES	= data-input-stream.c
data_input_stream_LDADD		= $(progs_ldadd)

data_output_stream_SOURCES	= data-output-stream.c
data_output_stream_LDADD	= $(progs_ldadd)

async_close_output_stream_SOURCES	= async-close-output-stream.c
async_close_output_stream_LDADD		= $(progs_ldadd)

filter_cat_SOURCES	= filter-cat.c
filter_cat_LDADD	= $(progs_ldadd)

g_icon_SOURCES	= g-icon.c
g_icon_LDADD	= $(progs_ldadd)

buffered_input_stream_SOURCES	= buffered-input-stream.c
buffered_input_stream_LDADD	= $(progs_ldadd)

buffered_output_stream_SOURCES	= buffered-output-stream.c
buffered_output_stream_LDADD	= $(progs_ldadd)

live_g_file_SOURCES	  = live-g-file.c
live_g_file_LDADD	  = $(progs_ldadd)

desktop_app_info_SOURCES  = desktop-app-info.c
desktop_app_info_LDADD	  = $(progs_ldadd)

unix_streams_SOURCES	  = unix-streams.c
unix_streams_LDADD	  = $(progs_ldadd) \
	$(top_builddir)/gthread/libgthread-2.0.la

win32_streams_SOURCES	  = win32-streams.c
win32_streams_LDADD	  = $(progs_ldadd) \
	$(top_builddir)/gthread/libgthread-2.0.la

unix_fd_SOURCES	  = unix-fd.c
unix_fd_LDADD	  = $(progs_ldadd)

simple_async_result_SOURCES	= simple-async-result.c
simple_async_result_LDADD	= $(progs_ldadd)

sleepy_stream_SOURCES		= sleepy-stream.c
sleepy_stream_LDADD		= $(progs_ldadd)

filter_streams_SOURCES		= filter-streams.c
filter_streams_LDADD		= $(progs_ldadd)

volumemonitor_SOURCES		= volumemonitor.c
volumemonitor_LDADD		= $(progs_ldadd)

resolver_SOURCES	  = resolver.c
resolver_LDADD		  = $(progs_ldadd) \
	$(top_builddir)/gthread/libgthread-2.0.la

socket_server_SOURCES	  = socket-server.c
socket_server_LDADD	  = $(progs_ldadd) \
	$(top_builddir)/gthread/libgthread-2.0.la

socket_client_SOURCES	  = socket-client.c \
                            gtlsconsoleinteraction.c \
                            gtlsconsoleinteraction.h
socket_client_LDADD	  = $(progs_ldadd) \
	$(top_builddir)/gthread/libgthread-2.0.la

echo_server_SOURCES	  = echo-server.c
echo_server_LDADD	  = $(progs_ldadd) \
	$(top_builddir)/gthread/libgthread-2.0.la

httpd_SOURCES		  = httpd.c
httpd_LDADD		  = $(progs_ldadd) \
	$(top_builddir)/gthread/libgthread-2.0.la

send_data_SOURCES	  = send-data.c
send_data_LDADD		  = $(progs_ldadd) \
	$(top_builddir)/gthread/libgthread-2.0.la

srvtarget_SOURCES	  = srvtarget.c
srvtarget_LDADD		  = $(progs_ldadd)

network_address_SOURCE	  = network-address.c
network_address_LDADD	  = $(progs_ldadd)

socket_SOURCE		  = socket.c
socket_LDADD		  = $(progs_ldadd)

pollable_SOURCE		  = pollable.c
pollable_LDADD		  = $(progs_ldadd)

contexts_SOURCES	  = contexts.c
contexts_LDADD		  = $(progs_ldadd) \
	$(top_builddir)/gthread/libgthread-2.0.la

gsettings_SOURCES	  = gsettings.c
gsettings_LDADD		  = $(progs_ldadd)

gschema_compile_SOURCES	  = gschema-compile.c
gschema_compile_LDADD	  = $(progs_ldadd)

if HAVE_DBUS1
TEST_PROGS += gdbus-serialization
gdbus_serialization_SOURCES = gdbus-serialization.c gdbus-tests.h gdbus-tests.c
gdbus_serialization_CFLAGS = $(DBUS1_CFLAGS)
gdbus_serialization_LDADD = $(progs_ldadd) $(DBUS1_LIBS)
endif

gdbus_addresses_SOURCES = gdbus-addresses.c
gdbus_addresses_LDADD = $(progs_ldadd)

gdbus_bz627724_SOURCES = gdbus-bz627724.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c
gdbus_bz627724_LDADD = $(progs_ldadd)

gdbus_close_pending_SOURCES = gdbus-close-pending.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c
gdbus_close_pending_LDADD = $(progs_ldadd)

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
gdbus_test_codegen_LDADD = $(progs_ldadd)
endif # OS_UNIX

gdbus_connection_SOURCES = gdbus-connection.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c
gdbus_connection_LDADD = $(progs_ldadd)

gdbus_connection_flush_SOURCES = \
	gdbus-connection-flush.c \
	test-io-stream.c \
	test-io-stream.h \
	test-pipe-unix.c \
	test-pipe-unix.h \
	$(NULL)
gdbus_connection_flush_LDADD = $(progs_ldadd)

gdbus_connection_loss_SOURCES = gdbus-connection-loss.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c
gdbus_connection_loss_LDADD = $(progs_ldadd)

gdbus_connection_slow_SOURCES = gdbus-connection-slow.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c
gdbus_connection_slow_LDADD = $(progs_ldadd)

gdbus_message_SOURCES = gdbus-message.c
gdbus_message_LDADD = $(progs_ldadd)

gdbus_names_SOURCES = gdbus-names.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c
gdbus_names_LDADD = $(progs_ldadd)

gdbus_proxy_SOURCES = gdbus-proxy.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c
gdbus_proxy_LDADD = $(progs_ldadd)

gdbus_proxy_threads_SOURCES = gdbus-proxy-threads.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c
gdbus_proxy_threads_LDADD = $(progs_ldadd)

gdbus_proxy_well_known_name_SOURCES = gdbus-proxy-well-known-name.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c
gdbus_proxy_well_known_name_LDADD = $(progs_ldadd)

gdbus_introspection_SOURCES = gdbus-introspection.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c
gdbus_introspection_LDADD = $(progs_ldadd)

gdbus_threading_SOURCES = gdbus-threading.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c
gdbus_threading_LDADD = $(progs_ldadd)

gdbus_export_SOURCES = gdbus-export.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c
gdbus_export_LDADD = $(progs_ldadd)

gdbus_error_SOURCES = gdbus-error.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c
gdbus_error_LDADD = $(progs_ldadd)

gdbus_non_socket_SOURCES = \
	gdbus-non-socket.c \
	gdbus-tests.c \
	gdbus-tests.h \
	test-io-stream.c \
	test-io-stream.h \
	test-pipe-unix.c \
	test-pipe-unix.h \
	$(NULL)
gdbus_non_socket_LDADD = $(progs_ldadd)

gdbus_exit_on_close_SOURCES = gdbus-exit-on-close.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c
gdbus_exit_on_close_LDADD = $(progs_ldadd)

gdbus_example_watch_name_SOURCES = gdbus-example-watch-name.c
gdbus_example_watch_name_LDADD   = $(progs_ldadd)

gdbus_example_watch_proxy_SOURCES = gdbus-example-watch-proxy.c
gdbus_example_watch_proxy_LDADD   = $(progs_ldadd)

gdbus_example_own_name_SOURCES = gdbus-example-own-name.c
gdbus_example_own_name_LDADD   = $(progs_ldadd)

gdbus_example_server_SOURCES = gdbus-example-server.c
gdbus_example_server_LDADD   = $(progs_ldadd)

gdbus_example_unix_fd_client_SOURCES = gdbus-example-unix-fd-client.c
gdbus_example_unix_fd_client_LDADD   = $(progs_ldadd)

gdbus_example_subtree_SOURCES = gdbus-example-subtree.c
gdbus_example_subtree_LDADD   = $(progs_ldadd)

gdbus_example_peer_SOURCES = gdbus-example-peer.c
gdbus_example_peer_LDADD   = $(progs_ldadd)

gdbus_example_proxy_subclass_SOURCES = gdbus-example-proxy-subclass.c
gdbus_example_proxy_subclass_LDADD   = $(progs_ldadd)

gdbus_example_export_SOURCES = gdbus-example-export.c
gdbus_example_export_LDADD   = $(progs_ldadd)

gdbus_connection_flush_helper_SOURCES = gdbus-connection-flush-helper.c
gdbus_connection_flush_helper_LDADD = $(progs_ldadd)

appinfo_SOURCES = appinfo.c
appinfo_LDADD   = $(progs_ldadd)

resources_SOURCES = resources.c test_resources.c test_resources2.c test_resources2.h
resources_DEPENDENCIES = test.gresource
resources_LDADD   = $(progs_ldadd)

appinfo_test_SOURCES = appinfo-test.c
appinfo_test_LDADD   = $(progs_ldadd)

contenttype_SOURCES = contenttype.c
contenttype_LDADD   = $(progs_ldadd)

mimeapps_SOURCES = mimeapps.c
mimeapps_LDADD   = $(progs_ldadd)

file_SOURCES = file.c
file_LDADD   = $(progs_ldadd)

fileattributematcher_SOURCES = fileattributematcher.c
fileattributematcher_LDADD   = $(progs_ldadd)

gapplication_SOURCES = gapplication.c gdbus-sessionbus.h gdbus-sessionbus.c gdbus-tests.h gdbus-tests.c
gapplication_LDADD = $(progs_ldadd)

basic_application_LDADD = $(progs_ldadd)

gapplication_example_open_SOURCES = gapplication-example-open.c
gapplication_example_open_LDADD   = $(progs_ldadd)

gapplication_example_cmdline_SOURCES = gapplication-example-cmdline.c
gapplication_example_cmdline_LDADD   = $(progs_ldadd)

gapplication_example_cmdline2_SOURCES = gapplication-example-cmdline2.c
gapplication_example_cmdline2_LDADD   = $(progs_ldadd)

gapplication_example_cmdline3_SOURCES = gapplication-example-cmdline3.c
gapplication_example_cmdline3_LDADD   = $(progs_ldadd)

gapplication_example_actions_SOURCES = gapplication-example-actions.c
gapplication_example_actions_LDADD   = $(progs_ldadd)

gmenumodel_SOURCES = gmenumodel.c gdbus-sessionbus.h gdbus-sessionbus.c
gmenumodel_LDADD = $(progs_ldadd)

schema_tests = \
	schema-tests/array-default-not-in-choices.gschema.xml		\
	schema-tests/bad-choice.gschema.xml				\
	schema-tests/bad-key.gschema.xml				\
	schema-tests/bad-key2.gschema.xml				\
	schema-tests/bad-key3.gschema.xml				\
	schema-tests/bad-key4.gschema.xml				\
	schema-tests/bad-type.gschema.xml				\
	schema-tests/bare-alias.gschema.xml				\
	schema-tests/choice-alias.gschema.xml				\
	schema-tests/choice-bad.gschema.xml				\
	schema-tests/choice-badtype.gschema.xml				\
	schema-tests/choice-invalid-alias.gschema.xml			\
	schema-tests/choice-missing-value.gschema.xml			\
	schema-tests/choice-shadowed-alias.gschema.xml			\
	schema-tests/choice-upside-down.gschema.xml			\
	schema-tests/choice.gschema.xml					\
	schema-tests/choices-wrong-type.gschema.xml			\
	schema-tests/default-in-aliases.gschema.xml			\
	schema-tests/default-not-in-choices.gschema.xml			\
	schema-tests/default-out-of-range.gschema.xml			\
	schema-tests/empty-key.gschema.xml				\
	schema-tests/enum-with-aliases.gschema.xml			\
	schema-tests/enum-with-bad-default.gschema.xml			\
	schema-tests/enum-with-chained-alias.gschema.xml		\
	schema-tests/enum-with-choice.gschema.xml			\
	schema-tests/enum-with-invalid-alias.gschema.xml		\
	schema-tests/enum-with-repeated-alias.gschema.xml		\
	schema-tests/enum-with-repeated-nick.gschema.xml		\
	schema-tests/enum-with-repeated-value.gschema.xml		\
	schema-tests/enum-with-shadow-alias.gschema.xml			\
	schema-tests/enum.gschema.xml					\
	schema-tests/flags-aliased-default.gschema.xml			\
	schema-tests/flags-bad-default.gschema.xml			\
	schema-tests/flags-more-than-one-bit.gschema.xml		\
	schema-tests/flags-with-enum-attr.gschema.xml			\
	schema-tests/flags-with-enum-tag.gschema.xml			\
	schema-tests/extend-and-shadow-indirect.gschema.xml		\
	schema-tests/extend-and-shadow.gschema.xml			\
	schema-tests/extend-missing.gschema.xml				\
	schema-tests/extend-nonlist.gschema.xml				\
	schema-tests/extend-self.gschema.xml				\
	schema-tests/extend-wrong-list-indirect.gschema.xml		\
	schema-tests/extend-wrong-list.gschema.xml			\
	schema-tests/extending.gschema.xml				\
	schema-tests/from-docs.gschema.xml				\
	schema-tests/incomplete-list.gschema.xml			\
	schema-tests/inherit-gettext-domain.gschema.xml			\
	schema-tests/invalid-path.gschema.xml				\
	schema-tests/key-in-list-indirect.gschema.xml			\
	schema-tests/key-in-list.gschema.xml				\
	schema-tests/list-of-missing.gschema.xml			\
	schema-tests/missing-quotes.gschema.xml				\
	schema-tests/no-default.gschema.xml				\
	schema-tests/overflow.gschema.xml				\
	schema-tests/override-missing.gschema.xml			\
	schema-tests/override-range-error.gschema.xml			\
	schema-tests/override-then-key.gschema.xml			\
	schema-tests/override-twice.gschema.xml				\
	schema-tests/override-type-error.gschema.xml			\
	schema-tests/override.gschema.xml				\
	schema-tests/range-badtype.gschema.xml				\
	schema-tests/range-default-high.gschema.xml			\
	schema-tests/range-default-low.gschema.xml			\
	schema-tests/range-high-default.gschema.xml			\
	schema-tests/range-low-default.gschema.xml			\
	schema-tests/range-missing-max.gschema.xml			\
	schema-tests/range-missing-min.gschema.xml			\
	schema-tests/range-parse-error.gschema.xml			\
	schema-tests/range-wrong-type.gschema.xml			\
	schema-tests/range.gschema.xml					\
	schema-tests/wrong-category.gschema.xml

proxy_SOURCES = proxy.c
proxy_LDADD   = $(progs_ldadd) \
	$(top_builddir)/gthread/libgthread-2.0.la

tls_certificate_SOURCES = tls-certificate.c gtesttlsbackend.c gtesttlsbackend.h
tls_certificate_LDADD   = $(progs_ldadd)

tls_interaction_LDADD = $(progs_ldadd)

cancellable_LDADD = $(progs_ldadd)

vfs_LDADD = $(progs_ldadd)

network_monitor_LDADD = $(progs_ldadd)

# -----------------------------------------------------------------------------

if OS_UNIX
gdbus_example_objectmanager_server_SOURCES  = gdbus-example-objectmanager-server.c
gdbus_example_objectmanager_server_CFLAGS   = -I$(top_builddir)/gio/tests/gdbus-object-manager-example
gdbus_example_objectmanager_server_LDADD    = $(top_builddir)/gio/tests/gdbus-object-manager-example/libgdbus-example-objectmanager.la $(progs_ldadd)

gdbus_example_objectmanager_client_SOURCES  = gdbus-example-objectmanager-client.c
gdbus_example_objectmanager_client_CFLAGS   = -I$(top_builddir)/gio/tests/gdbus-object-manager-example
gdbus_example_objectmanager_client_LDADD    = $(top_builddir)/gio/tests/gdbus-object-manager-example/libgdbus-example-objectmanager.la $(progs_ldadd)

gdbus_peer_SOURCES = gdbus-peer.c gdbus-tests.h gdbus-tests.c
gdbus_peer_CFLAGS  = -I$(top_builddir)/gio/tests/gdbus-object-manager-example
gdbus_peer_LDADD   = $(top_builddir)/gio/tests/gdbus-object-manager-example/libgdbus-example-objectmanager.la $(progs_ldadd)

endif OS_UNIX

# -----------------------------------------------------------------------------

EXTRA_DIST += \
	socket-common.c			\
	org.gtk.test.gschema		\
	org.gtk.test.gschema.xml	\
	testenum.h			\
	enums.xml.template		\
	de.po				\
	$(schema_tests)			\
	org.gtk.schemasourcecheck.gschema.xml \
	appinfo-test.desktop		\
	appinfo-test2.desktop		\
	appinfo-test-gnome.desktop	\
	appinfo-test-notgnome.desktop	\
	gdbus-testserver.py		\
	test-codegen.xml		\
	cert1.pem			\
	cert2.pem			\
	cert3.pem			\
	key.pem				\
	key8.pem			\
	key-cert.pem			\
	cert-key.pem			\
	cert-list.pem			\
	nothing.pem			\
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
test_resources.c: test2.gresource.xml Makefile $(shell $(glib_compile_resources) --generate-dependencies $(srcdir)/test2.gresource.xml)
	$(AM_V_GEN) $(glib_compile_resources) --target=$@ --sourcedir=$(srcdir) --generate-source --c-name _g_test1 $<

test_resources2.h test_resources2.c: test3.gresource.xml Makefile $(shell $(glib_compile_resources) --generate-dependencies $(srcdir)/test3.gresource.xml)
	$(AM_V_GEN) $(glib_compile_resources) --target=$@ --sourcedir=$(srcdir) --generate --c-name _g_test2 --manual-register $<

plugin_resources.c: test4.gresource.xml Makefile $(shell $(glib_compile_resources) --generate-dependencies $(srcdir)/test4.gresource.xml)
	$(AM_V_GEN) $(glib_compile_resources) --target=$@ --sourcedir=$(srcdir) --generate-source --c-name _g_plugin $<

test.gresource: test.gresource.xml Makefile $(shell $(glib_compile_resources) --generate-dependencies $(srcdir)/test.gresource.xml)
	$(AM_V_GEN) $(glib_compile_resources) --target=$@ --sourcedir=$(srcdir) $<

noinst_LTLIBRARIES = libresourceplugin.la

libresourceplugin_la_SOURCES = resourceplugin.c plugin_resources.c
libresourceplugin_la_LDFLAGS = -avoid-version -module $(no_undefined) -rpath $(libdir)
libresourceplugin_la_LIBADD = $(progs_ldadd)

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
