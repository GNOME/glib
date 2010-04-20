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
	gschema-compile

SAMPLE_PROGS = 			\
	resolver		\
	socket-server		\
	socket-client		\
	echo-server		\
	httpd			\
	send-data		\
	filter-cat

if OS_UNIX
TEST_PROGS += live-g-file desktop-app-info unix-fd #unix-streams
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

EXTRA_DIST +=                                     \
	socket-common.c                           \
	org.gtk.test.gschema                      \
	org.gtk.test.gschema.xml                  \
	de.po                                     \
	schema-tests/bad-type.gschema.xml         \
	schema-tests/incomplete-list.gschema.xml  \
	schema-tests/missing-quotes.gschema.xml   \
	schema-tests/no-default.gschema.xml       \
	schema-tests/wrong-category.gschema.xml   \
	schema-tests/overflow.gschema.xml         \
	schema-tests/bad-key.gschema.xml          \
	schema-tests/bad-key2.gschema.xml         \
	schema-tests/bad-key3.gschema.xml         \
	schema-tests/bad-key4.gschema.xml         \
	schema-tests/empty-key.gschema.xml

MISC_STUFF = gschemas.compiled test.mo

test.mo: de.po
	$(MSGFMT) -o test.mo $(srcdir)/de.po; \
	$(MKDIR_P) de/LC_MESSAGES; \
	cp -f test.mo de/LC_MESSAGES

gschemas.compiled:
	$(top_builddir)/gio/gschema-compile --targetdir=$(abs_builddir) $(srcdir)

DISTCLEANFILES =                                        \
	applications/mimeinfo.cache                     \
	de/LC_MESSAGES/test.mo                          \
	test.mo                                         \
	gsettings.store                                 \
	gschemas.compiled
