#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include <config.h>

#include <glib.h>

#define d(x) x

#define RUNS 100

#define WAIT                5    /* seconds */
#define MAX_THREADS         10
#define MAX_UNUSED_THREADS  2

G_LOCK_DEFINE_STATIC (thread_counter_pools);

static gulong abs_thread_counter = 0;
static gulong running_thread_counter = 0;
static gulong leftover_task_counter = 0;

G_LOCK_DEFINE_STATIC (last_thread);

static guint last_thread_id = 0;

G_LOCK_DEFINE_STATIC (thread_counter_sort);

static gulong sort_thread_counter = 0;


static GMainLoop *main_loop = NULL;


static void
test_thread_pools_entry_func (gpointer data, gpointer user_data)
{
  guint id = 0;

  id = GPOINTER_TO_UINT (data);

  d(g_print ("[pool] ---> [%3.3d] entered thread\n", id));

  G_LOCK (thread_counter_pools);
  abs_thread_counter++;
  running_thread_counter++;
  G_UNLOCK (thread_counter_pools);

  g_usleep (g_random_int_range (0, 4000));

  G_LOCK (thread_counter_pools);
  running_thread_counter--;
  leftover_task_counter--;

  g_print ("[pool] ---> [%3.3d] exiting thread (abs count:%ld, running count:%ld, left over:%ld)\n", 
 	   id, abs_thread_counter, running_thread_counter, leftover_task_counter); 
  G_UNLOCK (thread_counter_pools);
}

static void
test_thread_pools (void)
{
  GThreadPool *pool1, *pool2, *pool3;
  guint i;
  
  pool1 = g_thread_pool_new ((GFunc)test_thread_pools_entry_func, NULL, 3, FALSE, NULL);
  pool2 = g_thread_pool_new ((GFunc)test_thread_pools_entry_func, NULL, 5, TRUE, NULL);
  pool3 = g_thread_pool_new ((GFunc)test_thread_pools_entry_func, NULL, 7, TRUE, NULL);

  for (i = 0; i < RUNS; i++)
    {
      g_thread_pool_push (pool1, GUINT_TO_POINTER (i), NULL);
      g_thread_pool_push (pool2, GUINT_TO_POINTER (i), NULL);
      g_thread_pool_push (pool3, GUINT_TO_POINTER (i), NULL);
      leftover_task_counter += 3;
    } 
  
  g_thread_pool_free (pool1, TRUE, TRUE);
  g_thread_pool_free (pool2, FALSE, TRUE);
  g_thread_pool_free (pool3, FALSE, TRUE);

  g_assert (RUNS * 3 == abs_thread_counter + leftover_task_counter);
  g_assert (running_thread_counter == 0);  
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

  d(g_print ("%s ---> entered thread:%2.2d, last thread:%2.2d\n", 
	     is_sorted ? "[  sorted]" : "[unsorted]", thread_id, last_thread_id));

  if (is_sorted) {
    static gboolean last_failed = FALSE;

    if (last_thread_id > thread_id) {
      if (last_failed) {
	g_assert (last_thread_id <= thread_id);  
      }

      /* here we remember one fail and if it concurrently fails, it
	 can not be sorted. the last thread id might be < this thread
	 id if something is added to the queue since threads were
	 created */
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
  guint limit = 20;
  gint i;

  pool = g_thread_pool_new (test_thread_sort_entry_func, 
			    GINT_TO_POINTER (sort), 
			    MAX_THREADS,
			    FALSE,
			    NULL);

  g_thread_pool_set_max_unused_threads (MAX_UNUSED_THREADS); 

  if (sort) {
    g_thread_pool_set_sort_function (pool, 
				     test_thread_sort_compare_func,
				     GUINT_TO_POINTER (69));
  }
  
  for (i = 0; i < limit; i++) {
    guint id;

    id = g_random_int_range (1, limit*2);
    g_thread_pool_push (pool, GUINT_TO_POINTER (id), NULL);
  }

  g_assert (g_thread_pool_get_num_threads (pool) == g_thread_pool_get_max_threads (pool));
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
    d(g_print ("***** RUNNING TEST %2.2d *****\n", test_number)); 
  }
   
  if (run_next) {
    test_number++;

    switch (test_number) {
    case 1:
      test_thread_pools ();   
      break;
    case 2:
      test_thread_sort (FALSE);  
      break;
    case 3:
      test_thread_sort (TRUE);  
      break;
    default:
      d(g_print ("***** END OF TESTS *****\n")); 
      g_main_loop_quit (main_loop);
      continue_timeout = FALSE;
      break;
    }

    run_next = FALSE;
    return TRUE;
  }

  if (test_number == 1) {
    G_LOCK (thread_counter_pools); 
    quit &= running_thread_counter <= 0;
    d(g_print ("***** POOL RUNNING THREAD COUNT:%ld\n", 
	       running_thread_counter)); 
    G_UNLOCK (thread_counter_pools); 
  }

  if (test_number == 2 || test_number == 3) {
    G_LOCK (thread_counter_sort);
    quit &= sort_thread_counter <= 0;
    d(g_print ("***** POOL SORT THREAD COUNT:%ld\n", 
	       sort_thread_counter)); 
    G_UNLOCK (thread_counter_sort); 
  }

  if (quit) {
    run_next = TRUE;
  }

  return continue_timeout;
}

int 
main (int argc, char *argv[])
{
  /* Only run the test, if threads are enabled and a default thread
     implementation is available */

#if defined(G_THREADS_ENABLED) && ! defined(G_THREADS_IMPL_NONE)
  g_thread_init (NULL);

  d(g_print ("Starting... (in one second)\n"));
  g_timeout_add (1000, test_check_start_and_stop, NULL); 
  
  main_loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (main_loop);
#endif

  return 0;
}
