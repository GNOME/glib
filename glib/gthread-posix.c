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

/* The GMutex, GCond and GPrivate implementations in this file are some
 * of the lowest-level code in GLib.  All other parts of GLib (messages,
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
#include "gthreadprivate.h"

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

/**
 * g_mutex_init:
 * @mutex: an uninitialized #GMutex
 *
 * Initializes a #GMutex so that it can be used.
 *
 * This function is useful to initialize a mutex that has been
 * allocated on the stack, or as part of a larger structure.
 * It is not necessary to initialize a mutex that has been
 * created with g_mutex_new(). Also see #G_MUTEX_INITIALIZER
 * for an alternative way to initialize statically allocated mutexes.
 *
 * |[
 *   typedef struct {
 *     GMutex m;
 *     /&ast; ... &ast;/
 *   } Blob;
 *
 * Blob *b;
 *
 * b = g_new (Blob, 1);
 * g_mutex_init (&b->m);
 * /&ast; ... &ast;/
 * ]|
 *
 * To undo the effect of g_mutex_init() when a mutex is no longer
 * needed, use g_mutex_clear().
 *
 * Since: 2.32
 */
void
g_mutex_init (GMutex *mutex)
{
  gint status;

  if G_UNLIKELY ((status = pthread_mutex_init (&mutex->impl, NULL)) != 0)
    g_thread_abort (status, "pthread_mutex_init");
}

/**
 * g_mutex_clear:
 * @mutex: an initialized #GMutex
 *
 * Frees the resources allocated to a mutex with g_mutex_init().
 *
 * #GMutexes that have have been created with g_mutex_new() should
 * be freed with g_mutex_free() instead.
 *
 * Sine: 2.32
 */
void
g_mutex_clear (GMutex *mutex)
{
  gint status;

  if G_UNLIKELY ((status = pthread_mutex_destroy (&mutex->impl)) != 0)
    g_thread_abort (status, "pthread_mutex_destroy");
}

/**
 * g_mutex_lock:
 * @mutex: a #GMutex
 *
 * Locks @mutex. If @mutex is already locked by another thread, the
 * current thread will block until @mutex is unlocked by the other
 * thread.
 *
 * This function can be used even if g_thread_init() has not yet been
 * called, and, in that case, will do nothing.
 *
 * <note>#GMutex is neither guaranteed to be recursive nor to be
 * non-recursive, i.e. a thread could deadlock while calling
 * g_mutex_lock(), if it already has locked @mutex. Use
 * #GStaticRecMutex, if you need recursive mutexes.</note>
 */
void
g_mutex_lock (GMutex *mutex)
{
  gint status;

  if G_UNLIKELY ((status = pthread_mutex_lock (&mutex->impl)) != 0)
    g_thread_abort (status, "pthread_mutex_lock");
}

/**
 * g_mutex_unlock:
 * @mutex: a #GMutex
 *
 * Unlocks @mutex. If another thread is blocked in a g_mutex_lock()
 * call for @mutex, it will be woken and can lock @mutex itself.
 *
 * This function can be used even if g_thread_init() has not yet been
 * called, and, in that case, will do nothing.
 */
void
g_mutex_unlock (GMutex *mutex)
{
  gint status;

  if G_UNLIKELY ((status = pthread_mutex_unlock (&mutex->impl)) != 0)
    g_thread_abort (status, "pthread_mutex_lock");
}

/**
 * g_mutex_trylock:
 * @mutex: a #GMutex
 *
 * Tries to lock @mutex. If @mutex is already locked by another thread,
 * it immediately returns %FALSE. Otherwise it locks @mutex and returns
 * %TRUE.
 *
 * This function can be used even if g_thread_init() has not yet been
 * called, and, in that case, will immediately return %TRUE.
 *
 * <note>#GMutex is neither guaranteed to be recursive nor to be
 * non-recursive, i.e. the return value of g_mutex_trylock() could be
 * both %FALSE or %TRUE, if the current thread already has locked
 * @mutex. Use #GStaticRecMutex, if you need recursive
 * mutexes.</note>

 * Returns: %TRUE, if @mutex could be locked
 */
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

/**
 * g_cond_init:
 * @cond: an uninitialized #GCond
 *
 * Initialized a #GCond so that it can be used.
 *
 * This function is useful to initialize a #GCond that has been
 * allocated on the stack, or as part of a larger structure.
 * It is not necessary to initialize a #GCond that has been
 * created with g_cond_new(). Also see #G_COND_INITIALIZER
 * for an alternative way to initialize statically allocated
 * #GConds.
 *
 * Since: 2.32
 */
void
g_cond_init (GCond *cond)
{
  gint status;

  if G_UNLIKELY ((status = pthread_cond_init (&cond->impl, NULL)) != 0)
    g_thread_abort (status, "pthread_cond_init");
}

/**
 * g_cond_clear:
 * @cond: an initialized #GCond
 *
 * Frees the resources allocated ot a #GCond with g_cond_init().
 *
 * #GConds that have been created with g_cond_new() should
 * be freed with g_cond_free() instead.
 *
 * Since: 2.32
 */
void
g_cond_clear (GCond *cond)
{
  gint status;

  if G_UNLIKELY ((status = pthread_cond_destroy (&cond->impl)) != 0)
    g_thread_abort (status, "pthread_cond_destroy");
}

/**
 * g_cond_wait:
 * @cond: a #GCond
 * @mutex: a #GMutex that is currently locked
 *
 * Waits until this thread is woken up on @cond.
 * The @mutex is unlocked before falling asleep
 * and locked again before resuming.
 *
 * This function can be used even if g_thread_init() has not yet been
 * called, and, in that case, will immediately return.
 */
void
g_cond_wait (GCond  *cond,
             GMutex *mutex)
{
  gint status;

  if G_UNLIKELY ((status = pthread_cond_wait (&cond->impl, &mutex->impl)) != 0)
    g_thread_abort (status, "pthread_cond_wait");
}

/**
 * g_cond_signal:
 * @cond: a #GCond
 *
 * If threads are waiting for @cond, exactly one of them is woken up.
 * It is good practice to hold the same lock as the waiting thread
 * while calling this function, though not required.
 *
 * This function can be used even if g_thread_init() has not yet been
 * called, and, in that case, will do nothing.
 */
void
g_cond_signal (GCond *cond)
{
  gint status;

  if G_UNLIKELY ((status = pthread_cond_signal (&cond->impl)) != 0)
    g_thread_abort (status, "pthread_cond_signal");
}

/**
 * g_cond_broadcast:
 * @cond: a #GCond
 *
 * If threads are waiting for @cond, all of them are woken up.
 * It is good practice to lock the same mutex as the waiting threads
 * while calling this function, though not required.
 *
 * This function can be used even if g_thread_init() has not yet been
 * called, and, in that case, will do nothing.
 */
void
g_cond_broadcast (GCond *cond)
{
  gint status;

  if G_UNLIKELY ((status = pthread_cond_broadcast (&cond->impl)) != 0)
    g_thread_abort (status, "pthread_cond_broadcast");
}

/**
 * g_cond_timed_wait:
 * @cond: a #GCond
 * @mutex: a #GMutex that is currently locked
 * @abs_time: a #GTimeVal, determining the final time
 *
 * Waits until this thread is woken up on @cond, but not longer than
 * until the time specified by @abs_time. The @mutex is unlocked before
 * falling asleep and locked again before resuming.
 *
 * If @abs_time is %NULL, g_cond_timed_wait() acts like g_cond_wait().
 *
 * This function can be used even if g_thread_init() has not yet been
 * called, and, in that case, will immediately return %TRUE.
 *
 * To easily calculate @abs_time a combination of g_get_current_time()
 * and g_time_val_add() can be used.
 *
 * Returns: %TRUE if @cond was signalled, or %FALSE on timeout
 */
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

/**
 * g_cond_timedwait:
 * @cond: a #GCond
 * @mutex: a #GMutex that is currently locked
 * @abs_time: the final time, in microseconds
 *
 * A variant of g_cond_timed_wait() that takes @abs_time
 * as a #gint64 instead of a #GTimeVal.
 * See g_cond_timed_wait() for details.
 *
 * Returns: %TRUE if @cond was signalled, or %FALSE on timeout
 *
 * Since: 2.32
 */
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

void
g_private_init (GPrivate       *key,
                GDestroyNotify  notify)
{
  pthread_key_create (&key->key, notify);
  key->ready = TRUE;
}

/**
 * g_private_get:
 * @private_key: a #GPrivate
 *
 * Returns the pointer keyed to @private_key for the current thread. If
 * g_private_set() hasn't been called for the current @private_key and
 * thread yet, this pointer will be %NULL.
 *
 * This function can be used even if g_thread_init() has not yet been
 * called, and, in that case, will return the value of @private_key
 * casted to #gpointer. Note however, that private data set
 * <emphasis>before</emphasis> g_thread_init() will
 * <emphasis>not</emphasis> be retained <emphasis>after</emphasis> the
 * call. Instead, %NULL will be returned in all threads directly after
 * g_thread_init(), regardless of any g_private_set() calls issued
 * before threading system initialization.
 *
 * Returns: the corresponding pointer
 */
gpointer
g_private_get (GPrivate *key)
{
  if (!key->ready)
    return key->single_value;

  /* quote POSIX: No errors are returned from pthread_getspecific(). */
  return pthread_getspecific (key->key);
}

/**
 * g_private_set:
 * @private_key: a #GPrivate
 * @data: the new pointer
 *
 * Sets the pointer keyed to @private_key for the current thread.
 *
 * This function can be used even if g_thread_init() has not yet been
 * called, and, in that case, will set @private_key to @data casted to
 * #GPrivate*. See g_private_get() for resulting caveats.
 */
void
g_private_set (GPrivate *key,
               gpointer  value)
{
  gint status;

  if (!key->ready)
    {
      key->single_value = value;
      return;
    }

  if G_UNLIKELY ((status = pthread_setspecific (key->key, value)) != 0)
    g_thread_abort (status, "pthread_setspecific");
}

/* {{{1 GThread */

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

static gulong g_thread_min_stack_size = 0;

#define G_MUTEX_SIZE (sizeof (pthread_mutex_t))

void
_g_thread_impl_init(void)
{
#ifdef _SC_THREAD_STACK_MIN
  g_thread_min_stack_size = MAX (sysconf (_SC_THREAD_STACK_MIN), 0);
#endif /* _SC_THREAD_STACK_MIN */
}

void
g_system_thread_create (GThreadFunc       thread_func,
                        gpointer          arg,
                        gulong            stack_size,
                        gboolean          joinable,
                        gboolean          bound,
                        GThreadPriority   priority,
                        gpointer          thread,
                        GError          **error)
{
  pthread_attr_t attr;
  gint ret;

  g_return_if_fail (thread_func);

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

/**
 * g_thread_yield:
 *
 * Gives way to other threads waiting to be scheduled.
 *
 * This function is often used as a method to make busy wait less evil.
 * But in most cases you will encounter, there are better methods to do
 * that. So in general you shouldn't use this function.
 */
void
g_thread_yield (void)
{
  sched_yield ();
}

void
g_system_thread_join (gpointer thread)
{
  gpointer ignore;
  posix_check_cmd (pthread_join (*(pthread_t*)thread, &ignore));
}

void
g_system_thread_exit (void)
{
  pthread_exit (NULL);
}

void
g_system_thread_self (gpointer thread)
{
  *(pthread_t*)thread = pthread_self();
}

gboolean
g_system_thread_equal (gpointer thread1,
                       gpointer thread2)
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
  g_private_new,
  g_private_get,
  g_private_set,
  NULL,
  g_thread_yield,
  NULL,
  g_system_thread_exit,
  NULL,
  NULL,
  g_system_thread_equal,
};

/* vim:set foldmethod=marker: */
