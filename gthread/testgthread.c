#include <stdlib.h>

#define main testglib_main
#include <testglib.c>
#undef main

#define TEST_PRIVATE_THREADS 9
#define TEST_PRIVATE_ROUNDS 5

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

#if defined(DEFAULTMUTEX) /* we are using solaris threads */
gulong
new_thread(GHookFunc func, gpointer data)
{
  thread_t thread;
  thr_create( NULL, 0, (void* (*)(void*))func, data, THR_BOUND, &thread);
  return thread;
}
#define join_thread(thread) thr_join( (thread_t)thread, NULL, NULL )
#define self_thread() (gulong)thr_self()
#elif defined(PTHREAD_MUTEX_INITIALIZER) /* we are using posix threads */
gulong
new_thread(GHookFunc func, gpointer data)
{
  pthread_t thread;
  pthread_attr_t pthread_attr;
  pthread_attr_init( pthread_attr );
  pthread_attr_setdetachstate( pthread_attr,  PTHREAD_CREATE_JOINABLE );
  pthread_create( &thread, &pthread_attr, (void* (*)(void*))func, data );
  return thread;
}
#define join_thread(thread) pthread_join( (pthread_t)thread, NULL )
#define self_thread() (gulong)pthread_self()
#else /* we are not having a thread implementation, do nothing */
#define new_thread(func,data) (0)
#define join_thread(thread) ((void)0)
#endif 

void
wait_thread(double seconds) 
{
  GTimer *timer = g_timer_new(); 
  while( g_timer_elapsed (timer, NULL) < seconds );
}

gpointer 
private_constructor()
{
  guint* result = g_new(guint,2);
  result[0] = 0;
  result[1] = self_thread();
  g_print("allocating data for the thread %d.\n",result[1]);
  return result;
}

void
private_destructor(gpointer data)
{
  guint* real = data;
  g_print("freeing data for the thread %d.\n",real[1]);  
  g_free( real ); 
}

GStaticPrivate private; 

void 
test_private_func(void* data)
{
  guint i = 0;
  while( i < TEST_PRIVATE_ROUNDS )
    {
      guint random_value = rand() % 10000;
      guint* data = g_static_private_get( &private );
      if (!data)
	{
	  data = private_constructor();
	  g_static_private_set( &private, data, private_destructor );
	}
      *data = random_value;
      wait_thread(.2);
      g_assert( *(guint*)g_static_private_get( &private ) == random_value );
      i++;
    }
}

void
test_private()
{
  int i;
  gulong threads[ TEST_PRIVATE_THREADS ];
  for( i = 0; i < TEST_PRIVATE_THREADS; i++ )
    {
      threads[ i ] = new_thread( test_private_func, (gpointer)i );
    }
  for( i = 0; i < TEST_PRIVATE_THREADS; i++ )
    {
      join_thread( threads[ i ] );
    }
  g_print("\n");
}

int
main()
{ 
  test_mutexes();

  g_thread_init( NULL );

  test_mutexes();

  test_private();
  
  /* later we might want to start n copies of that */
  testglib_main(0,NULL);

  return 0;
}
