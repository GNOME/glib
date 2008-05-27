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


TEST_PROGS       += memory-input-stream g-file g-file-info data-input-stream data-output-stream

if OS_UNIX
TEST_PROGS       += live-g-file
endif

memory_input_stream_SOURCES	  = memory-input-stream.c
memory_input_stream_LDADD	  = $(progs_ldadd)

g_file_SOURCES	  = g-file.c
g_file_LDADD	  = $(progs_ldadd)

g_file_info_SOURCES	= g-file-info.c
g_file_info_LDADD	= $(progs_ldadd)

data_input_stream_SOURCES	= data-input-stream.c
data_input_stream_LDADD		= $(progs_ldadd)

data_output_stream_SOURCES	= data-output-stream.c
data_output_stream_LDADD	= $(progs_ldadd)

live_g_file_SOURCES	  = live-g-file.c
live_g_file_LDADD	  = $(progs_ldadd)
