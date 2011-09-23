/* Unit tests for GRWLock
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
test_rwlock1 (void)
{
  GRWLock lock;

  g_rw_lock_init (&lock);
  g_rw_lock_writer_lock (&lock);
  g_rw_lock_writer_unlock (&lock);
  g_rw_lock_writer_lock (&lock);
  g_rw_lock_writer_unlock (&lock);
  g_rw_lock_clear (&lock);
}

static void
test_rwlock2 (void)
{
  GRWLock lock = G_RW_LOCK_INIT;

  g_rw_lock_writer_lock (&lock);
  g_rw_lock_writer_unlock (&lock);
  g_rw_lock_writer_lock (&lock);
  g_rw_lock_writer_unlock (&lock);
  g_rw_lock_clear (&lock);
}

static void
test_rwlock3 (void)
{
  GRWLock lock = G_RW_LOCK_INIT;
  gboolean ret;

  ret = g_rw_lock_writer_trylock (&lock);
  g_assert (ret);
  ret = g_rw_lock_writer_trylock (&lock);
  g_assert (!ret);

  g_rw_lock_writer_unlock (&lock);

  g_rw_lock_clear (&lock);
}

static void
test_rwlock4 (void)
{
  GRWLock lock = G_RW_LOCK_INIT;

  g_rw_lock_reader_lock (&lock);
  g_rw_lock_reader_unlock (&lock);
  g_rw_lock_reader_lock (&lock);
  g_rw_lock_reader_unlock (&lock);
  g_rw_lock_clear (&lock);
}

static void
test_rwlock5 (void)
{
  GRWLock lock = G_RW_LOCK_INIT;
  gboolean ret;

  ret = g_rw_lock_reader_trylock (&lock);
  g_assert (ret);
  ret = g_rw_lock_reader_trylock (&lock);
  g_assert (ret);

  g_rw_lock_reader_unlock (&lock);
  g_rw_lock_reader_unlock (&lock);

  g_rw_lock_clear (&lock);
}

static void
test_rwlock6 (void)
{
  GRWLock lock = G_RW_LOCK_INIT;
  gboolean ret;

  g_rw_lock_writer_lock (&lock);
  ret = g_rw_lock_reader_trylock (&lock);
  g_assert (!ret);
  g_rw_lock_writer_unlock (&lock);

  g_rw_lock_reader_lock (&lock);
  ret = g_rw_lock_writer_trylock (&lock);
  g_assert (!ret);
  g_rw_lock_reader_unlock (&lock);

  g_rw_lock_clear (&lock);
}


#define LOCKS      48
#define ITERATIONS 10000
#define THREADS    100


GThread *owners[LOCKS];
GRWLock  locks[LOCKS];

static void
acquire (gint nr)
{
  GThread *self;

  self = g_thread_self ();

  if (!g_rw_lock_writer_trylock (&locks[nr]))
    {
      if (g_test_verbose ())
        g_print ("thread %p going to block on lock %d\n", self, nr);

      g_rw_lock_writer_lock (&locks[nr]);
    }

  g_assert (owners[nr] == NULL);   /* hopefully nobody else is here */
  owners[nr] = self;

  /* let some other threads try to ruin our day */
  g_thread_yield ();
  g_thread_yield ();
  g_thread_yield ();

  g_assert (owners[nr] == self);   /* hopefully this is still us... */
  owners[nr] = NULL;               /* make way for the next guy */

  g_rw_lock_writer_unlock (&locks[nr]);
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
test_rwlock7 (void)
{
  gint i;
  GThread *threads[THREADS];

  for (i = 0; i < LOCKS; i++)
    g_rw_lock_init (&locks[i]);

  for (i = 0; i < THREADS; i++)
    threads[i] = g_thread_create (thread_func, NULL, TRUE, NULL);

  for (i = 0; i < THREADS; i++)
    g_thread_join (threads[i]);

  for (i = 0; i < LOCKS; i++)
    g_rw_lock_clear (&locks[i]);

  for (i = 0; i < LOCKS; i++)
    g_assert (owners[i] == NULL);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/thread/rwlock1", test_rwlock1);
  g_test_add_func ("/thread/rwlock2", test_rwlock2);
  g_test_add_func ("/thread/rwlock3", test_rwlock3);
  g_test_add_func ("/thread/rwlock4", test_rwlock4);
  g_test_add_func ("/thread/rwlock5", test_rwlock5);
  g_test_add_func ("/thread/rwlock6", test_rwlock6);
  g_test_add_func ("/thread/rwlock7", test_rwlock7);

  return g_test_run ();
}
