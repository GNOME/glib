/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gthread.c: solaris thread system implementation
 * Copyright 1998 Sebastian Wilhelmi; University of Karlsruhe
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* 
 * MT safe
 */

#include <thread.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

/* we can't use g_log in the g_thread functions, because g_log depends
   on these functions to work properly, we even should not use
   g_strerror (might be a FIXME) */

#define solaris_error_args( name, num )                                  \
  "Gthread: Fatal error in file %s: line %d (%s): error %s during %s\n", \
  __FILE__, __LINE__, G_GNUC_PRETTY_FUNCTION,                            \
  g_strerror((num)), #name

#define solaris_check_for_error( what ) G_STMT_START{                    \
    int error = (what);                                                  \
    if( error ) {                                                        \
      fprintf(stderr, solaris_error_args( what, error ) );               \
      exit(1);                                                           \
    }                                                                    \
  }G_STMT_END

static GMutex *
g_mutex_new_solaris_impl (void)
{
  /* we can not use g_new and friends, as they might use mutexes by
     themself */
  GMutex *result = malloc (sizeof (mutex_t));
  solaris_check_for_error (result?NULL:ENOMEM);
  solaris_check_for_error (mutex_init ((mutex_t *) result, USYNC_PROCESS, 0));
  return result;
}

static void
g_mutex_free_solaris_impl (GMutex * mutex)
{
  solaris_check_for_error (mutex_destroy ((mutex_t *) mutex));
  free (mutex);
}

/* mutex_lock, mutex_unlock can be taken directly, as
   signature and semantic are right, but without error check then!!!!,
   we might want to change this therefore. */

static gboolean
g_mutex_trylock_solaris_impl (GMutex * mutex)
{
  int result;
  result = mutex_trylock ((mutex_t *) mutex);
  if (result == EBUSY)
    return FALSE;
  solaris_check_for_error (result);
  return TRUE;
}

static GCond *
g_cond_new_solaris_impl ()
{
  GCond *result = (GCond *) g_new0 (cond_t, 1);
  solaris_check_for_error (cond_init ((cond_t *) result, USYNC_THREAD, 0));
  return result;
}

/* cond_signal, cond_broadcast and cond_wait
   can be taken directly, as signature and semantic are right, but
   without error check then!!!!, we might want to change this
   therfore. */

#define G_MICROSEC 1000000
#define G_NANOSEC 1000000000

static gboolean
g_cond_timed_wait_solaris_impl (GCond * cond, GMutex * entered_mutex,
				GTimeVal * abs_time)
{
  int result;
  timestruc_t end_time;
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
  result = cond_timedwait ((cond_t *) cond, (mutex_t *) entered_mutex,
			   &end_time);
  timed_out = (result == ETIME);
  if (!timed_out)
    solaris_check_for_error (result);
  return !timed_out;
}

static void
g_cond_free_solaris_impl (GCond * cond)
{
  solaris_check_for_error (cond_destroy ((cond_t *) cond));
  g_free (cond);
}

static GPrivate *
g_private_new_solaris_impl (GDestroyNotify destructor)
{
  /* we can not use g_new and friends, as they might use private data
     by themself */
  GPrivate *result = malloc (sizeof (thread_key_t));

  solaris_check_for_error (malloc ? NULL : ENOMEM);

  solaris_check_for_error (thr_keycreate ((thread_key_t *) result,
					  destructor));
  return result;
}

void
g_private_set_solaris_impl (GPrivate * private, gpointer value)
{
  solaris_check_for_error (private ? NULL : EINVAL);

  solaris_check_for_error (thr_setspecific (*(thread_key_t *) private, value));
}

gpointer
g_private_get_solaris_impl (GPrivate * private)
{
  gpointer result;

  solaris_check_for_error (private ? NULL : EINVAL);

  solaris_check_for_error (thr_getspecific (*(thread_key_t *) private,
					    &result));
  return result;
}

static GThreadFunctions
  g_thread_functions_for_glib_use_default =
{
  g_mutex_new_solaris_impl,
  (void (*)(GMutex *)) mutex_lock,
  g_mutex_trylock_solaris_impl,
  (void (*)(GMutex *)) mutex_unlock,
  g_mutex_free_solaris_impl,
  g_cond_new_solaris_impl,
  (void (*)(GCond *)) cond_signal,
  (void (*)(GCond *)) cond_broadcast,
  (void (*)(GCond *, GMutex *)) cond_wait,
  g_cond_timed_wait_solaris_impl,
  g_cond_free_solaris_impl,
  g_private_new_solaris_impl,
  g_private_get_solaris_impl,
  g_private_set_solaris_impl
};
