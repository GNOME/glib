#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <glib.h>

#include <time.h>
#include <stdlib.h>

#define d(x) x

#define MAX_THREADS            50
#define MAX_SORTS              5    /* only applies if
				       ASYC_QUEUE_DO_SORT is set to 1 */ 
#define MAX_TIME               20   /* seconds */
#define MIN_TIME               5    /* seconds */

#define SORT_QUEUE_AFTER       1
#define SORT_QUEUE_ON_PUSH     1    /* if this is done, the
				       SORT_QUEUE_AFTER is ignored */
#define QUIT_WHEN_DONE         1


#if SORT_QUEUE_ON_PUSH == 1
#  undef SORT_QUEUE_AFTER
#  define SORT_QUEUE_AFTER     0
#endif


static GMainLoop *main_loop = NULL;
static GThreadPool *thread_pool = NULL;
static GAsyncQueue *async_queue = NULL;


static gint
sort_compare (gconstpointer p1, gconstpointer p2, gpointer user_data)
{
  gint32 id1;
  gint32 id2;

  id1 = GPOINTER_TO_INT (p1);
  id2 = GPOINTER_TO_INT (p2);

  d(g_print ("comparing #1:%d and #2:%d, returning %d\n", 
	     id1, id2, (id1 > id2 ? +1 : id1 == id2 ? 0 : -1)));

  return (id1 > id2 ? +1 : id1 == id2 ? 0 : -1);
}

static gboolean
sort_queue (gpointer user_data)
{
  static gint sorts = 0;
  gboolean can_quit = FALSE;
  gint sort_multiplier;
  gint len;
  gint i;

  sort_multiplier = GPOINTER_TO_INT (user_data);

  if (SORT_QUEUE_AFTER) {
    d(g_print ("sorting async queue...\n")); 
    g_async_queue_sort (async_queue, sort_compare, NULL);

    sorts++;

    if (sorts >= sort_multiplier) {
      can_quit = TRUE;
    }
    
    g_async_queue_sort (async_queue, sort_compare, NULL);
    len = g_async_queue_length (async_queue);

    d(g_print ("sorted queue (for %d/%d times, size:%d)...\n", sorts, MAX_SORTS, len)); 
  } else {
    can_quit = TRUE;
    len = g_async_queue_length (async_queue);
    d(g_print ("printing queue (size:%d)...\n", len)); 
  }

  for (i = 0; i < len; i++) {
    gpointer p;
    
    p = g_async_queue_pop (async_queue);
    d(g_print ("item %d ---> %d\n", i, GPOINTER_TO_INT (p))); 
  }
  
  if (can_quit && QUIT_WHEN_DONE) {
    g_main_loop_quit (main_loop);
  }

  return !can_quit;
}

static void
enter_thread (gpointer data, gpointer user_data)
{
  gint   len;
  gint   id;
  gulong ms;

  id = GPOINTER_TO_INT (data);
  
  ms = g_random_int_range (MIN_TIME * 1000, MAX_TIME * 1000);
  d(g_print ("entered thread with id:%d, adding to queue in:%ld ms\n", id, ms));

  g_usleep (ms * 1000);

  if (SORT_QUEUE_ON_PUSH) {
    g_async_queue_push_sorted (async_queue, GINT_TO_POINTER (id), sort_compare, NULL);
  } else {
    g_async_queue_push (async_queue, GINT_TO_POINTER (id));
  }

  len = g_async_queue_length (async_queue);

  d(g_print ("thread id:%d added to async queue (size:%d)\n", 
	     id, len));
}

int main (int argc, char *argv[])
{
#if defined(G_THREADS_ENABLED) && ! defined(G_THREADS_IMPL_NONE)
  gint i;
  gint max_threads = MAX_THREADS;
  gint max_unused_threads = MAX_THREADS;
  gint sort_multiplier = MAX_SORTS;
  gint sort_interval;

  g_thread_init (NULL);

  d(g_print ("creating async queue...\n"));
  async_queue = g_async_queue_new ();

  g_return_val_if_fail (async_queue != NULL, EXIT_FAILURE);

  d(g_print ("creating thread pool with max threads:%d, max unused threads:%d...\n",
	     max_threads, max_unused_threads));
  thread_pool = g_thread_pool_new (enter_thread,
				   async_queue,
				   max_threads,
				   FALSE,
				   NULL);

  g_return_val_if_fail (thread_pool != NULL, EXIT_FAILURE);

  g_thread_pool_set_max_unused_threads (max_unused_threads);

  d(g_print ("creating threads...\n"));
  for (i = 0; i <= max_threads; i++) {
    GError *error = NULL;
  
    g_thread_pool_push (thread_pool, GINT_TO_POINTER (i), &error);
    
    if (!error) {
      g_assert_not_reached ();
    }
  }

  if (!SORT_QUEUE_AFTER) {
    sort_multiplier = 1;
  }
  
  sort_interval = ((MAX_TIME / sort_multiplier) + 2)  * 1000;
  d(g_print ("adding timeout of %d seconds to sort %d times\n", 
	     sort_interval, sort_multiplier));
  g_timeout_add (sort_interval, sort_queue, GINT_TO_POINTER (sort_multiplier));

  if (SORT_QUEUE_ON_PUSH) {
    d(g_print ("sorting when pushing into the queue...\n")); 
  }

  d(g_print ("entering main event loop\n"));

  main_loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (main_loop);
#endif
  
  return EXIT_SUCCESS;
}
