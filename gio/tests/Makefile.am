NULL =

include $(top_srcdir)/Makefile.decl

INCLUDES = 			\
	-g 			\
	-I$(top_srcdir) 	\
	-I$(top_srcdir)/glib 	\
	-I$(top_srcdir)/gmodule	\
	-I$(top_srcdir)/gobject	\
	-I$(top_srcdir)/gio 	\
	$(GLIB_DEBUG_FLAGS)	\
	-DSRCDIR=\""$(srcdir)"\"

noinst_PROGRAMS = $(TEST_PROGS) $(SAMPLE_PROGS)
noinst_DATA = $(MISC_STUFF)

progs_ldadd     = 					\
	$(top_builddir)/glib/libglib-2.0.la 		\
	$(top_builddir)/gobject/libgobject-2.0.la	\
	$(top_builddir)/gio/libgio-2.0.la

TEST_PROGS +=	 		\
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
	sleepy-stream		\
	filter-streams		\
	simple-async-result	\
	srvtarget		\
	contexts		\
	gsettings		\
	gschema-compile		\
	gdbus-addresses		\
	gdbus-connection	\
	gdbus-names		\
	gdbus-proxy		\
	gdbus-introspection	\
	gdbus-threading		\
	gdbus-export		\
	gdbus-error		\
	gdbus-peer		\
	gdbus-exit-on-close	\
	$(NULL)

SAMPLE_PROGS = 				\
	resolver			\
	socket-server			\
	socket-client			\
	echo-server			\
	httpd				\
	send-data			\
	filter-cat			\
	gdbus-example-own-name		\
	gdbus-example-watch-name	\
	gdbus-example-watch-proxy	\
	gdbus-example-server		\
	gdbus-example-subtree		\
	gdbus-example-peer		\
	$(NULL)


if OS_UNIX
TEST_PROGS += live-g-file desktop-app-info unix-fd #unix-streams
SAMPLE_PROGS +=	gdbus-example-unix-fd-client
endif

if OS_WIN32
TEST_PROGS += win32-streams
endif

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

filter_cat_SOURCES	= filter-cat.c
filter_cat_LDADD	= $(progs_ldadd)

g_icon_SOURCES	= g-icon.c
g_icon_LDADD	= $(progs_ldadd)

buffered_input_stream_SOURCES	= buffered-input-stream.c
buffered_input_stream_LDADD	= $(progs_ldadd)

live_g_file_SOURCES	  = live-g-file.c
live_g_file_LDADD	  = $(progs_ldadd)

desktop_app_info_SOURCES  = desktop-app-info.c
desktop_app_info_LDADD	  = $(progs_ldadd)

#unix_streams_SOURCES	  = unix-streams.c
#unix_streams_LDADD	  = $(progs_ldadd) \
#	$(top_builddir)/gthread/libgthread-2.0.la

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

resolver_SOURCES	  = resolver.c
resolver_LDADD		  = $(progs_ldadd) \
	$(top_builddir)/gthread/libgthread-2.0.la

socket_server_SOURCES	  = socket-server.c
socket_server_LDADD	  = $(progs_ldadd) \
	$(top_builddir)/gthread/libgthread-2.0.la

socket_client_SOURCES	  = socket-client.c
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

gdbus_connection_SOURCES = gdbus-connection.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c
gdbus_connection_LDADD = $(progs_ldadd)

gdbus_names_SOURCES = gdbus-names.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c
gdbus_names_LDADD = $(progs_ldadd)

gdbus_proxy_SOURCES = gdbus-proxy.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c
gdbus_proxy_LDADD = $(progs_ldadd)

gdbus_introspection_SOURCES = gdbus-introspection.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c
gdbus_introspection_LDADD = $(progs_ldadd)

gdbus_threading_SOURCES = gdbus-threading.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c
gdbus_threading_LDADD = $(progs_ldadd)

gdbus_export_SOURCES = gdbus-export.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c
gdbus_export_LDADD = $(progs_ldadd)

gdbus_error_SOURCES = gdbus-error.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c
gdbus_error_LDADD = $(progs_ldadd)

gdbus_peer_SOURCES = gdbus-peer.c gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c
gdbus_peer_LDADD = $(progs_ldadd)

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

EXTRA_DIST += \
	socket-common.c						\
	org.gtk.test.gschema					\
	org.gtk.test.gschema.xml				\
	de.po							\
	schema-tests/bad-type.gschema.xml			\
	schema-tests/incomplete-list.gschema.xml		\
	schema-tests/missing-quotes.gschema.xml			\
	schema-tests/no-default.gschema.xml			\
	schema-tests/wrong-category.gschema.xml			\
	schema-tests/overflow.gschema.xml			\
	schema-tests/bad-key.gschema.xml			\
	schema-tests/bad-key2.gschema.xml			\
	schema-tests/bad-key3.gschema.xml			\
	schema-tests/bad-key4.gschema.xml			\
	schema-tests/empty-key.gschema.xml			\
	schema-tests/array-default-not-in-choices.gschema.xml	\
	schema-tests/bad-type.gschema.xml			\
	schema-tests/choice-missing-value.gschema.xml		\
	schema-tests/choices-wrong-type.gschema.xml		\
	schema-tests/default-not-in-choices.gschema.xml		\
	schema-tests/default-out-of-range.gschema.xml		\
	schema-tests/incomplete-list.gschema.xml		\
	schema-tests/missing-quotes.gschema.xml			\
	schema-tests/no-default.gschema.xml			\
	schema-tests/overflow.gschema.xml			\
	schema-tests/range-missing-max.gschema.xml		\
	schema-tests/range-missing-min.gschema.xml		\
	schema-tests/range-wrong-type.gschema.xml		\
	schema-tests/wrong-category.gschema.xml
 
MISC_STUFF = gschemas.compiled test.mo

test.mo: de.po
	$(MSGFMT) -o test.mo $(srcdir)/de.po; \
	$(MKDIR_P) de/LC_MESSAGES; \
	cp -f test.mo de/LC_MESSAGES

gschemas.compiled:
	$(top_builddir)/gio/glib-compile-schemas --targetdir=$(abs_builddir) $(srcdir)

DISTCLEANFILES =                                        \
	applications/mimeinfo.cache                     \
	de/LC_MESSAGES/test.mo                          \
	test.mo                                         \
	gsettings.store                                 \
	gschemas.compiled
