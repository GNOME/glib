#define main testglib_main
#include <testglib.c>
#undef main

void
test_mutexes()
{
  GMutex* mutex;
  GCond* cond;
  GStaticMutex static_mutex = G_STATIC_MUTEX_INIT;
  G_LOCK_DEFINE(test_me);

  mutex = g_mutex_new();
  cond = g_cond_new();
 
  g_mutex_lock(mutex);
  g_mutex_unlock(mutex);
  
  g_static_mutex_lock(static_mutex);
  g_static_mutex_unlock(static_mutex);
  
  g_cond_signal(cond);
  g_cond_broadcast(cond);
  
  g_lock(test_me);
  g_unlock(test_me);
  
  g_cond_free(cond);
  g_mutex_free(mutex);
}

int
main()
{ 
  test_mutexes();

  g_thread_init( NULL );

  test_mutexes();

  /* later we might want to start n copies of that */
  testglib_main(0,NULL);

  return 0;
}
