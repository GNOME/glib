SUBDIRS=gobject

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/glib -I$(top_srcdir)/gmodule $(GLIB_DEBUG_FLAGS)

EFENCE=

libglib = $(top_builddir)/glib/libglib-2.0.la
libgthread = $(top_builddir)/gthread/libgthread-2.0.la
libgmodule = $(top_builddir)/gmodule/libgmodule-2.0.la
libgobject = $(top_builddir)/gobject/libgobject-2.0.la

if PLATFORM_WIN32
libadd_libgmodule = $(libgmodule)
libadd_libglib = $(libglib)
no_undefined = -no-undefined

module_test_exp = module-test.exp

module-test.exp: module-test.o
	dlltool --output-exp module-test.exp module-test.o

spawn_test_win32_gui = spawn-test-win32-gui

spawn_test_win32_gui_LDFLAGS = -mwindows

endif

EXTRA_DIST =					\
	$(test_scripts)				\
	makefile.msc.in				\
	casefold.txt				\
	casemap.txt				\
	gen-casefold-txt.pl			\
	gen-casemap-txt.pl			\
	iochannel-test-infile			\
	utf8.txt

BUILT_EXTRA_DIST =				\
	makefile.msc

if HAVE_CXX
CXX_TEST = cxx-test
cxx_test_LDADD = $(progs_ldadd)
cxx_test_SOURCES = cxx-test.C
else
CXX_TEST =
endif

if ENABLE_TIMELOOP
timeloop = timeloop timeloop-closure
endif
noinst_PROGRAMS = testglib testgdate testgdateparser unicode-normalize unicode-collate $(timeloop)
testglib_LDADD = $(libglib)
patterntest_LDADD = $(libglib)
testgdate_LDADD = $(libglib)
testgdateparser_LDADD = $(libglib)
unicode_normalize_LDADD = $(libglib)
unicode_collate_LDADD = $(libglib)
if ENABLE_TIMELOOP
timeloop_LDADD = $(libglib)
timeloop_closure_LDADD = $(libglib) $(libgobject)
endif

test_programs =					\
	atomic-test				\
	array-test				\
	$(CXX_TEST)				\
	child-test				\
	completion-test				\
	date-test				\
	dirname-test				\
	file-test				\
	env-test				\
	gio-test				\
	hash-test				\
	iochannel-test				\
	list-test				\
	mainloop-test				\
	markup-escape-test			\
	module-test				\
	node-test				\
	option-test				\
	patterntest				\
	printf-test				\
	queue-test				\
	qsort-test				\
	rand-test				\
	relation-test				\
	shell-test				\
	slist-test				\
	spawn-test				\
	$(spawn_test_win32_gui)			\
	strfunc-test				\
	string-test				\
	strtod-test				\
	thread-test				\
	threadpool-test				\
	tree-test				\
	type-test				\
	unicode-caseconv			\
	unicode-encoding			\
	uri-test

test_scripts = run-markup-tests.sh

test_script_support_programs = markup-test

check_PROGRAMS = $(test_programs) $(test_script_support_programs)

TESTS = $(test_programs) $(test_scripts)
TESTS_ENVIRONMENT = srcdir=$(srcdir) \
	LIBCHARSET_ALIAS_DIR=$(top_builddir)/glib/libcharset

progs_ldadd = $(EFENCE) $(libglib) $(EFENCE)
thread_ldadd = $(libgthread) $(G_THREAD_LIBS) $(progs_ldadd)
module_ldadd = $(libgmodule) $(G_MODULE_LIBS) $(progs_ldadd)

atomic_test_LDADD = $(progs_ldadd)
array_test_LDADD = $(progs_ldadd)
child_test_LDADD = $(thread_ldadd)
completion_test_LDADD = $(progs_ldadd)
date_test_LDADD = $(progs_ldadd)
dirname_test_LDADD = $(progs_ldadd)
file_test_LDADD = $(progs_ldadd)
env_test_LDADD = $(progs_ldadd)
gio_test_LDADD = $(progs_ldadd)
hash_test_LDADD = $(progs_ldadd)
iochannel_test_LDADD = $(progs_ldadd)
list_test_LDADD = $(progs_ldadd)
mainloop_test_LDADD = $(thread_ldadd)
markup_test_LDADD = $(progs_ldadd)
markup_escape_test_LDADD = $(progs_ldadd)
module_test_LDADD = $(module_ldadd) $(module_test_exp)
module_test_LDFLAGS = $(G_MODULE_LDFLAGS)
node_test_LDADD = $(progs_ldadd)
option_test_LDADD = $(progs_ldadd)
printf_test_LDADD = $(progs_ldadd)
queue_test_LDADD = $(progs_ldadd)
qsort_test_LDADD = $(progs_ldadd)
rand_test_LDADD = $(progs_ldadd)
relation_test_LDADD = $(progs_ldadd)
shell_test_LDADD = $(progs_ldadd)
slist_test_LDADD = $(progs_ldadd)
spawn_test_LDADD = $(progs_ldadd)
strfunc_test_LDADD = $(progs_ldadd)
string_test_LDADD = $(progs_ldadd)
strtod_test_LDADD = $(progs_ldadd) -lm
thread_test_LDADD = $(thread_ldadd)
threadpool_test_LDADD = $(thread_ldadd)
tree_test_LDADD = $(progs_ldadd)
type_test_LDADD = $(progs_ldadd)
unicode_encoding_LDADD = $(progs_ldadd)
unicode_caseconv_LDADD = $(progs_ldadd)
uri_test_LDADD = $(progs_ldadd)

lib_LTLIBRARIES = libmoduletestplugin_a.la libmoduletestplugin_b.la

# Prevent those libs from being installed
install-libLTLIBRARIES:
	:

libmoduletestplugin_a_la_SOURCES = libmoduletestplugin_a.c
libmoduletestplugin_a_la_LDFLAGS = $(G_MODULE_LDFLAGS) -avoid-version -module $(no_undefined) -rpath $(libdir)
libmoduletestplugin_a_la_LIBADD = $(G_MODULE_LIBS) $(libadd_libgmodule) $(libadd_libglib)

libmoduletestplugin_b_la_SOURCES = libmoduletestplugin_b.c
libmoduletestplugin_b_la_LDFLAGS = $(G_MODULE_LDFLAGS) -avoid-version -module $(no_undefined) -rpath $(libdir)
libmoduletestplugin_b_la_LIBADD = $(G_MODULE_LIBS) $(libadd_libgmodule) $(libadd_libglib)

dist-hook: $(BUILT_EXTRA_DIST)
	files='$(BUILT_EXTRA_DIST)'; \
	for f in $$files; do \
	  if test -f $$f; then d=.; else d=$(srcdir); fi; \
	  cp $$d/$$f $(distdir) || exit 1; done
	mkdir $(distdir)/markups;          \
	for f in $(srcdir)/markups/* ; do  \
	  cp $$f $(distdir)/markups;       \
	done

DISTCLEANFILES = iochannel-test-outfile
