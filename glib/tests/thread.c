/* Unit tests for GThread
 * Copyright (C) 2011 Red Hat, Inc
 * Author: Matthias Clasen
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In no event shall the authors or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 */

#include <config.h>

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <glib.h>

#ifndef G_OS_WIN32
#include <sys/resource.h>
#endif

static gpointer
thread1_func (gpointer data)
{
  g_thread_exit (GINT_TO_POINTER (1));

  g_assert_not_reached ();

  return NULL;
}

/* test that g_thread_exit() works */
static void
test_thread1 (void)
{
  gpointer result;
  GThread *thread;
  GError *error = NULL;

  thread = g_thread_new ("test", thread1_func, NULL, TRUE, &error);
  g_assert_no_error (error);

  result = g_thread_join (thread);

  g_assert_cmpint (GPOINTER_TO_INT (result), ==, 1);
}

static gpointer
thread2_func (gpointer data)
{
  return g_thread_self ();
}

/* test that g_thread_self() works */
static void
test_thread2 (void)
{
  gpointer result;
  GThread *thread;

  thread = g_thread_new ("test", thread2_func, NULL, TRUE, NULL);

  g_assert (g_thread_self () != thread);

  result = g_thread_join (thread);

  g_assert (result == thread);
}

static gpointer
thread3_func (gpointer data)
{
  GThread *peer = data;
  gint retval;

  retval = 3;

  if (peer)
    {
      gpointer result;

      result = g_thread_join (peer);

      retval += GPOINTER_TO_INT (result);
    }

  return GINT_TO_POINTER (retval);
}

/* test that g_thread_join() works across peers */
static void
test_thread3 (void)
{
  gpointer result;
  GThread *thread1, *thread2, *thread3;

  thread1 = g_thread_new_full ("a", thread3_func, NULL, TRUE, 0, NULL);
  thread2 = g_thread_new_full ("b", thread3_func, thread1, TRUE, 100, NULL);
  thread3 = g_thread_new_full ("c", thread3_func, thread2, TRUE, 100000, NULL);

  result = g_thread_join (thread3);

  g_assert_cmpint (GPOINTER_TO_INT(result), ==, 9);
}

/* test that thread creation fails as expected,
 * by setting RLIMIT_NPROC ridiculously low
 */
static void
test_thread4 (void)
{
#ifdef HAVE_PRLIMIT
  struct rlimit ol, nl;
  GThread *thread;
  GError *error;
  gint ret;

  nl.rlim_cur = 1;
  nl.rlim_max = 1;

  if ((ret = prlimit (getpid(), RLIMIT_NPROC, &nl, &ol)) != 0)
    g_error ("prlimit failed: %s\n", g_strerror (ret));

  error = NULL;
  thread = g_thread_new ("a", thread1_func, NULL, FALSE, &error);
  g_assert (thread == NULL);
  g_assert_error (error, G_THREAD_ERROR, G_THREAD_ERROR_AGAIN);
  g_error_free (error);

  prlimit (getpid (), RLIMIT_NPROC, &ol, NULL);
#endif
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/thread/thread1", test_thread1);
  g_test_add_func ("/thread/thread2", test_thread2);
  g_test_add_func ("/thread/thread3", test_thread3);
  g_test_add_func ("/thread/thread4", test_thread4);

  return g_test_run ();
}
