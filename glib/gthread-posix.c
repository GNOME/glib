/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gthread.c: posix thread system implementation
 * Copyright 1998 Sebastian Wilhelmi; University of Karlsruhe
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */

/* The GMutex and GCond implementations in this file are some of the
 * lowest-level code in GLib.  All other parts of GLib (messages,
 * memory, slices, etc) assume that they can freely use these facilities
 * without risking recursion.
 *
 * As such, these functions are NOT permitted to call any other part of
 * GLib.
 *
 * The thread manipulation functions (create, exit, join, etc.) have
 * more freedom -- they can do as they please.
 */

#include "config.h"

#include "gthread.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

static void
g_thread_abort (gint         status,
                const gchar *function)
{
  fprintf (stderr, "GLib (gthread-posix.c): Unexpected error from C library during '%s': %s.  Aborting.\n",
           strerror (status), function);
  abort ();
}

/* {{{1 GMutex */
void
g_mutex_init (GMutex *mutex)
{
  gint status;

  if G_UNLIKELY ((status = pthread_mutex_init (&mutex->impl, NULL)) != 0)
    g_thread_abort (status, "pthread_mutex_init");
}

void
g_mutex_clear (GMutex *mutex)
{
  gint status;

  if G_UNLIKELY ((status = pthread_mutex_destroy (&mutex->impl)) != 0)
    g_thread_abort (status, "pthread_mutex_destroy");
}

void
g_mutex_lock (GMutex *mutex)
{
  gint status;

  if G_UNLIKELY ((status = pthread_mutex_lock (&mutex->impl)) != 0)
    g_thread_abort (status, "pthread_mutex_lock");
}

void
g_mutex_unlock (GMutex *mutex)
{
  gint status;

  if G_UNLIKELY ((status = pthread_mutex_unlock (&mutex->impl)) != 0)
    g_thread_abort (status, "pthread_mutex_lock");
}

gboolean
g_mutex_trylock (GMutex *mutex)
{
  gint status;

  if G_LIKELY ((status = pthread_mutex_trylock (&mutex->impl)) == 0)
    return TRUE;

  if G_UNLIKELY (status != EBUSY)
    g_thread_abort (status, "pthread_mutex_trylock");

  return FALSE;
}

/* {{{1 GCond */

void
g_cond_init (GCond *cond)
{
  gint status;

  if G_UNLIKELY ((status = pthread_cond_init (&cond->impl, NULL)) != 0)
    g_thread_abort (status, "pthread_cond_init");
}

void
g_cond_clear (GCond *cond)
{
  gint status;

  if G_UNLIKELY ((status = pthread_cond_destroy (&cond->impl)) != 0)
    g_thread_abort (status, "pthread_cond_destroy");
}

void
g_cond_wait (GCond  *cond,
             GMutex *mutex)
{
  gint status;

  if G_UNLIKELY ((status = pthread_cond_wait (&cond->impl, &mutex->impl)) != 0)
    g_thread_abort (status, "pthread_cond_wait");
}

void
g_cond_signal (GCond *cond)
{
  gint status;

  if G_UNLIKELY ((status = pthread_cond_signal (&cond->impl)) != 0)
    g_thread_abort (status, "pthread_cond_signal");
}

void
g_cond_broadcast (GCond *cond)
{
  gint status;

  if G_UNLIKELY ((status = pthread_cond_broadcast (&cond->impl)) != 0)
    g_thread_abort (status, "pthread_cond_broadcast");
}

gboolean
g_cond_timed_wait (GCond    *cond,
                   GMutex   *mutex,
                   GTimeVal *abs_time)
{
  struct timespec end_time;
  gint status;

  if (abs_time == NULL)
    {
      g_cond_wait (cond, mutex);
      return TRUE;
    }

  end_time.tv_sec = abs_time->tv_sec;
  end_time.tv_nsec = abs_time->tv_usec * 1000;

  if ((status = pthread_cond_timedwait (&cond->impl, &mutex->impl, &end_time)) == 0)
    return TRUE;

  if G_UNLIKELY (status != ETIMEDOUT)
    g_thread_abort (status, "pthread_cond_timedwait");

  return FALSE;
}

gboolean
g_cond_timedwait (GCond  *cond,
                  GMutex *mutex,
                  gint64  abs_time)
{
  struct timespec end_time;
  gint status;

  end_time.tv_sec = abs_time / 1000000;
  end_time.tv_nsec = (abs_time % 1000000) * 1000;

  if ((status = pthread_cond_timedwait (&cond->impl, &mutex->impl, &end_time)) == 0)
    return TRUE;

  if G_UNLIKELY (status != ETIMEDOUT)
    g_thread_abort (status, "pthread_cond_timedwait");

  return FALSE;
}

/* {{{1 GPrivate */

#include "glib.h"
#include "gthreadprivate.h"

#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_SCHED_H
#include <sched.h>
#endif

#define posix_check_err(err, name) G_STMT_START{			\
  int error = (err); 							\
  if (error)	 		 		 			\
    g_error ("file %s: line %d (%s): error '%s' during '%s'",		\
           __FILE__, __LINE__, G_STRFUNC,				\
           g_strerror (error), name);					\
  }G_STMT_END

#define posix_check_cmd(cmd) posix_check_err (cmd, #cmd)

#ifdef G_ENABLE_DEBUG
static gboolean posix_check_cmd_prio_warned = FALSE;
# define posix_check_cmd_prio(cmd) G_STMT_START{			\
    int err = (cmd);							\
    if (err == EPERM)		 		 			\
      { 	 			 				\
        if (!posix_check_cmd_prio_warned) 		 		\
          { 	 				 			\
            posix_check_cmd_prio_warned = TRUE;		 		\
            g_warning ("Priorities can only be changed " 		\
                        "(resp. increased) by root."); 			\
          }			 					\
      }			 						\
    else  		 						\
      posix_check_err (err, #cmd);					\
     }G_STMT_END
#else /* G_ENABLE_DEBUG */
# define posix_check_cmd_prio(cmd) G_STMT_START{			\
    int err = (cmd);							\
    if (err != EPERM)		 		 			\
      posix_check_err (err, #cmd);					\
     }G_STMT_END
#endif /* G_ENABLE_DEBUG */

#if defined (POSIX_MIN_PRIORITY) && defined (POSIX_MAX_PRIORITY)
# define HAVE_PRIORITIES 1
static gint priority_normal_value;
# ifdef __FreeBSD__
   /* FreeBSD threads use different priority values from the POSIX_
    * defines so we just set them here. The corresponding macros
    * PTHREAD_MIN_PRIORITY and PTHREAD_MAX_PRIORITY are implied to be
    * exported by the docs, but they aren't.
    */
#  define PRIORITY_LOW_VALUE      0
#  define PRIORITY_URGENT_VALUE   31
# else /* !__FreeBSD__ */
#  define PRIORITY_LOW_VALUE      POSIX_MIN_PRIORITY
#  define PRIORITY_URGENT_VALUE   POSIX_MAX_PRIORITY
# endif /* !__FreeBSD__ */
# define PRIORITY_NORMAL_VALUE    priority_normal_value

# define PRIORITY_HIGH_VALUE \
    ((PRIORITY_NORMAL_VALUE + PRIORITY_URGENT_VALUE * 2) / 3)

static gint
g_thread_priority_map (GThreadPriority priority)
{
  switch (priority)
    {
    case G_THREAD_PRIORITY_LOW:
      return PRIORITY_LOW_VALUE;

    case G_THREAD_PRIORITY_NORMAL:
      return PRIORITY_NORMAL_VALUE;

    case G_THREAD_PRIORITY_HIGH:
      return PRIORITY_HIGH_VALUE;

    case G_THREAD_PRIORITY_URGENT:
      return PRIORITY_URGENT_VALUE;

    default:
      g_assert_not_reached ();
    }
}

#endif /* POSIX_MIN_PRIORITY && POSIX_MAX_PRIORITY */

static gulong g_thread_min_stack_size = 0;

#define G_MUTEX_SIZE (sizeof (pthread_mutex_t))

void
_g_thread_impl_init(void)
{
#ifdef _SC_THREAD_STACK_MIN
  g_thread_min_stack_size = MAX (sysconf (_SC_THREAD_STACK_MIN), 0);
#endif /* _SC_THREAD_STACK_MIN */
#ifdef HAVE_PRIORITIES
  {
    struct sched_param sched;
    int policy;
    posix_check_cmd (pthread_getschedparam (pthread_self(), &policy, &sched));
    priority_normal_value = sched.sched_priority;
  }
#endif /* HAVE_PRIORITIES */
}

static GPrivate *
g_private_new_posix_impl (GDestroyNotify destructor)
{
  GPrivate *result = (GPrivate *) g_new (pthread_key_t, 1);
  posix_check_cmd (pthread_key_create ((pthread_key_t *) result, destructor));
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

  return pthread_getspecific (*(pthread_key_t *) private_key);
}

/* {{{1 GThread */


static void
g_thread_create_posix_impl (GThreadFunc thread_func,
			    gpointer arg,
			    gulong stack_size,
			    gboolean joinable,
			    gboolean bound,
			    GThreadPriority priority,
			    gpointer thread,
			    GError **error)
{
  pthread_attr_t attr;
  gint ret;

  g_return_if_fail (thread_func);
  g_return_if_fail (priority >= G_THREAD_PRIORITY_LOW);
  g_return_if_fail (priority <= G_THREAD_PRIORITY_URGENT);

  posix_check_cmd (pthread_attr_init (&attr));

#ifdef HAVE_PTHREAD_ATTR_SETSTACKSIZE
  if (stack_size)
    {
      stack_size = MAX (g_thread_min_stack_size, stack_size);
      /* No error check here, because some systems can't do it and
       * we simply don't want threads to fail because of that. */
      pthread_attr_setstacksize (&attr, stack_size);
    }
#endif /* HAVE_PTHREAD_ATTR_SETSTACKSIZE */

#ifdef PTHREAD_SCOPE_SYSTEM
  if (bound)
    /* No error check here, because some systems can't do it and we
     * simply don't want threads to fail because of that. */
    pthread_attr_setscope (&attr, PTHREAD_SCOPE_SYSTEM);
#endif /* PTHREAD_SCOPE_SYSTEM */

  posix_check_cmd (pthread_attr_setdetachstate (&attr,
          joinable ? PTHREAD_CREATE_JOINABLE : PTHREAD_CREATE_DETACHED));

#ifdef HAVE_PRIORITIES
  {
    struct sched_param sched;
    posix_check_cmd (pthread_attr_getschedparam (&attr, &sched));
    sched.sched_priority = g_thread_priority_map (priority);
    posix_check_cmd_prio (pthread_attr_setschedparam (&attr, &sched));
  }
#endif /* HAVE_PRIORITIES */
  ret = pthread_create (thread, &attr, (void* (*)(void*))thread_func, arg);

  posix_check_cmd (pthread_attr_destroy (&attr));

  if (ret == EAGAIN)
    {
      g_set_error (error, G_THREAD_ERROR, G_THREAD_ERROR_AGAIN, 
		   "Error creating thread: %s", g_strerror (ret));
      return;
    }

  posix_check_err (ret, "pthread_create");
}

static void
g_thread_yield_posix_impl (void)
{
  POSIX_YIELD_FUNC;
}

static void
g_thread_join_posix_impl (gpointer thread)
{
  gpointer ignore;
  posix_check_cmd (pthread_join (*(pthread_t*)thread, &ignore));
}

static void
g_thread_exit_posix_impl (void)
{
  pthread_exit (NULL);
}

static void
g_thread_set_priority_posix_impl (gpointer thread, GThreadPriority priority)
{
  g_return_if_fail (priority >= G_THREAD_PRIORITY_LOW);
  g_return_if_fail (priority <= G_THREAD_PRIORITY_URGENT);
#ifdef HAVE_PRIORITIES
  {
    struct sched_param sched;
    int policy;
    posix_check_cmd (pthread_getschedparam (*(pthread_t*)thread, &policy,
					    &sched));
    sched.sched_priority = g_thread_priority_map (priority);
    posix_check_cmd_prio (pthread_setschedparam (*(pthread_t*)thread, policy,
						 &sched));
  }
#endif /* HAVE_PRIORITIES */
}

static void
g_thread_self_posix_impl (gpointer thread)
{
  *(pthread_t*)thread = pthread_self();
}

static gboolean
g_thread_equal_posix_impl (gpointer thread1, gpointer thread2)
{
  return (pthread_equal (*(pthread_t*)thread1, *(pthread_t*)thread2) != 0);
}

/* {{{1 Epilogue */
GThreadFunctions g_thread_functions_for_glib_use =
{
  g_mutex_new,
  g_mutex_lock,
  g_mutex_trylock,
  g_mutex_unlock,
  g_mutex_free,
  g_cond_new,
  g_cond_signal,
  g_cond_broadcast,
  g_cond_wait,
  g_cond_timed_wait,
  g_cond_free,
  g_private_new_posix_impl,
  g_private_get_posix_impl,
  g_private_set_posix_impl,
  g_thread_create_posix_impl,
  g_thread_yield_posix_impl,
  g_thread_join_posix_impl,
  g_thread_exit_posix_impl,
  g_thread_set_priority_posix_impl,
  g_thread_self_posix_impl,
  g_thread_equal_posix_impl
};

/* vim:set foldmethod=marker: */
