/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gthread.c: posix thread system implementation
 * Copyright 1998 Sebastian Wilhelmi; University of Karlsruhe
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GLib Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

/* 
 * MT safe
 */

#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#if GLIB_SIZEOF_PTHREAD_T == 2
#define PTHREAD_T_CAST_INT gint16
#elif GLIB_SIZEOF_PTHREAD_T == 4
#define PTHREAD_T_CAST_INT gint32
#elif GLIB_SIZEOF_PTHREAD_T == 8 && defined(G_HAVE_GINT64)
#define PTHREAD_T_CAST_INT gint64
#else
# error This should not happen. Contact the GLib team.
#endif

#define GPOINTER_TO_PTHREAD_T(x) ((pthread_t)(PTHREAD_T_CAST_INT)(x))
#define PTHREAD_T_TO_GPOINTER(x) ((gpointer)(PTHREAD_T_CAST_INT)(x))

#define posix_print_error( name, num )                          \
  g_error( "file %s: line %d (%s): error %s during %s",         \
           __FILE__, __LINE__, G_GNUC_PRETTY_FUNCTION,          \
           g_strerror((num)), #name )

#if defined(G_THREADS_IMPL_POSIX)
# define posix_check_for_error( what ) G_STMT_START{             \
    int error = (what);                                           \
    if( error ) { posix_print_error( what, error ); }             \
    }G_STMT_END
# define mutexattr_default NULL
# define condattr_default NULL
#elif defined(G_THREADS_IMPL_DCE)
# define posix_check_for_error( what ) G_STMT_START{             \
    if( (what) == -1 ) { posix_print_error( what, errno ); }       \
    }G_STMT_END
# define pthread_key_create(a, b) pthread_keycreate (a, b)
# define pthread_attr_init(a) pthread_attr_create (a)
# define pthread_attr_destroy(a) pthread_attr_delete (a)
# define pthread_create(a, b, c, d) pthread_create(a, &b, c, d) 
# define mutexattr_default (&pthread_mutexattr_default)
# define condattr_default (&pthread_condattr_default)
#else /* neither G_THREADS_IMPL_POSIX nor G_THREADS_IMPL_DCE are defined */
# error This should not happen. Contact the GLib team.
#endif

#define HAVE_G_THREAD_IMPL_INIT
static void 
g_thread_impl_init()
{
  g_thread_min_priority = POSIX_MIN_PRIORITY;
  g_thread_max_priority = POSIX_MAX_PRIORITY;
}

static GMutex *
g_mutex_new_posix_impl (void)
{
  GMutex *result = (GMutex *) g_new (pthread_mutex_t, 1);
  posix_check_for_error (pthread_mutex_init ((pthread_mutex_t *) result, 
					     mutexattr_default));
  return result;
}

static void
g_mutex_free_posix_impl (GMutex * mutex)
{
  posix_check_for_error (pthread_mutex_destroy ((pthread_mutex_t *) mutex));
  g_free (mutex);
}

/* NOTE: the functions g_mutex_lock and g_mutex_unlock may not use
   functions from gmem.c and gmessages.c; */

/* pthread_mutex_lock, pthread_mutex_unlock can be taken directly, as
   signature and semantic are right, but without error check then!!!!,
   we might want to change this therefore. */

static gboolean
g_mutex_trylock_posix_impl (GMutex * mutex)
{
  int result;

  result = pthread_mutex_trylock ((pthread_mutex_t *) mutex);

#ifdef G_THREADS_IMPL_POSIX
  if (result == EBUSY)
    return FALSE;
#else /* G_THREADS_IMPL_DCE */
  if (result == 0)
    return FALSE;
#endif

  posix_check_for_error (result);
  return TRUE;
}

static GCond *
g_cond_new_posix_impl (void)
{
  GCond *result = (GCond *) g_new (pthread_cond_t, 1);
  posix_check_for_error (pthread_cond_init ((pthread_cond_t *) result, 
					    condattr_default));
  return result;
}

/* pthread_cond_signal, pthread_cond_broadcast and pthread_cond_wait
   can be taken directly, as signature and semantic are right, but
   without error check then!!!!, we might want to change this
   therfore. */

#define G_MICROSEC 1000000
#define G_NANOSEC 1000000000

static gboolean
g_cond_timed_wait_posix_impl (GCond * cond,
			      GMutex * entered_mutex,
			      GTimeVal * abs_time)
{
  int result;
  struct timespec end_time;
  gboolean timed_out;

  g_return_val_if_fail (cond != NULL, FALSE);
  g_return_val_if_fail (entered_mutex != NULL, FALSE);

  if (!abs_time)
    {
      g_cond_wait (cond, entered_mutex);
      return TRUE;
    }

  end_time.tv_sec = abs_time->tv_sec;
  end_time.tv_nsec = abs_time->tv_usec * (G_NANOSEC / G_MICROSEC);
  g_assert (end_time.tv_nsec < G_NANOSEC);
  result = pthread_cond_timedwait ((pthread_cond_t *) cond,
				   (pthread_mutex_t *) entered_mutex,
				   &end_time);

#ifdef G_THREADS_IMPL_POSIX
  timed_out = (result == ETIMEDOUT);
#else /* G_THREADS_IMPL_DCE */
  timed_out = (result == -1) && (errno = EAGAIN);
#endif

  if (!timed_out)
    posix_check_for_error (result);
  return !timed_out;
}

static void
g_cond_free_posix_impl (GCond * cond)
{
  posix_check_for_error (pthread_cond_destroy ((pthread_cond_t *) cond));
  g_free (cond);
}

static GPrivate *
g_private_new_posix_impl (GDestroyNotify destructor)
{
  GPrivate *result = (GPrivate *) g_new (pthread_key_t, 1);
  posix_check_for_error (pthread_key_create ((pthread_key_t *) result,
					     destructor));
  return result;
}

/* NOTE: the functions g_private_get and g_private_set may not use
   functions from gmem.c and gmessages.c */

static void
g_private_set_posix_impl (GPrivate * private_key, gpointer value)
{
  if (!private_key)
    return;
  pthread_setspecific (*(pthread_key_t *) private_key, value);
}

static gpointer
g_private_get_posix_impl (GPrivate * private_key)
{
  if (!private_key)
    return NULL;
#ifdef G_THREADS_IMPL_POSIX
  return pthread_getspecific (*(pthread_key_t *) private_key);
#else /* G_THREADS_IMPL_DCE */
  {
    void* data;
    posix_check_for_error (pthread_getspecific (*(pthread_key_t *) 
						private_key, &data));
    return data;
  }
#endif
}

gpointer 
g_thread_create_posix_impl (GThreadFunc thread_func, 
			    gpointer arg, 
			    gulong stack_size,
			    gboolean joinable,
			    gboolean bound,
			    GThreadPriority priority)
{     
  pthread_t thread;
  pthread_attr_t attr;
  struct sched_param sched;

  g_return_val_if_fail (thread_func, NULL);

  posix_check_for_error (pthread_attr_init (&attr));
  
#ifdef HAVE_PTHREAD_ATTR_SETSTACKSIZE
  if (stack_size)
      posix_check_for_error (pthread_attr_setstacksize (&attr, stack_size));
#endif /* HAVE_PTHREAD_ATTR_SETSTACKSIZE */

#ifdef _POSIX_THREAD_PRIORITY_SCHEDULING
  if (bound)
     posix_check_for_error (pthread_attr_setscope (&attr, 
						   PTHREAD_SCOPE_SYSTEM));
#endif

  posix_check_for_error( pthread_attr_setdetachstate( &attr,
          joinable ? PTHREAD_CREATE_JOINABLE : PTHREAD_CREATE_DETACHED ) );
  
#ifdef G_THREADS_IMPL_POSIX
  posix_check_for_error (pthread_attr_getschedparam (&attr, &sched));
  sched.sched_priority = g_thread_map_priority (priority);
  posix_check_for_error (pthread_attr_setschedparam (&attr, &sched));
#else /* G_THREADS_IMPL_DCE */
  posix_check_for_error 
    (pthread_attr_setprio (&attr, g_thread_map_priority (priority));
#endif

  posix_check_for_error( pthread_create (&thread, &attr, 
                                         (void* (*)(void*))thread_func,
                                         arg) );
  
  posix_check_for_error( pthread_attr_destroy (&attr) );

  return PTHREAD_T_TO_GPOINTER (thread);
}

void 
g_thread_yield_posix_impl (void)
{
  POSIX_YIELD_FUNC;
}

void
g_thread_join_posix_impl (gpointer thread)
{     
  gpointer ignore;
  posix_check_for_error (pthread_join (GPOINTER_TO_PTHREAD_T (thread), 
				       &ignore));
}

void 
g_thread_exit_posix_impl (void) 
{
  pthread_exit (NULL);
}

void
g_thread_set_priority_posix_impl (gpointer thread, GThreadPriority priority)
{
  struct sched_param sched;
  int policy;

#ifdef G_THREADS_IMPL_POSIX
  posix_check_for_error (pthread_getschedparam (GPOINTER_TO_PTHREAD_T (thread), 
						&policy, &sched));
  sched.sched_priority = g_thread_map_priority (priority);
  posix_check_for_error (pthread_setschedparam (GPOINTER_TO_PTHREAD_T (thread), 
						policy, &sched));
#else /* G_THREADS_IMPL_DCE */
  posix_check_for_error (pthread_setprio (GPOINTER_TO_PTHREAD_T (thread), 
					  g_thread_map_priority (priority)));
#endif
}


static GThreadFunctions g_thread_functions_for_glib_use_default =
{
  g_mutex_new_posix_impl,
  (void (*)(GMutex *)) pthread_mutex_lock,
  g_mutex_trylock_posix_impl,
  (void (*)(GMutex *)) pthread_mutex_unlock,
  g_mutex_free_posix_impl,
  g_cond_new_posix_impl,
  (void (*)(GCond *)) pthread_cond_signal,
  (void (*)(GCond *)) pthread_cond_broadcast,
  (void (*)(GCond *, GMutex *)) pthread_cond_wait,
  g_cond_timed_wait_posix_impl,
  g_cond_free_posix_impl,
  g_private_new_posix_impl,
  g_private_get_posix_impl,
  g_private_set_posix_impl,
  g_thread_create_posix_impl,
  g_thread_yield_posix_impl,
  g_thread_join_posix_impl,
  g_thread_exit_posix_impl,
  g_thread_set_priority_posix_impl,
  (gpointer (*)())pthread_self
};
