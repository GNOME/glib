include $(top_srcdir)/Makefile.decl

SUBDIRS=gobject refcount

AM_CPPFLAGS = 				\
	-I$(top_srcdir) 		\
	-I$(top_srcdir)/glib 		\
	-I$(top_srcdir)/gmodule 	\
	$(GLIB_DEBUG_FLAGS)

AM_CFLAGS = -g

EFENCE=

libglib = $(top_builddir)/glib/libglib-2.0.la
libgthread = $(top_builddir)/gthread/libgthread-2.0.la
libgmodule = $(top_builddir)/gmodule/libgmodule-2.0.la
libgobject = $(top_builddir)/gobject/libgobject-2.0.la

libadd_libgmodule = $(libgmodule)
libadd_libglib = $(libglib)
if PLATFORM_WIN32
no_undefined = -no-undefined

module_test_exp = module-test.exp

module-test.exp: module-test.o
	dlltool --output-exp module-test.exp module-test.o

spawn_test_win32_gui = spawn-test-win32-gui

spawn_test_win32_gui_LDFLAGS = -mwindows

endif

EXTRA_DIST +=					\
	$(test_scripts)				\
	makefile.msc.in				\
	casefold.txt				\
	casemap.txt				\
	gen-casefold-txt.pl			\
	gen-casemap-txt.pl			\
	iochannel-test-infile			\
	utf8.txt				\
	timeloop-basic.c		

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
noinst_PROGRAMS = $(TEST_PROGS)	\
	testgdate 		\
	testgdateparser 	\
	unicode-normalize 	\
	unicode-collate 	\
	$(timeloop) 		\
	errorcheck-mutex-test	\
	assert-msg-test

TEST_PROGS              += scannerapi
scannerapi_SOURCES       = scannerapi.c
scannerapi_LDADD         = $(progs_ldadd)
TEST_PROGS		+= testglib
testglib_SOURCES         = testglib.c
testglib_LDADD		 = $(libglib)
TEST_PROGS              += testingbase64
testingbase64_SOURCES    = testingbase64.c
testingbase64_LDADD      = $(progs_ldadd)


patterntest_LDADD = $(libglib)
testgdate_LDADD = $(libglib)
testgdateparser_LDADD = $(libglib)
unicode_normalize_LDADD = $(libglib)
errorcheck_mutex_test_LDADD = $(libglib) $(libgthread) $(G_THREAD_LIBS) 
assert_msg_test_LDADD = $(libglib)
if ENABLE_TIMELOOP
timeloop_LDADD = $(libglib)
timeloop_closure_LDADD = $(libglib) $(libgobject)
endif

test_programs =					\
	atomic-test				\
	base64-test				\
	bit-test				\
	$(CXX_TEST)				\
	checksum-test				\
	child-test				\
	completion-test				\
	convert-test				\
	date-test				\
	dirname-test				\
	file-test				\
	env-test				\
	gio-test				\
	hash-test				\
	iochannel-test				\
	list-test				\
	mainloop-test				\
	mapping-test				\
	markup-collect				\
	markup-escape-test			\
	module-test				\
	node-test				\
	onceinit				\
	patterntest				\
	queue-test				\
	asyncqueue-test				\
	qsort-test				\
	relation-test				\
	sequence-test				\
	shell-test				\
	slist-test				\
	slice-test				\
	slice-color				\
	slice-concurrent			\
	slice-threadinit			\
	spawn-test				\
	$(spawn_test_win32_gui)			\
	thread-test				\
	threadpool-test				\
	tree-test				\
	type-test				\
	unicode-caseconv			\
	unicode-encoding			\
	utf8-validate				\
	utf8-pointer				\
	uri-test				\
	regex-test

test_scripts = run-markup-tests.sh run-collate-tests.sh run-bookmark-test.sh run-assert-msg-test.sh

test_script_support_programs = markup-test unicode-collate bookmarkfile-test

check_PROGRAMS = $(test_programs) $(test_script_support_programs)

TESTS = $(test_programs) $(test_scripts)
TESTS_ENVIRONMENT = srcdir=$(srcdir) \
	LIBCHARSET_ALIAS_DIR=$(top_builddir)/glib/libcharset \
	MALLOC_CHECK_=2	\
        MALLOC_PERTURB_=$$(($${RANDOM:-256} % 256))

progs_ldadd = $(EFENCE) $(libglib) $(EFENCE)
thread_ldadd = $(libgthread) $(G_THREAD_LIBS) $(progs_ldadd)
module_ldadd = $(libgmodule) $(G_MODULE_LIBS) $(progs_ldadd)

atomic_test_LDADD = $(progs_ldadd)
base64_test_LDADD = $(progs_ldadd)
bit_test_LDADD = $(progs_ldadd)
bookmarkfile_test_LDADD = $(progs_ldadd)
checksum_test_LDADD = $(progs_ldadd)
child_test_LDADD = $(thread_ldadd)
completion_test_LDADD = $(progs_ldadd)
convert_test_LDADD = $(progs_ldadd)
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
mapping_test_LDADD = $(progs_ldadd)
markup_escape_test_LDADD = $(progs_ldadd)
module_test_LDADD = $(module_ldadd) $(module_test_exp)
module_test_LDFLAGS = $(G_MODULE_LDFLAGS)
node_test_LDADD = $(progs_ldadd)
onceinit_LDADD = $(thread_ldadd)
queue_test_LDADD = $(progs_ldadd)
asyncqueue_test_LDADD = $(thread_ldadd)
qsort_test_LDADD = $(progs_ldadd)
relation_test_LDADD = $(progs_ldadd)
sequence_test_LDADD = $(progs_ldadd)
shell_test_LDADD = $(progs_ldadd)
slist_test_LDADD = $(progs_ldadd)
slice_test_SOURCES = slice-test.c memchunks.c
slice_test_LDADD = $(thread_ldadd)
slice_color_SOURCES = slice-color.c memchunks.c
slice_color_LDADD = $(thread_ldadd)
slice_concurrent_SOURCES = slice-concurrent.c
slice_concurrent_LDADD = $(thread_ldadd)
slice_threadinit_SOURCES = slice-threadinit.c
slice_threadinit_LDADD = $(thread_ldadd)
spawn_test_LDADD = $(progs_ldadd)
thread_test_LDADD = $(thread_ldadd)
threadpool_test_LDADD = $(thread_ldadd)
tree_test_LDADD = $(progs_ldadd)
type_test_LDADD = $(progs_ldadd)
unicode_encoding_LDADD = $(progs_ldadd)
unicode_caseconv_LDADD = $(progs_ldadd)
unicode_collate_LDADD = $(progs_ldadd)
utf8_validate_LDADD = $(progs_ldadd)
utf8_pointer_LDADD = $(progs_ldadd)
uri_test_LDADD = $(progs_ldadd)
regex_test_LDADD = $(progs_ldadd)
markup_collect_LDADD = $(progs_ldadd)

noinst_LTLIBRARIES = libmoduletestplugin_a.la libmoduletestplugin_b.la

libmoduletestplugin_a_la_SOURCES = libmoduletestplugin_a.c
libmoduletestplugin_a_la_LDFLAGS = $(G_MODULE_LDFLAGS) -avoid-version -module $(no_undefined) -rpath $(libdir)
libmoduletestplugin_a_la_LIBADD = $(G_MODULE_LIBS) $(libadd_libgmodule) $(libadd_libglib)

libmoduletestplugin_b_la_SOURCES = libmoduletestplugin_b.c
libmoduletestplugin_b_la_LDFLAGS = $(G_MODULE_LDFLAGS) -avoid-version -module $(no_undefined) -rpath $(libdir)
libmoduletestplugin_b_la_LIBADD = $(G_MODULE_LIBS) $(libadd_libgmodule) $(libadd_libglib)

dist-hook: $(BUILT_EXTRA_DIST)
	files='$(BUILT_EXTRA_DIST)'; 				\
	for f in $$files; do 					\
	  if test -f $$f; then d=.; else d=$(srcdir); fi; 	\
	  cp $$d/$$f $(distdir) || exit 1; done
	mkdir $(distdir)/markups;          			\
	for f in $(srcdir)/markups/* ; do  			\
	  cp $$f $(distdir)/markups; done 
	mkdir $(distdir)/collate;          			\
	for f in $(srcdir)/collate/* ; do  			\
          if test -f $$f; then cp $$f $(distdir)/collate; fi; done
	mkdir $(distdir)/bookmarks;          			\
	for f in $(srcdir)/bookmarks/* ; do  			\
	  cp $$f $(distdir)/bookmarks; done 

DISTCLEANFILES =		\
	iochannel-test-outfile 	\
	file-test-get-contents 	\
	maptest 		\
	mapchild 		\
	collate.out
