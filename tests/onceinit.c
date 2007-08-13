/* g_once_init_*() test
 * Copyright (C) 2007 Tim Janik
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.

 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

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
#include <stdlib.h>

#define N_THREADS               (11)

static GMutex      *tmutex = NULL;
static GCond       *tcond = NULL;
static volatile int tmain_call_count = 0;
static char         dummy_value = 'x';

static void
assert_singleton_execution1 (void)
{
  static volatile int seen_execution = 0;
  int old_seen_execution = g_atomic_int_exchange_and_add (&seen_execution, 1);
  if (old_seen_execution != 0)
    g_error ("%s: function executed more than once", G_STRFUNC);
}

static void
assert_singleton_execution2 (void)
{
  static volatile int seen_execution = 0;
  int old_seen_execution = g_atomic_int_exchange_and_add (&seen_execution, 1);
  if (old_seen_execution != 0)
    g_error ("%s: function executed more than once", G_STRFUNC);
}

static void
assert_singleton_execution3 (void)
{
  static volatile int seen_execution = 0;
  int old_seen_execution = g_atomic_int_exchange_and_add (&seen_execution, 1);
  if (old_seen_execution != 0)
    g_error ("%s: function executed more than once", G_STRFUNC);
}

static void
initializer1 (void)
{
  static volatile gsize initialized = 0;
  if (g_once_init_enter (&initialized))
    {
      gsize initval = 42;
      assert_singleton_execution1();
      g_once_init_leave (&initialized, initval);
    }
}

static gpointer
initializer2 (void)
{
  static volatile gsize initialized = 0;
  if (g_once_init_enter (&initialized))
    {
      void *pointer_value = &dummy_value;
      assert_singleton_execution2();
      g_once_init_leave (&initialized, (gsize) pointer_value);
    }
  return (void*) initialized;
}

static void
initializer3 (void)
{
  static volatile gsize initialized = 0;
  if (g_once_init_enter (&initialized))
    {
      gsize initval = 42;
      assert_singleton_execution3();
      g_usleep (25 * 1000);     /* waste time for multiple threads to wait */
      g_once_init_leave (&initialized, initval);
    }
}

static gpointer
tmain_call_initializer3 (gpointer user_data)
{
  g_mutex_lock (tmutex);
  g_cond_wait (tcond, tmutex);
  g_mutex_unlock (tmutex);
  //g_printf ("[");
  initializer3();
  //g_printf ("]\n");
  g_atomic_int_exchange_and_add (&tmain_call_count, 1);
  return NULL;
}

int
main (int   argc,
      char *argv[])
{
  GThread *threads[N_THREADS];
  int i;
  /* test simple initializer */
  initializer1();
  initializer1();
  /* test pointer initializer */
  void *p = initializer2();
  g_assert (p == &dummy_value);
  p = initializer2();
  g_assert (p == &dummy_value);
  /* setup threads */
  g_thread_init (NULL);
  tmutex = g_mutex_new ();
  tcond = g_cond_new ();
  /* start multiple threads for initializer3() */
  g_mutex_lock (tmutex);
  for (i = 0; i < N_THREADS; i++)
    threads[i] = g_thread_create (tmain_call_initializer3, 0, FALSE, NULL);
  g_mutex_unlock (tmutex);
  /* concurrently call initializer3() */
  g_cond_broadcast (tcond);
  /* loop until all threads passed the call to initializer3() */
  while (g_atomic_int_get (&tmain_call_count) < i)
    {
      if (rand() % 2)
        g_thread_yield();   /* concurrent shuffling for single core */
      else
        g_usleep (1000);    /* concurrent shuffling for multi core */
      g_cond_broadcast (tcond);
    }
  return 0;
}
