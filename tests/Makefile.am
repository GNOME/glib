
INCLUDES = -I$(top_srcdir) @GLIB_DEBUG_FLAGS@

EXTRA_DIST = \
	makefile.msc	\
	makefile.cygwin

TESTS = \
	array-test	\
	date-test	\
	dirname-test	\
	hash-test	\
	list-test	\
	node-test	\
	queue-test	\
	rand-test	\
	relation-test	\
	slist-test	\
	stack-test	\
	string-test	\
	strfunc-test	\
	tree-test	\
	type-test	\
	thread-test

noinst_PROGRAMS = $(TESTS)

array_test_LDADD = $(top_builddir)/libglib.la
date_test_LDADD = $(top_builddir)/libglib.la
dirname_test_LDADD = $(top_builddir)/libglib.la
hash_test_LDADD = $(top_builddir)/libglib.la
list_test_LDADD = $(top_builddir)/libglib.la
node_test_LDADD = $(top_builddir)/libglib.la
queue_test_LDADD = $(top_builddir)/libglib.la
rand_test_LDADD = $(top_builddir)/libglib.la
relation_test_LDADD = $(top_builddir)/libglib.la
slist_test_LDADD = $(top_builddir)/libglib.la
stack_test_LDADD = $(top_builddir)/libglib.la
string_test_LDADD = $(top_builddir)/libglib.la
strfunc_test_LDADD = $(top_builddir)/libglib.la
tree_test_LDADD = $(top_builddir)/libglib.la
type_test_LDADD = $(top_builddir)/libglib.la
thread_test_LDADD = $(top_builddir)/libglib.la \
		$(top_builddir)/gthread/libgthread.la @G_THREAD_LIBS@

makefile.msc: $(top_builddir)/config.status $(top_srcdir)/tests/makefile.msc.in
	cd $(top_builddir) && CONFIG_FILES=tests/$@ CONFIG_HEADERS= $(SHELL) ./config.status

makefile.cygwin: $(top_builddir)/config.status $(top_srcdir)/tests/makefile.cygwin.in
	cd $(top_builddir) && CONFIG_FILES=tests/$@ CONFIG_HEADERS= $(SHELL) ./config.status
