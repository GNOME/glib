#include <glib.h>

static void
test_mutex1 (void)
{
  GMutex mutex;

  g_mutex_init (&mutex);
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
  g_mutex_clear (&mutex);
}

static void
test_mutex3 (void)
{
  GMutex *mutex;

  mutex = g_mutex_new ();
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

  ret = g_mutex_trylock (&mutex);
  /* no guarantees that mutex is recursive, so could return 0 or 1 */

  g_mutex_unlock (&mutex);
  g_mutex_clear (&mutex);
}


int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/thread/mutex1", test_mutex1);
  g_test_add_func ("/thread/mutex2", test_mutex2);
  g_test_add_func ("/thread/mutex3", test_mutex3);
  g_test_add_func ("/thread/mutex4", test_mutex4);

  return g_test_run ();
}
