include $(top_srcdir)/Makefile.decl

INCLUDES = 			\
	-g 			\
	-I$(top_srcdir) 	\
	-I$(top_srcdir)/glib 	\
	-I$(top_srcdir)/gmodule	\
	-I$(top_srcdir)/gobject	\
	-I$(top_srcdir)/gio 	\
	$(GLIB_DEBUG_FLAGS)

noinst_PROGRAMS = $(TEST_PROGS)
progs_ldadd     = 					\
	$(top_builddir)/glib/libglib-2.0.la 		\
	$(top_builddir)/gobject/libgobject-2.0.la	\
	$(top_builddir)/gio/libgio-2.0.la


TEST_PROGS +=	 		\
	memory-input-stream 	\
	memory-output-stream 	\
	g-file 			\
	g-file-info 		\
	data-input-stream 	\
	data-output-stream 	\
	g-icon			\
	buffered-input-stream	\
	sleepy-stream		\
	filter-streams		\
	simple-async-result

if OS_UNIX
TEST_PROGS += live-g-file unix-streams desktop-app-info
endif

memory_input_stream_SOURCES	  = memory-input-stream.c
memory_input_stream_LDADD	  = $(progs_ldadd)

memory_output_stream_SOURCES	  = memory-output-stream.c
memory_output_stream_LDADD	  = $(progs_ldadd)

g_file_SOURCES	  = g-file.c
g_file_LDADD	  = $(progs_ldadd)

g_file_info_SOURCES	= g-file-info.c
g_file_info_LDADD	= $(progs_ldadd)

data_input_stream_SOURCES	= data-input-stream.c
data_input_stream_LDADD		= $(progs_ldadd)

data_output_stream_SOURCES	= data-output-stream.c
data_output_stream_LDADD	= $(progs_ldadd)

g_icon_SOURCES	= g-icon.c
g_icon_LDADD	= $(progs_ldadd)

buffered_input_stream_SOURCES	= buffered-input-stream.c
buffered_input_stream_LDADD	= $(progs_ldadd)

live_g_file_SOURCES	  = live-g-file.c
live_g_file_LDADD	  = $(progs_ldadd)

desktop_app_info_SOURCES  = desktop-app-info.c
desktop_app_info_LDADD	  = $(progs_ldadd)

unix_streams_SOURCES	  = unix-streams.c
unix_streams_LDADD	  = $(progs_ldadd) \
	$(top_builddir)/gthread/libgthread-2.0.la

simple_async_result_SOURCES	= simple-async-result.c
simple_async_result_LDADD	= $(progs_ldadd)

sleepy_stream_SOURCES		= sleepy-stream.c
sleepy_stream_LDADD		= $(progs_ldadd)

filter_streams_SOURCES		= filter-streams.c
filter_streams_LDADD		= $(progs_ldadd)

DISTCLEAN_FILES = applications/mimeinfo.cache
