include $(top_srcdir)/Makefile.decl

INCLUDES = -g $(gthread_INCLUDES) $(GLIB_DEBUG_FLAGS)

noinst_PROGRAMS = $(TEST_PROGS) test-spawn-echo
progs_ldadd     = $(top_builddir)/glib/libglib-2.0.la \
		  $(top_builddir)/gthread/libgthread-2.0.la

test_spawn_echo_SOURCES = test-spawn-echo.c
test_spawn_echo_LDADD = $(progs_ldadd)

TEST_PROGS	 += 1bit-mutex
1bit_mutex_LDADD  = $(progs_ldadd) $(top_builddir)/gthread/libgthread-2.0.la

TEST_PROGS	 += 642026
642026_LDADD	  = $(progs_ldadd)

TEST_PROGS	 += 642026-ec
642026_ec_SOURCES = 642026.c
642026_ec_LDADD   = $(progs_ldadd)
642026_ec_CFLAGS  = -DG_ERRORCHECK_MUTEXES

TEST_PROGS	      += 1bit-emufutex
1bit_emufutex_SOURCES  = 1bit-mutex.c
1bit_emufutex_CFLAGS   = -DTEST_EMULATED_FUTEX
1bit_emufutex_LDADD    = $(progs_ldadd) $(top_builddir)/gthread/libgthread-2.0.la

if OS_UNIX
TEST_PROGS += unix-multithreaded
unix_multithreaded_SOURCES = $(top_srcdir)/glib/tests/unix.c
unix_multithreaded_CFLAGS   = -DTEST_THREADED
unix_multithreaded_LDADD    = $(progs_ldadd) $(top_builddir)/gthread/libgthread-2.0.la
endif

TEST_PROGS += atomic
atomic_SOURCES = atomic.c
atomic_LDADD = $(progs_ldadd) $(top_builddir)/gthread/libgthread-2.0.la

TEST_PROGS += spawn-multithreaded
spawn_multithreaded_SOURCES = spawn-multithreaded.c
spawn_multithreaded_LDADD    = $(progs_ldadd) $(top_builddir)/gthread/libgthread-2.0.la

TEST_PROGS += spawn-singlethread
spawn_singlethread_SOURCES = spawn-singlethread.c
spawn_singlethread_LDADD    = $(progs_ldadd) $(top_builddir)/gthread/libgthread-2.0.la

TEST_PROGS += gwakeup
gwakeup_SOURCES = gwakeuptest.c ../../glib/gwakeup.c
gwakeup_LDADD    = $(progs_ldadd) $(top_builddir)/gthread/libgthread-2.0.la

if HAVE_EVENTFD
TEST_PROGS += gwakeup-fallback
gwakeup_fallback_SOURCES = gwakeuptest.c ../../glib/gwakeup.c
gwakeup_fallback_CFLAGS = $(AM_CFLAGS) -DTEST_EVENTFD_FALLBACK
gwakeup_fallback_LDADD    = $(progs_ldadd) $(top_builddir)/gthread/libgthread-2.0.la
endif
