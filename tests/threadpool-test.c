#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include <config.h>

#include <glib.h>

/* #define DEBUG_MSG(x) */
#define DEBUG_MSG(args) g_printerr args ; g_printerr ("\n");

#define RUNS 100

#define WAIT                5    /* seconds */
#define MAX_THREADS         10

/* if > 0 the test will run continously (since the test ends when
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
test_thread_pools_entry_func (gpointer data, gpointer user_data)
{
  guint id = 0;

  id = GPOINTER_TO_UINT (data);

  DEBUG_MSG (("[pool] ---> [%3.3d] entered thread.", id));

  G_LOCK (thread_counter_pools);
  abs_thread_counter++;
  running_thread_counter++;
  G_UNLOCK (thread_counter_pools);

  g_usleep (g_random_int_range (0, 4000));

  G_LOCK (thread_counter_pools);
  running_thread_counter--;
  leftover_task_counter--;

  DEBUG_MSG (("[pool] ---> [%3.3d] exiting thread (abs count:%ld, "
	      "running count:%ld, left over:%ld)", 
	      id, abs_thread_counter, 
	      running_thread_counter, leftover_task_counter)); 
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
      g_thread_pool_push (pool1, GUINT_TO_POINTER (i + 1), NULL);
      g_thread_pool_push (pool2, GUINT_TO_POINTER (i + 1), NULL);
      g_thread_pool_push (pool3, GUINT_TO_POINTER (i + 1), NULL);
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

  DEBUG_MSG (("%s ---> entered thread:%2.2d, last thread:%2.2d", 
	      is_sorted ? "[  sorted]" : "[unsorted]", 
	      thread_id, last_thread_id));

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
    g_thread_pool_push (pool, GUINT_TO_POINTER (id + 1), NULL);
  }

  g_assert (g_thread_pool_get_num_threads (pool) == g_thread_pool_get_max_threads (pool));
}

static void
test_thread_idle_time_entry_func (gpointer data, gpointer user_data)
{
  guint thread_id;
  
  thread_id = GPOINTER_TO_UINT (data);

  DEBUG_MSG (("[idle] ---> entered thread:%2.2d", thread_id));

  g_usleep (WAIT * 1000);

  DEBUG_MSG (("[idle] <--- exiting thread:%2.2d", thread_id));
}

static gboolean 
test_thread_idle_timeout (gpointer data)
{
  guint interval;
  gint i;

  interval = GPOINTER_TO_UINT (data);
  
  for (i = 0; i < 2; i++) {
    g_thread_pool_push (idle_pool, GUINT_TO_POINTER (100 + i), NULL); 
    DEBUG_MSG (("[idle] ===> pushed new thread with id:%d, number "
		"of threads:%d, unprocessed:%d",
		100 + i, 
		g_thread_pool_get_num_threads (idle_pool),
		g_thread_pool_unprocessed (idle_pool)));
  }
  

  return FALSE;
}

static void
test_thread_idle_time ()
{
  guint limit = 50;
  guint interval = 10000;
  gint i;

  idle_pool = g_thread_pool_new (test_thread_idle_time_entry_func, 
				 NULL, 
				 MAX_THREADS,
				 FALSE,
				 NULL);

  g_thread_pool_set_max_unused_threads (MAX_UNUSED_THREADS);  
  g_thread_pool_set_max_idle_time (interval); 

  g_assert (g_thread_pool_get_max_unused_threads () == MAX_UNUSED_THREADS);   
  g_assert (g_thread_pool_get_max_idle_time () == interval);

  for (i = 0; i < limit; i++) {
    g_thread_pool_push (idle_pool, GUINT_TO_POINTER (i + 1), NULL); 
    DEBUG_MSG (("[idle] ===> pushed new thread with id:%d, "
		"number of threads:%d, unprocessed:%d",
		i,
		g_thread_pool_get_num_threads (idle_pool),
		g_thread_pool_unprocessed (idle_pool)));
  }

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
    DEBUG_MSG (("***** RUNNING TEST %2.2d *****", test_number)); 
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
    case 4:
      test_thread_idle_time ();   
      break;
    default:
      DEBUG_MSG (("***** END OF TESTS *****")); 
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
    DEBUG_MSG (("***** POOL RUNNING THREAD COUNT:%ld", 
		running_thread_counter)); 
    G_UNLOCK (thread_counter_pools); 
  }

  if (test_number == 2 || test_number == 3) {
    G_LOCK (thread_counter_sort);
    quit &= sort_thread_counter <= 0;
    DEBUG_MSG (("***** POOL SORT THREAD COUNT:%ld", 
		sort_thread_counter)); 
    G_UNLOCK (thread_counter_sort); 
  }

  if (test_number == 4) {
    guint idle;

    idle = g_thread_pool_get_num_unused_threads ();
    quit &= idle < 1;
    DEBUG_MSG (("***** POOL IDLE THREAD COUNT:%d, UNPROCESSED JOBS:%d",
		idle, g_thread_pool_unprocessed (idle_pool)));
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

  DEBUG_MSG (("Starting... (in one second)"));
  g_timeout_add (1000, test_check_start_and_stop, NULL); 
  
  main_loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (main_loop);
#endif

  return 0;
}
