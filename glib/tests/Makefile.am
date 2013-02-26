include $(top_srcdir)/Makefile.decl

AM_CPPFLAGS = 				\
	-g 				\
	$(glib_INCLUDES) 		\
	-DG_LOG_DOMAIN=\"GLib\" 	\
	-DSRCDIR=\""$(srcdir)"\"	\
	$(GLIB_DEBUG_FLAGS)

AM_CFLAGS = $(GLIB_WARN_CFLAGS)

LDADD = $(top_builddir)/glib/libglib-2.0.la -lm

TEST_PROGS += 				\
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
	gwakeup				\
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
	1bit-emufutex			\
	642026				\
	642026-ec

noinst_PROGRAMS = $(TEST_PROGS) test-spawn-echo

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

TEST_PROGS += unix
TEST_PROGS += include

# some testing of gtester functionality
XMLLINT = xmllint

gtester-xmllint-check: # check testreport xml with xmllint if present
	${GTESTER} -k --quiet -o tmpsample.xml --test-arg=--gtester-selftest ${GTESTER}
	${XMLLINT} --version 2>/dev/null; test "$$?" != 0 || ${XMLLINT} --noout tmpsample.xml

check-am: gtester-xmllint-check

private_LDFLAGS = -pthread

endif

CLEANFILES = \
	tmpsample.xml

EXTRA_DIST += \
	4096-random-bytes	\
	keyfiletest.ini		\
	pages.ini		\
	bookmarks.xbel		\
	empty			\
	echo-script

dist-hook:
	mkdir $(distdir)/markups;		\
	for f in $(srcdir)/markups/*; do 	\
	  cp $$f $(distdir)/markups; done
	mkdir $(distdir)/bookmarks;		\
	for f in $(srcdir)/bookmarks/* ; do 	\
	  cp $$f $(distdir)/bookmarks; done
