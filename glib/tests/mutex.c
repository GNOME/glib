/* Unit tests for GMutex
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

static void
test_mutex1 (void)
{
  GMutex mutex;

  g_mutex_init (&mutex);
  g_mutex_lock (&mutex);
  g_mutex_unlock (&mutex);
  g_mutex_lock (&mutex);
  g_mutex_unlock (&mutex);
  g_mutex_clear (&mutex);
}

static void
test_mutex2 (void)
{
  GMutex mutex = G_MUTEX_INIT;

  g_mutex_lock (&mutex);
  g_mutex_unlock (&mutex);
  g_mutex_lock (&mutex);
  g_mutex_unlock (&mutex);
  g_mutex_clear (&mutex);
}

static void
test_mutex3 (void)
{
  GMutex *mutex;

  mutex = g_mutex_new ();
  g_mutex_lock (mutex);
  g_mutex_unlock (mutex);
  g_mutex_lock (mutex);
  g_mutex_unlock (mutex);
  g_mutex_free (mutex);
}

static void
test_mutex4 (void)
{
  GMutex mutex = G_MUTEX_INIT;
  gboolean ret;

  ret = g_mutex_trylock (&mutex);
  g_assert (ret);

  /* no guarantees that mutex is recursive, so could return 0 or 1 */
  if (g_mutex_trylock (&mutex))
    g_mutex_unlock (&mutex);

  g_mutex_unlock (&mutex);
  g_mutex_clear (&mutex);
}

#define LOCKS      48
#define ITERATIONS 10000
#define THREADS    100


GThread *owners[LOCKS];
GMutex   locks[LOCKS];
gpointer ptrs[LOCKS];

static void
acquire (gint nr)
{
  GThread *self;

  self = g_thread_self ();

  if (!g_mutex_trylock (&locks[nr]))
    {
      if (g_test_verbose ())
        g_print ("thread %p going to block on lock %d\n", self, nr);

      g_mutex_lock (&locks[nr]);
    }

  g_assert (owners[nr] == NULL);   /* hopefully nobody else is here */
  owners[nr] = self;

  /* let some other threads try to ruin our day */
  g_thread_yield ();
  g_thread_yield ();
  g_thread_yield ();

  g_assert (owners[nr] == self);   /* hopefully this is still us... */
  owners[nr] = NULL;               /* make way for the next guy */

  g_mutex_unlock (&locks[nr]);
}

static gpointer
thread_func (gpointer data)
{
  gint i;
  GRand *rand;

  rand = g_rand_new ();

  for (i = 0; i < ITERATIONS; i++)
    acquire (g_rand_int_range (rand, 0, LOCKS));

  g_rand_free (rand);

  return NULL;
}

static void
test_mutex5 (void)
{
  gint i;
  GThread *threads[THREADS];

  for (i = 0; i < LOCKS; i++)
    g_mutex_init (&locks[i]);

  for (i = 0; i < THREADS; i++)
    threads[i] = g_thread_create (thread_func, NULL, TRUE, NULL);

  for (i = 0; i < THREADS; i++)
    g_thread_join (threads[i]);

  for (i = 0; i < LOCKS; i++)
    g_mutex_clear (&locks[i]);

  for (i = 0; i < LOCKS; i++)
    g_assert (owners[i] == NULL);
}


int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/thread/mutex1", test_mutex1);
  g_test_add_func ("/thread/mutex2", test_mutex2);
  g_test_add_func ("/thread/mutex3", test_mutex3);
  g_test_add_func ("/thread/mutex4", test_mutex4);
  g_test_add_func ("/thread/mutex5", test_mutex5);

  return g_test_run ();
}
