#include "config.h"

#include <glib.h>

#define WAIT                5    /* seconds */
#define MAX_THREADS         10

/* if > 0 the test will run continuously (since the test ends when
 * thread count is 0), if -1 it means no limit to unused threads
 * if 0 then no unused threads are possible */
#define MAX_UNUSED_THREADS -1

G_LOCK_DEFINE_STATIC (thread_counter_pools);

static gulong abs_thread_counter = 0;
static gulong running_thread_counter = 0;
static gulong leftover_task_counter = 0;

G_LOCK_DEFINE_STATIC (last_thread);

static guint last_thread_id = 0;

G_LOCK_DEFINE_STATIC (thread_counter_sort);

static gulong sort_thread_counter = 0;

static GThreadPool *idle_pool = NULL;

static GMainLoop *main_loop = NULL;

static void
test_thread_functions (void)
{
  gint max_unused_threads;
  guint max_idle_time;

  /* This function attempts to call functions which don't need a
   * threadpool to operate to make sure no uninitialised pointers
   * accessed and no errors occur.
   */

  max_unused_threads = 3;

  g_thread_pool_set_max_unused_threads (max_unused_threads);

  g_assert_cmpint (g_thread_pool_get_max_unused_threads (), ==,
                   max_unused_threads);

  g_assert_cmpint (g_thread_pool_get_num_unused_threads (), ==, 0);

  g_thread_pool_stop_unused_threads ();

  max_idle_time = 10 * G_USEC_PER_SEC;

  g_thread_pool_set_max_idle_time (max_idle_time);

  g_assert_cmpint (g_thread_pool_get_max_idle_time (), ==, max_idle_time);

  g_thread_pool_set_max_idle_time (0);

  g_assert_cmpint (g_thread_pool_get_max_idle_time (), ==, 0);
}

static void
test_thread_stop_unused (void)
{
  GThreadPool *pool;
  guint i;
  guint limit = 100;

  /* Spawn a few threads. */
  g_thread_pool_set_max_unused_threads (-1);
  pool = g_thread_pool_new ((GFunc) g_usleep, NULL, -1, FALSE, NULL);

  for (i = 0; i < limit; i++)
    g_thread_pool_push (pool, GUINT_TO_POINTER (1000), NULL);

  /* Wait for the threads to migrate. */
  while (g_thread_pool_get_num_threads (pool) != 0)
    g_usleep (100);

  g_assert_cmpuint (g_thread_pool_get_num_threads (pool), ==, 0);
  g_assert_cmpuint (g_thread_pool_get_num_unused_threads (), >, 0);

  /* Wait for threads to die. */
  g_thread_pool_stop_unused_threads ();

  while (g_thread_pool_get_num_unused_threads () != 0)
    g_usleep (100);

  g_assert_cmpuint (g_thread_pool_get_num_unused_threads (), ==, 0);

  g_thread_pool_set_max_unused_threads (MAX_THREADS);

  g_assert_cmpuint (g_thread_pool_get_num_threads (pool), ==, 0);
  g_assert_cmpuint (g_thread_pool_get_num_unused_threads (), ==, 0);

  g_thread_pool_free (pool, FALSE, TRUE);
}

static void
test_thread_stop_unused_multiple (void)
{
  GThreadPool *pools[10];
  guint i, j;
  const guint limit = 10;
  gboolean all_stopped;

  /* Spawn a few threads. */
  g_thread_pool_set_max_unused_threads (-1);

  for (i = 0; i < G_N_ELEMENTS (pools); i++)
    {
      pools[i] = g_thread_pool_new ((GFunc) g_usleep, NULL, -1, FALSE, NULL);

      for (j = 0; j < limit; j++)
        g_thread_pool_push (pools[i], GUINT_TO_POINTER (100), NULL);
    }

  all_stopped = FALSE;
  while (!all_stopped)
    {
      all_stopped = TRUE;
      for (i = 0; i < G_N_ELEMENTS (pools); i++)
        all_stopped &= (g_thread_pool_get_num_threads (pools[i]) == 0);
    }

  for (i = 0; i < G_N_ELEMENTS (pools); i++)
    {
      g_assert_cmpuint (g_thread_pool_get_num_threads (pools[i]), ==, 0);
      g_assert_cmpuint (g_thread_pool_get_num_unused_threads (), >, 0);
    }

  /* Wait for threads to die. */
  g_thread_pool_stop_unused_threads ();

  while (g_thread_pool_get_num_unused_threads () != 0)
    g_usleep (100);

  g_assert_cmpuint (g_thread_pool_get_num_unused_threads (), ==, 0);

  for (i = 0; i < G_N_ELEMENTS (pools); i++)
    g_thread_pool_free (pools[i], FALSE, TRUE);
}

static void
test_thread_pools_entry_func (gpointer data, gpointer user_data)
{
  G_LOCK (thread_counter_pools);
  abs_thread_counter++;
  running_thread_counter++;
  G_UNLOCK (thread_counter_pools);

  g_usleep (g_random_int_range (0, 4000));

  G_LOCK (thread_counter_pools);
  running_thread_counter--;
  leftover_task_counter--;

  G_UNLOCK (thread_counter_pools);
}

static void
test_thread_pools (void)
{
  GThreadPool *pool1, *pool2, *pool3;
  guint runs;
  guint i;

  pool1 = g_thread_pool_new ((GFunc)test_thread_pools_entry_func, NULL, 3, FALSE, NULL);
  pool2 = g_thread_pool_new ((GFunc)test_thread_pools_entry_func, NULL, 5, TRUE, NULL);
  pool3 = g_thread_pool_new ((GFunc)test_thread_pools_entry_func, NULL, 7, TRUE, NULL);

  runs = 300;
  for (i = 0; i < runs; i++)
    {
      g_thread_pool_push (pool1, GUINT_TO_POINTER (i + 1), NULL);
      g_thread_pool_push (pool2, GUINT_TO_POINTER (i + 1), NULL);
      g_thread_pool_push (pool3, GUINT_TO_POINTER (i + 1), NULL);

      G_LOCK (thread_counter_pools);
      leftover_task_counter += 3;
      G_UNLOCK (thread_counter_pools);
    }

  g_thread_pool_free (pool1, TRUE, TRUE);
  g_thread_pool_free (pool2, FALSE, TRUE);
  g_thread_pool_free (pool3, FALSE, TRUE);

  g_assert_cmpint (runs * 3, ==, abs_thread_counter + leftover_task_counter);
  g_assert_cmpint (running_thread_counter, ==, 0);
}

static gint
test_thread_sort_compare_func (gconstpointer a, gconstpointer b, gpointer user_data)
{
  guint32 id1, id2;

  id1 = GPOINTER_TO_UINT (a);
  id2 = GPOINTER_TO_UINT (b);

  return (id1 > id2 ? +1 : id1 == id2 ? 0 : -1);
}

static void
test_thread_sort_entry_func (gpointer data, gpointer user_data)
{
  guint thread_id;
  gboolean is_sorted;

  G_LOCK (last_thread);

  thread_id = GPOINTER_TO_UINT (data);
  is_sorted = GPOINTER_TO_INT (user_data);

  if (is_sorted) {
    static gboolean last_failed = FALSE;

    if (last_thread_id > thread_id) {
      if (last_failed) {
          g_assert_cmpint (last_thread_id, <=, thread_id);
      }

      /* Here we remember one fail and if it concurrently fails, it
       * can not be sorted. the last thread id might be < this thread
       * id if something is added to the queue since threads were
       * created
       */
      last_failed = TRUE;
    } else {
      last_failed = FALSE;
    }

    last_thread_id = thread_id;
  }

  G_UNLOCK (last_thread);

  g_usleep (WAIT * 1000);
}

static void
test_thread_sort (gboolean sort)
{
  GThreadPool *pool;
  guint limit;
  guint max_threads;
  guint i;

  limit = MAX_THREADS * 10;

  if (sort) {
    max_threads = 1;
  } else {
    max_threads = MAX_THREADS;
  }

  /* It is important that we only have a maximum of 1 thread for this
   * test since the results can not be guaranteed to be sorted if > 1.
   *
   * Threads are scheduled by the operating system and are executed at
   * random. It cannot be assumed that threads are executed in the
   * order they are created. This was discussed in bug #334943.
   */

  pool = g_thread_pool_new (test_thread_sort_entry_func,
			    GINT_TO_POINTER (sort),
			    max_threads,
			    FALSE,
			    NULL);

  g_thread_pool_set_max_unused_threads (MAX_UNUSED_THREADS);

  if (sort) {
    g_thread_pool_set_sort_function (pool,
				     test_thread_sort_compare_func,
				     NULL);
  }

  for (i = 0; i < limit; i++) {
    guint id;

    id = g_random_int_range (1, limit) + 1;
    g_thread_pool_push (pool, GUINT_TO_POINTER (id), NULL);
    g_test_message ("%s ===> pushed new thread with id:%d, number "
                    "of threads:%d, unprocessed:%d",
                    sort ? "[  sorted]" : "[unsorted]",
                    id,
                    g_thread_pool_get_num_threads (pool),
                    g_thread_pool_unprocessed (pool));
  }

  g_assert_cmpint (g_thread_pool_get_max_threads (pool), ==, (gint) max_threads);
  g_assert_cmpuint (g_thread_pool_get_num_threads (pool), <=,
                    (guint) g_thread_pool_get_max_threads (pool));
  g_thread_pool_free (pool, TRUE, TRUE);
}

static void
test_thread_idle_time_entry_func (gpointer data, gpointer user_data)
{
  g_usleep (WAIT * 1000);
}

static gboolean
test_thread_idle_timeout (gpointer data)
{
  gint i;

  for (i = 0; i < 2; i++) {
    g_thread_pool_push (idle_pool, GUINT_TO_POINTER (100 + i), NULL);
  }

  return FALSE;
}

static void
test_thread_idle_time (void)
{
  guint limit = 50;
  guint interval = 10000;
  guint i;

  idle_pool = g_thread_pool_new (test_thread_idle_time_entry_func,
				 NULL,
				 0,
				 FALSE,
				 NULL);

  g_thread_pool_set_max_threads (idle_pool, MAX_THREADS, NULL);
  g_thread_pool_set_max_unused_threads (MAX_UNUSED_THREADS);
  g_thread_pool_set_max_idle_time (interval);

  g_assert_cmpint (g_thread_pool_get_max_threads (idle_pool), ==,
                   MAX_THREADS);
  g_assert_cmpint (g_thread_pool_get_max_unused_threads (), ==,
                   MAX_UNUSED_THREADS);
  g_assert_cmpint (g_thread_pool_get_max_idle_time (), ==, interval);

  for (i = 0; i < limit; i++) {
    g_thread_pool_push (idle_pool, GUINT_TO_POINTER (i + 1), NULL);
  }

  g_assert_cmpint (g_thread_pool_unprocessed (idle_pool), <=, limit);

  g_timeout_add ((interval - 1000),
                 test_thread_idle_timeout,
                 GUINT_TO_POINTER (interval));
}

static gboolean
test_check_start_and_stop (gpointer user_data)
{
  static guint test_number = 0;
  static gboolean run_next = FALSE;
  gboolean continue_timeout = TRUE;
  gboolean quit = TRUE;

  if (test_number == 0) {
    run_next = TRUE;
    g_test_message ("***** RUNNING TEST %2.2d *****", test_number);
  }

  if (run_next) {
    test_number++;

    switch (test_number) {
    case 1:
      test_thread_functions ();
      break;
    case 2:
      test_thread_stop_unused ();
      break;
    case 3:
      test_thread_pools ();
      break;
    case 4:
      test_thread_sort (FALSE);
      break;
    case 5:
      test_thread_sort (TRUE);
      break;
    case 6:
      test_thread_stop_unused ();
      break;
    case 7:
      test_thread_stop_unused_multiple ();
      break;
    case 8:
      test_thread_idle_time ();
      break;
    default:
      g_test_message ("***** END OF TESTS *****");
      g_main_loop_quit (main_loop);
      continue_timeout = FALSE;
      break;
    }

    run_next = FALSE;
    return continue_timeout;
  }

  if (test_number == 3) {
    G_LOCK (thread_counter_pools);
    quit &= running_thread_counter <= 0;
    g_test_message ("***** POOL RUNNING THREAD COUNT:%ld",
                    running_thread_counter);
    G_UNLOCK (thread_counter_pools);
  }

  if (test_number == 4 || test_number == 5) {
    G_LOCK (thread_counter_sort);
    quit &= sort_thread_counter <= 0;
    g_test_message ("***** POOL SORT THREAD COUNT:%ld",
                    sort_thread_counter);
    G_UNLOCK (thread_counter_sort);
  }

  if (test_number == 8) {
    guint idle;

    idle = g_thread_pool_get_num_unused_threads ();
    quit &= idle < 1;
    g_test_message ("***** POOL IDLE THREAD COUNT:%d, UNPROCESSED JOBS:%d",
                    idle, g_thread_pool_unprocessed (idle_pool));
  }

  if (quit) {
    run_next = TRUE;
  }

  return continue_timeout;
}

static void
test_threadpool_basics (void)
{
  g_timeout_add (1000, test_check_start_and_stop, NULL);

  main_loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (main_loop);
  g_main_loop_unref (main_loop);

  g_thread_pool_free (idle_pool, FALSE, TRUE);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/threadpool/basics", test_threadpool_basics);

  return g_test_run ();
}
