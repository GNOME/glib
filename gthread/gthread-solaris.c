#include <thread.h>
#include <errno.h>
#include <stdlib.h>

#define solaris_print_error( name, num )                          \
  g_error( "file %s: line %d (%s): error %s during %s",         \
           __FILE__, __LINE__, G_GNUC_PRETTY_FUNCTION,          \
           g_strerror((num)), #name )

#define solaris_check_for_error( what ) G_STMT_START{             \
  int error = (what);                                           \
  if( error ) { solaris_print_error( what, error ); }             \
  }G_STMT_END

static GMutex*
g_mutex_new_solaris_impl()
{
  /* we can not use g_new and friends, as they might use mutexes by
     themself */
  GMutex* result = malloc(sizeof(mutex_t));
  solaris_check_for_error( mutex_init( (mutex_t*) result,  USYNC_PROCESS, 0));
  return result;
}

static void
g_mutex_free_solaris_impl(GMutex* mutex)
{
  solaris_check_for_error( mutex_destroy((mutex_t*)mutex) );
  free (mutex);  
}

/* mutex_lock, mutex_unlock can be taken directly, as
   signature and semantic are right, but without error check then!!!!,
   we might want to change this therfore. */

static gboolean
g_mutex_try_lock_solaris_impl(GMutex* mutex)
{
  int result;
  result = mutex_trylock((mutex_t*)mutex);
  if( result == EBUSY )
    {
      return FALSE;
    }
  solaris_check_for_error( result );
  return TRUE;
}

static GCond*
g_cond_new_solaris_impl()
{
  GCond* result = (GCond*) g_new0 (cond_t, 1);
  solaris_check_for_error( cond_init( (cond_t*)result, USYNC_THREAD, 0 ) );
  return result;
}

/* cond_signal, cond_broadcast and cond_wait
   can be taken directly, as signature and semantic are right, but
   without error check then!!!!, we might want to change this
   therfore. */

#define G_MICROSEC 1000000
#define G_NANOSEC 1000000000

static gboolean 
g_cond_timed_wait_solaris_impl(GCond* cond, GMutex* entered_mutex, 
			       GTimeVal *abs_time)
{
  int result;
  timestruc_t end_time;
  gboolean timed_out;

  g_assert( cond );
  g_assert( entered_mutex );
  
  if( !abs_time )
    {
      g_cond_wait(cond, entered_mutex);
      return TRUE;
    }
  
  end_time.tv_sec = abs_time->tv_sec;
  end_time.tv_nsec = abs_time->tv_usec * ( G_NANOSEC / G_MICROSEC );
  g_assert( end_time.tv_nsec < G_NANOSEC );
  result = cond_timedwait( (cond_t*)cond, (mutex_t*)entered_mutex, &end_time );
  timed_out = ( result == ETIME );
  if( !timed_out ) solaris_check_for_error( result );
  return !timed_out;
}

static void 
g_cond_free_solaris_impl(GCond* cond)
{
  solaris_check_for_error( cond_destroy((cond_t*)cond) );
  g_free (cond);  
}

static GPrivate*
g_private_new_solaris_impl(GDestroyNotify destructor)
{
  /* we can not use g_new and friends, as they might use mutexes by
     themself */
  GPrivate* result = malloc( sizeof(thread_key_t) );
  /* FIXME: we shouldn't use g_log here either actually */
  solaris_check_for_error( thr_keycreate( (thread_key_t*)result, destructor) );
  return result;
}

void
g_private_set_solaris_impl(GPrivate* private, gpointer value)
{
  g_assert( private );

  /* FIXME: we shouldn't use g_log here actually */
  solaris_check_for_error( thr_setspecific( *(thread_key_t*)private, value) );
}

gpointer
g_private_get_solaris_impl(GPrivate* private)
{
  gpointer result;

  g_assert( private );

  /* FIXME: we shouldn't use g_log here actually */
  solaris_check_for_error( thr_getspecific( *(thread_key_t*)private, 
					    &result) );
  return result;
}

static GThreadFunctions
g_thread_functions_for_glib_use_default = {
  g_mutex_new_solaris_impl,
  (void(*)(GMutex*))mutex_lock,
  g_mutex_try_lock_solaris_impl,
  (void(*)(GMutex*))mutex_unlock,
  g_mutex_free_solaris_impl,
  g_cond_new_solaris_impl,
  (void(*)(GCond*))cond_signal,
  (void(*)(GCond*))cond_broadcast,
  (void(*)(GCond*,GMutex*))cond_wait,
  g_cond_timed_wait_solaris_impl,
  g_cond_free_solaris_impl,
  g_private_new_solaris_impl,
  g_private_get_solaris_impl,
  g_private_set_solaris_impl
};
