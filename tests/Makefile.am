
INCLUDES = -I$(top_srcdir) @GLIB_DEBUG_FLAGS@

EFENCE=

EXTRA_DIST = \
	$(test_scripts)		\
	makefile.msc.in		\
	makefile.mingw.in

BUILT_EXTRA_DIST = \
	makefile.msc	\
	makefile.mingw

test_programs = \
	array-test	\
	date-test	\
	dirname-test	\
	gio-test	\
	hash-test	\
	list-test	\
	mainloop-test	\
	node-test	\
	queue-test	\
	rand-test	\
	relation-test	\
	shell-test	\
	slist-test	\
	spawn-test	\
	strfunc-test	\
	string-test	\
	thread-test	\
	threadpool-test \
	tree-test	\
	type-test

test_scripts = run-markup-tests.sh

test_script_support_programs = markup-test

check_PROGRAMS = $(test_programs) $(test_script_support_programs)

TESTS = $(test_programs) $(test_scripts)
TESTS_ENVIRONMENT = srcdir=$(srcdir)

progs_LDADD = $(EFENCE) $(top_builddir)/libglib-1.3.la $(EFENCE)
thread_LDADD = $(progs_LDADD) $(top_builddir)/gthread/libgthread-1.3.la @G_THREAD_LIBS@

array_test_LDADD = $(progs_LDADD)
date_test_LDADD = $(progs_LDADD)
dirname_test_LDADD = $(progs_LDADD)
gio_test_LDADD = $(progs_LDADD)
hash_test_LDADD = $(progs_LDADD)
list_test_LDADD = $(progs_LDADD)
mainloop_test_LDADD = $(thread_LDADD)
markup_test_LDADD = $(progs_LDADD)
node_test_LDADD = $(progs_LDADD)
queue_test_LDADD = $(progs_LDADD)
rand_test_LDADD = $(progs_LDADD)
relation_test_LDADD = $(progs_LDADD)
shell_test_LDADD = $(progs_LDADD)
slist_test_LDADD = $(progs_LDADD)
spawn_test_LDADD = $(progs_LDADD)
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

dist-hook: $(BUILT_EXTRA_DIST)
	files='$(BUILT_EXTRA_DIST)'; \
	for f in $$files; do \
	  if test -f $$f; then d=.; else d=$(srcdir); fi; \
	  cp $$d/$$f $(distdir) || exit 1; done
	mkdir $(distdir)/markups;          \
	for f in $(srcdir)/markups/* ; do  \
	  cp $$f $(distdir)/markups;       \
	done
