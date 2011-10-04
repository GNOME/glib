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

#include <glib.h>

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

  thread = g_thread_new ("test", thread1_func, NULL, TRUE, NULL);

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

  thread1 = g_thread_new ("a", thread3_func, NULL, TRUE, NULL);
  thread2 = g_thread_new ("b", thread3_func, thread1, TRUE, NULL);
  thread3 = g_thread_new ("c", thread3_func, thread2, TRUE, NULL);

  result = g_thread_join (thread3);

  g_assert_cmpint (GPOINTER_TO_INT(result), ==, 9);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_assert (g_thread_get_initialized ());

  g_test_add_func ("/thread/thread1", test_thread1);
  g_test_add_func ("/thread/thread2", test_thread2);
  g_test_add_func ("/thread/thread3", test_thread3);

  return g_test_run ();
}
