#include <glib.h>

#define RUNS 100

G_LOCK_DEFINE_STATIC (thread_counter);
gulong abs_thread_counter;
gulong running_thread_counter;

void
thread_pool_func (gpointer a, gpointer b)
{
  G_LOCK (thread_counter);
  abs_thread_counter++;
  running_thread_counter++;
  G_UNLOCK (thread_counter);

  g_usleep (g_random_int_range (0, 4000));

  G_LOCK (thread_counter);
  running_thread_counter--;
  G_UNLOCK (thread_counter);
}

int 
main (int   argc,
      char *argv[])
{
  /* Only run the test, if threads are enabled and a default thread
     implementation is available */
#if defined(G_THREADS_ENABLED) && ! defined(G_THREADS_IMPL_NONE)
  GThreadPool *pool1, *pool2, *pool3;
  guint i;
  g_thread_init (NULL);
  
  pool1 = g_thread_pool_new (thread_pool_func, NULL, 3, FALSE, NULL);
  pool2 = g_thread_pool_new (thread_pool_func, NULL, 5, TRUE, NULL);
  pool3 = g_thread_pool_new (thread_pool_func, NULL, 7, TRUE, NULL);

  for (i = 0; i < RUNS; i++)
    {
      g_thread_pool_push (pool1, GUINT_TO_POINTER (1), NULL);
      g_thread_pool_push (pool2, GUINT_TO_POINTER (1), NULL);
      g_thread_pool_push (pool3, GUINT_TO_POINTER (1), NULL);
    } 
  
  g_thread_pool_free (pool1, FALSE, TRUE);
  g_thread_pool_free (pool2, FALSE, TRUE);
  g_thread_pool_free (pool3, FALSE, TRUE);

  g_assert (RUNS * 3 == abs_thread_counter);
  g_assert (running_thread_counter == 0);  
#endif
  return 0;
}
