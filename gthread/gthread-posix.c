#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>

#define posix_print_error( name, num )                          \
  g_error( "file %s: line %d (%s): error %s during %s",         \
           __FILE__, __LINE__, G_GNUC_PRETTY_FUNCTION,          \
           g_strerror((num)), #name )

#define posix_check_for_error( what ) G_STMT_START{             \
  int error = (what);                                           \
  if( error ) { posix_print_error( what, error ); }             \
  }G_STMT_END

static GMutex*
g_mutex_new_posix_impl (void)
{
  /* we can not use g_new and friends, as they might use mutexes by
     themself */
  GMutex* result = malloc(sizeof(pthread_mutex_t));
  posix_check_for_error( pthread_mutex_init( (pthread_mutex_t*)result, 
					     NULL ) );
  return result;
}

static void
g_mutex_free_posix_impl (GMutex* mutex)
{
  posix_check_for_error( pthread_mutex_destroy((pthread_mutex_t*)mutex) );
  free (mutex);  
}

/* pthread_mutex_lock, pthread_mutex_unlock can be taken directly, as
   signature and semantic are right, but without error check then!!!!,
   we might want to change this therfore. */

static gboolean
g_mutex_try_lock_posix_impl (GMutex* mutex)
{
  int result;
  result = pthread_mutex_trylock((pthread_mutex_t*)mutex);
  if( result == EBUSY )
    {
      return FALSE;
    }
  posix_check_for_error( result );
  return TRUE;
}

static GCond*
g_cond_new_posix_impl (void)
{
  GCond* result = (GCond*) g_new0 (pthread_cond_t, 1);
  posix_check_for_error( pthread_cond_init( (pthread_cond_t*)result, NULL ) );
  return result;
}

/* pthread_cond_signal, pthread_cond_broadcast and pthread_cond_wait
   can be taken directly, as signature and semantic are right, but
   without error check then!!!!, we might want to change this
   therfore. */

#define G_MICROSEC 1000000
#define G_NANOSEC 1000000000

static gboolean 
g_cond_timed_wait_posix_impl (GCond*    cond, 
			      GMutex*   entered_mutex, 
			      GTimeVal* abs_time)
{
  int result;
  struct timespec end_time;
  gboolean timed_out;

  g_return_val_if_fail (cond != NULL, FALSE);
  g_return_val_if_fail (entered_mutex != NULL, FALSE);
  
  if (!abs_time)
    {
      g_cond_wait(cond, entered_mutex);
      return TRUE;
    }

  end_time.tv_sec = abs_time->tv_sec;
  end_time.tv_nsec = abs_time->tv_usec * (G_NANOSEC / G_MICROSEC);
  g_assert (end_time.tv_nsec < G_NANOSEC);
  result = pthread_cond_timedwait ((pthread_cond_t*)cond, 
				   (pthread_mutex_t*)entered_mutex, 
				   &end_time);
  timed_out = ( result == ETIMEDOUT );
  if (!timed_out) 
    posix_check_for_error (result);
  return !timed_out;
}

static void 
g_cond_free_posix_impl(GCond* cond)
{
  posix_check_for_error (pthread_cond_destroy((pthread_cond_t*)cond));
  g_free (cond);  
}

static GPrivate*
g_private_new_posix_impl(GDestroyNotify destructor)
{
  /* we can not use g_new and friends, as they might use mutexes by
     themself */
  GPrivate* result = malloc( sizeof(pthread_key_t) );
  /* FIXME: we shouldn't use g_log here actually */
  posix_check_for_error(  pthread_key_create( (pthread_key_t*)result, 
					      destructor) );
  return result;
}

void
g_private_set_posix_impl(GPrivate* private, gpointer value)
{
  g_return_if_fail (private != NULL);

  /* FIXME: we shouldn't use g_log here actually */
  posix_check_for_error( pthread_setspecific(*(pthread_key_t*)private, 
					     value) );
}

gpointer
g_private_get_posix_impl(GPrivate* private)
{
  g_return_val_if_fail (private != NULL, NULL);

  /* FIXME: we shouldn't use g_log here actually */
  return pthread_getspecific(*(pthread_key_t*)private );
}

static GThreadFunctions
g_thread_functions_for_glib_use_default = {
  g_mutex_new_posix_impl,
  (void(*)(GMutex*))pthread_mutex_lock,
  g_mutex_try_lock_posix_impl,
  (void(*)(GMutex*))pthread_mutex_unlock,
  g_mutex_free_posix_impl,
  g_cond_new_posix_impl,
  (void(*)(GCond*))pthread_cond_signal,
  (void(*)(GCond*))pthread_cond_broadcast,
  (void(*)(GCond*,GMutex*))pthread_cond_wait,
  g_cond_timed_wait_posix_impl,
  g_cond_free_posix_impl,
  g_private_new_posix_impl,
  g_private_get_posix_impl,
  g_private_set_posix_impl
};

