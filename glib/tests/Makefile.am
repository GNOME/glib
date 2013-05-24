include $(top_srcdir)/Makefile.decl
NULL =

insttestdir=$(pkglibexecdir)/installed-tests

bookmark_test_files = \
	bookmarks/fail-01.xbel \
	bookmarks/fail-02.xbel \
	bookmarks/fail-03.xbel \
	bookmarks/fail-04.xbel \
	bookmarks/fail-05.xbel \
	bookmarks/fail-06.xbel \
	bookmarks/fail-07.xbel \
	bookmarks/fail-08.xbel \
	bookmarks/fail-09.xbel \
	bookmarks/fail-10.xbel \
	bookmarks/fail-11.xbel \
	bookmarks/fail-12.xbel \
	bookmarks/fail-13.xbel \
	bookmarks/fail-14.xbel \
	bookmarks/fail-15.xbel \
	bookmarks/fail-16.xbel \
	bookmarks/fail-17.xbel \
	bookmarks/valid-01.xbel \
	bookmarks/valid-02.xbel \
	bookmarks/valid-03.xbel \
	$(NULL)

markup_tests = \
	fail-1 fail-2 fail-3 fail-4 fail-5 \
	fail-6 fail-7 fail-8 fail-9 fail-10 \
	fail-11 fail-12 fail-13 fail-14 fail-15 \
	fail-16 fail-17 fail-18 fail-19 fail-20 \
	fail-21 fail-22 fail-23 fail-24 fail-25 \
	fail-26 fail-27 fail-28 fail-29 fail-30 \
	fail-31 fail-32 fail-33 fail-34 fail-35 \
	fail-36 fail-37 fail-38 fail-39 fail-40 \
	fail-41 fail-42 fail-43 fail-44 fail-45 \
	fail-46 fail-47 fail-48 fail-49 \
	valid-1 valid-2 valid-3 valid-4 valid-5 \
	valid-6 valid-7 valid-8 valid-9 valid-10 \
	valid-11 valid-12 valid-13 valid-14 valid-15 \
	$(NULL)

all_markup_test_files = $(addprefix markups/,$(markup_tests:=.gmarkup) $(markup_tests:=.expected))

test_files = \
	keyfiletest.ini		\
	pages.ini		\
	keyfile.c		\
	empty			\
	4096-random-bytes	\
	$(NULL)

test_script_files = \
	echo-script

AM_CPPFLAGS = 				\
	-g 				\
	$(glib_INCLUDES) 		\
	-DG_LOG_DOMAIN=\"GLib\" 	\
	-DSRCDIR=\""$(srcdir)"\"	\
	-DEXEEXT=\"$(EXEEXT)\"		\
	$(GLIB_DEBUG_FLAGS)

AM_CFLAGS = $(GLIB_WARN_CFLAGS)

LDADD = $(top_builddir)/glib/libglib-2.0.la -lm

TEST_PROGS +=				\
	1bit-emufutex			\
	gwakeup				\
	$(NULL)

all_test_programs = 			\
	array-test			\
	asyncqueue			\
	atomic				\
	base64				\
	bitlock				\
	bookmarkfile			\
	bytes				\
	cache				\
	checksum			\
	collate				\
	cond				\
	convert				\
	dataset				\
	date				\
	dir				\
	environment			\
	error				\
	fileutils			\
	gdatetime			\
	gvariant			\
	hash				\
	hmac				\
	hook				\
	hostutils			\
	keyfile				\
	list				\
	logging				\
	mainloop			\
	mappedfile			\
	markup-parse			\
	markup-collect			\
	markup-escape			\
	markup-subparser		\
	mem-overflow			\
	mutex				\
	node				\
	once				\
	option-context			\
	option-argv0			\
	pattern				\
	private				\
	protocol			\
	queue				\
	rand				\
	rec-mutex			\
	regex				\
	rwlock				\
	scannerapi			\
	sequence			\
	shell				\
	slice				\
	slist				\
	sort				\
	spawn-multithreaded		\
	spawn-singlethread		\
	strfuncs			\
	string				\
	testing 			\
	test-printf			\
	thread				\
	timeout				\
	tree				\
	utf8-performance		\
	utf8-pointer			\
	utf8-validate			\
	utf8-misc			\
	utils				\
	unicode				\
	uri				\
	1bit-mutex			\
	642026				\
	642026-ec

if OS_UNIX
all_test_programs += unix
all_test_programs += include
endif

extra_test_binaries = test-spawn-echo

if BUILD_MODULAR_TESTS
TEST_PROGS += $(all_test_programs)
noinst_PROGRAMS = $(TEST_PROGS) $(extra_test_binaries)
endif

if BUILDOPT_INSTALL_TESTS
insttest_PROGRAMS = $(all_test_programs) $(extra_test_binaries)
testmetadir = $(datadir)/installed-tests/$(PACKAGE)
testmeta_DATA = $(all_test_programs:=.test)

testdatadir=$(insttestdir)
testdata_DATA = $(test_files)

testdata_SCRIPTS = $(test_script_files)

bookmarksdir=$(insttestdir)/bookmarks
bookmarks_DATA = $(bookmark_test_files)

markupsdir=$(insttestdir)/markups
markups_DATA = $(all_markup_test_files)
endif

%.test: %$(EXEEXT) Makefile
	$(AM_V_GEN) (echo '[Test]' > $@.tmp; \
	 echo 'Type=session' >> $@.tmp; \
	 echo 'Exec=env G_TEST_DATA=$(pkglibexecdir)/installed-tests $(pkglibexecdir)/installed-tests/$<' >> $@.tmp; \
	 mv $@.tmp $@)

atomic_CFLAGS  = $(AM_CFLAGS)
if HAVE_GCC
atomic_CFLAGS += -Wstrict-aliasing=2
endif

642026_ec_SOURCES = 642026.c
642026_ec_CFLAGS = -DG_ERRORCHECK_MUTEXES

1bit_emufutex_SOURCES  = 1bit-mutex.c
1bit_emufutex_CFLAGS = $(AM_CFLAGS) -DTEST_EMULATED_FUTEX

gwakeup_SOURCES = gwakeuptest.c ../../glib/gwakeup.c

if HAVE_EVENTFD
TEST_PROGS += gwakeup-fallback
gwakeup_fallback_SOURCES = gwakeuptest.c ../../glib/gwakeup.c
gwakeup_fallback_CFLAGS = $(AM_CFLAGS) -DTEST_EVENTFD_FALLBACK
endif

if OS_UNIX

# some testing of gtester functionality
XMLLINT = xmllint

gtester-xmllint-check: # check testreport xml with xmllint if present
	${GTESTER} -k --quiet -o tmpsample.xml --test-arg=--gtester-selftest ${GTESTER}
	${XMLLINT} --version 2>/dev/null; test "$$?" != 0 || ${XMLLINT} --noout tmpsample.xml

check-am: gtester-xmllint-check

private_LDFLAGS = @G_THREAD_LIBS@

endif

CLEANFILES = \
	tmpsample.xml

EXTRA_DIST += \
	$(bookmark_test_files)	\
	$(all_markup_test_files) \
	$(test_files)		\
	$(test_script_files)	\
	bookmarks.xbel		\
	$(NULL)
