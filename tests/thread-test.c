#include <glib.h>

/* GMutex */

static GMutex* test_g_mutex_mutex = NULL;
static guint test_g_mutex_int = 0;
G_LOCK_DEFINE_STATIC (test_g_mutex);

static void
test_g_mutex_thread (gpointer data)
{
  g_assert (GPOINTER_TO_INT (data) == 42);
  g_assert (g_mutex_trylock (test_g_mutex_mutex) == FALSE);
  g_assert (G_TRYLOCK (test_g_mutex) == FALSE);
  g_mutex_lock (test_g_mutex_mutex);
  g_assert (test_g_mutex_int == 42);
  g_mutex_unlock (test_g_mutex_mutex);
}

static void
test_g_mutex (void)
{
  GThread *thread;
  test_g_mutex_mutex = g_mutex_new ();

  g_assert (g_mutex_trylock (test_g_mutex_mutex));
  g_assert (G_TRYLOCK (test_g_mutex));
  thread = g_thread_create (test_g_mutex_thread, 
			    GINT_TO_POINTER (42),
			    0, TRUE, TRUE, G_THREAD_PRIORITY_NORMAL, NULL);
  g_usleep (G_MICROSEC);
  test_g_mutex_int = 42;
  G_UNLOCK (test_g_mutex);
  g_mutex_unlock (test_g_mutex_mutex);
  g_thread_join (thread);
  g_mutex_free (test_g_mutex_mutex);
}

/* GStaticRecMutex */

static GStaticRecMutex test_g_static_rec_mutex_mutex = G_STATIC_REC_MUTEX_INIT;
static guint test_g_static_rec_mutex_int = 0;

static void
test_g_static_rec_mutex_thread (gpointer data)
{
  g_assert (GPOINTER_TO_INT (data) == 42);
  g_assert (g_static_rec_mutex_trylock (&test_g_static_rec_mutex_mutex) 
	    == FALSE);
  g_static_rec_mutex_lock (&test_g_static_rec_mutex_mutex);
  g_static_rec_mutex_lock (&test_g_static_rec_mutex_mutex);
  g_assert (test_g_static_rec_mutex_int == 42);
  g_static_rec_mutex_unlock (&test_g_static_rec_mutex_mutex);
  g_static_rec_mutex_unlock (&test_g_static_rec_mutex_mutex);
}

static void
test_g_static_rec_mutex (void)
{
  GThread *thread;

  g_assert (g_static_rec_mutex_trylock (&test_g_static_rec_mutex_mutex));
  thread = g_thread_create (test_g_static_rec_mutex_thread, 
			    GINT_TO_POINTER (42),
			    0, TRUE, TRUE, G_THREAD_PRIORITY_NORMAL, NULL);
  g_usleep (G_MICROSEC);
  g_assert (g_static_rec_mutex_trylock (&test_g_static_rec_mutex_mutex));
  g_usleep (G_MICROSEC);
  test_g_static_rec_mutex_int = 41;
  g_static_rec_mutex_unlock (&test_g_static_rec_mutex_mutex);
  test_g_static_rec_mutex_int = 42;  
  g_static_rec_mutex_unlock (&test_g_static_rec_mutex_mutex);
  g_usleep (G_MICROSEC);
  g_static_rec_mutex_lock (&test_g_static_rec_mutex_mutex);
  test_g_static_rec_mutex_int = 0;  
  g_static_rec_mutex_unlock (&test_g_static_rec_mutex_mutex);
  g_thread_join (thread);
}

/* GStaticPrivate */

#define THREADS 10

static GStaticPrivate test_g_static_private_private = G_STATIC_PRIVATE_INIT;
static GStaticMutex test_g_static_private_mutex = G_STATIC_MUTEX_INIT;
static guint test_g_static_private_counter = 0;

static gpointer
test_g_static_private_constructor (void)
{
  g_static_mutex_lock (&test_g_static_private_mutex);
  test_g_static_private_counter++;
  g_static_mutex_unlock (&test_g_static_private_mutex);  
  return g_new (guint,1);
}

static void
test_g_static_private_destructor (gpointer data)
{
  g_static_mutex_lock (&test_g_static_private_mutex);
  test_g_static_private_counter--;
  g_static_mutex_unlock (&test_g_static_private_mutex);  
  g_free (data);
}


static void
test_g_static_private_thread (gpointer data)
{
  guint number = GPOINTER_TO_INT (data);
  guint i;
  guint* private;
  for (i = 0; i < 10; i++)
    {
      number = number * 11 + 1; /* A very simple and bad RNG ;-) */
      private = g_static_private_get (&test_g_static_private_private);
      if (!private || number % 7 > 3)
	{
	  private = test_g_static_private_constructor ();
	  g_static_private_set (&test_g_static_private_private, private,
				test_g_static_private_destructor);
	}
      *private = number;
      g_usleep (G_MICROSEC / 5);
      g_assert (number == *private);
    }
}

static void
test_g_static_private (void)
{
  GThread *threads[THREADS];
  guint i;
  for (i = 0; i < THREADS; i++)
    {
      threads[i] = g_thread_create (test_g_static_private_thread, 
				    GINT_TO_POINTER (i),
				    0, TRUE, TRUE, 
				    G_THREAD_PRIORITY_NORMAL, NULL);      
    }
  for (i = 0; i < THREADS; i++)
    {
      g_thread_join (threads[i]);
    }
  g_assert (test_g_static_private_counter == 0); 
}

/* GStaticRWLock */

/* -1 = writing; >0 = # of readers */
static gint test_g_static_rw_lock_state = 0; 
G_LOCK_DEFINE (test_g_static_rw_lock_state);

static gboolean test_g_static_rw_lock_run = TRUE; 
static GStaticRWLock test_g_static_rw_lock_lock = G_STATIC_RW_LOCK_INIT;

static void
test_g_static_rw_lock_thread (gpointer data)
{
  while (test_g_static_rw_lock_run)
    {
      if (g_random_double() > .2) /* I'm a reader */
	{
	  
	  if (g_random_double() > .2) /* I'll block */
	    g_static_rw_lock_reader_lock (&test_g_static_rw_lock_lock);
	  else /* I'll only try */
	    if (!g_static_rw_lock_reader_trylock (&test_g_static_rw_lock_lock))
	      continue;
	  G_LOCK (test_g_static_rw_lock_state);
	  g_assert (test_g_static_rw_lock_state >= 0);
	  test_g_static_rw_lock_state++;
	  G_UNLOCK (test_g_static_rw_lock_state);

	  g_usleep (10);

	  G_LOCK (test_g_static_rw_lock_state);
	  test_g_static_rw_lock_state--;
	  G_UNLOCK (test_g_static_rw_lock_state);

	  g_static_rw_lock_reader_unlock (&test_g_static_rw_lock_lock);
	}
      else /* I'm a writer */
	{
	  
	  if (g_random_double() > .2) /* I'll block */ 
	    g_static_rw_lock_writer_lock (&test_g_static_rw_lock_lock);
	  else /* I'll only try */
	    if (!g_static_rw_lock_writer_trylock (&test_g_static_rw_lock_lock))
	      continue;
	  G_LOCK (test_g_static_rw_lock_state);
	  g_assert (test_g_static_rw_lock_state == 0);
	  test_g_static_rw_lock_state = -1;
	  G_UNLOCK (test_g_static_rw_lock_state);

	  g_usleep (10);

	  G_LOCK (test_g_static_rw_lock_state);
	  test_g_static_rw_lock_state = 0;
	  G_UNLOCK (test_g_static_rw_lock_state);

	  g_static_rw_lock_writer_unlock (&test_g_static_rw_lock_lock);
	}
    }
}

static void
test_g_static_rw_lock ()
{
  GThread *threads[THREADS];
  guint i;
  for (i = 0; i < THREADS; i++)
    {
      threads[i] = g_thread_create (test_g_static_rw_lock_thread, 
				    0, 0, TRUE, TRUE, 
				    G_THREAD_PRIORITY_NORMAL, NULL);      
    }
  g_usleep (G_MICROSEC);
  test_g_static_rw_lock_run = FALSE;
  for (i = 0; i < THREADS; i++)
    {
      g_thread_join (threads[i]);
    }
  g_assert (test_g_static_rw_lock_state == 0);
}

/* run all the tests */
void
run_all_tests()
{
  test_g_mutex ();
  test_g_static_rec_mutex ();
  test_g_static_private ();
  test_g_static_rw_lock ();  
}

int 
main (int   argc,
      char *argv[])
{
  /* Only run the test, if threads are enabled and a default thread
     implementation is available */
#if defined(G_THREADS_ENABLED) && ! defined(G_THREADS_IMPL_NONE)
  g_thread_init (NULL);
  run_all_tests ();

  /* Now we rerun all tests, but this time we fool the system into
   * thinking, that the available thread system is not native, but
   * userprovided. */

  g_thread_use_default_impl = FALSE;
  run_all_tests ();
  
#endif
  return 0;
}
