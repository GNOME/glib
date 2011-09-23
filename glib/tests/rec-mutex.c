#include <glib.h>

static void
test_rec_mutex1 (void)
{
  GRecMutex mutex;

  g_rec_mutex_init (&mutex);
  g_rec_mutex_lock (&mutex);
  g_rec_mutex_unlock (&mutex);
  g_rec_mutex_clear (&mutex);
}

static void
test_rec_mutex2 (void)
{
  GRecMutex mutex = G_REC_MUTEX_INIT;

  g_rec_mutex_lock (&mutex);
  g_rec_mutex_unlock (&mutex);
  g_rec_mutex_clear (&mutex);
}

static void
test_rec_mutex3 (void)
{
  GRecMutex mutex = G_REC_MUTEX_INIT;
  gboolean ret;

  ret = g_rec_mutex_trylock (&mutex);
  g_assert (ret);

  ret = g_rec_mutex_trylock (&mutex);
  g_assert (ret);

  g_rec_mutex_unlock (&mutex);
  g_rec_mutex_clear (&mutex);
  g_rec_mutex_clear (&mutex);
}


int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/thread/rec-mutex1", test_rec_mutex1);
  g_test_add_func ("/thread/rec-mutex2", test_rec_mutex2);
  g_test_add_func ("/thread/rec-mutex3", test_rec_mutex3);

  return g_test_run ();
}
