
INCLUDES = -I$(top_srcdir) @GLIB_DEBUG_FLAGS@

EXTRA_DIST = \
	makefile.msc	\
	makefile.msc.in	\
	makefile.mingw	\
	makefile.mingw.in

TESTS = \
	array-test	\
	date-test	\
	dirname-test	\
	gio-test	\
	hash-test	\
	list-test	\
	node-test	\
	queue-test	\
	rand-test	\
	relation-test	\
	slist-test	\
	strfunc-test	\
	string-test	\
	thread-test	\
	threadpool-test \
	tree-test	\
	type-test

noinst_PROGRAMS = $(TESTS)

progs_LDADD = $(top_builddir)/libglib-1.3.la
thread_LDADD = $(progs_LDADD) $(top_builddir)/gthread/libgthread-1.3.la @G_THREAD_LIBS@

array_test_LDADD = $(progs_LDADD)
date_test_LDADD = $(progs_LDADD)
dirname_test_LDADD = $(progs_LDADD)
gio_test_LDADD = $(progs_LDADD)
hash_test_LDADD = $(progs_LDADD)
list_test_LDADD = $(progs_LDADD)
node_test_LDADD = $(progs_LDADD)
queue_test_LDADD = $(progs_LDADD)
rand_test_LDADD = $(progs_LDADD)
relation_test_LDADD = $(progs_LDADD)
slist_test_LDADD = $(progs_LDADD)
strfunc_test_LDADD = $(progs_LDADD)
string_test_LDADD = $(progs_LDADD)
thread_test_LDADD = $(thread_LDADD)
threadpool_test_LDADD = $(thread_LDADD)
tree_test_LDADD = $(progs_LDADD)
type_test_LDADD = $(progs_LDADD)

makefile.msc: $(top_builddir)/config.status $(top_srcdir)/tests/makefile.msc.in
	cd $(top_builddir) && CONFIG_FILES=tests/$@ CONFIG_HEADERS= $(SHELL) ./config.status

makefile.mingw: $(top_builddir)/config.status $(top_srcdir)/tests/makefile.mingw.in
	cd $(top_builddir) && CONFIG_FILES=tests/$@ CONFIG_HEADERS= $(SHELL) ./config.status

