/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
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

/* 
 * MT safe
 */

#include "config.h"
#include "glibconfig.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifndef G_OS_WIN32
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#endif /* G_OS_WIN32 */

#ifdef G_OS_WIN32
#include <windows.h>
#endif /* G_OS_WIN32 */

#include "glib.h"


struct _GTimer
{
#ifdef G_OS_WIN32
  DWORD start;
  DWORD end;
#else /* !G_OS_WIN32 */
  struct timeval start;
  struct timeval end;
#endif /* !G_OS_WIN32 */

  guint active : 1;
};

#ifdef G_OS_WIN32
#  define GETTIME(v) \
     v = GetTickCount ()
#else /* !G_OS_WIN32 */
#  define GETTIME(v) \
     gettimeofday (&v, NULL)
#endif /* !G_OS_WIN32 */

GTimer*
g_timer_new (void)
{
  GTimer *timer;

  timer = g_new (GTimer, 1);
  timer->active = TRUE;

  GETTIME (timer->start);

  return timer;
}

void
g_timer_destroy (GTimer *timer)
{
  g_return_if_fail (timer != NULL);

  g_free (timer);
}

void
g_timer_start (GTimer *timer)
{
  g_return_if_fail (timer != NULL);

  timer->active = TRUE;

  GETTIME (timer->start);
}

void
g_timer_stop (GTimer *timer)
{
  g_return_if_fail (timer != NULL);

  timer->active = FALSE;

  GETTIME(timer->end);
}

void
g_timer_reset (GTimer *timer)
{
  g_return_if_fail (timer != NULL);

  GETTIME (timer->start);
}

void
g_timer_continue (GTimer *timer)
{
#ifdef G_OS_WIN32
  DWORD elapsed;
#else
  struct timeval elapsed;
#endif /* G_OS_WIN32 */

  g_return_if_fail (timer != NULL);
  g_return_if_fail (timer->active == FALSE);

  /* Get elapsed time and reset timer start time
   *  to the current time minus the previously
   *  elapsed interval.
   */

#ifdef G_OS_WIN32

  elapsed = timer->end - timer->start;

  GETTIME (timer->start);

  timer->start -= elapsed;

#else /* !G_OS_WIN32 */

  if (timer->start.tv_usec > timer->end.tv_usec)
    {
      timer->end.tv_usec += G_USEC_PER_SEC;
      timer->end.tv_sec--;
    }

  elapsed.tv_usec = timer->end.tv_usec - timer->start.tv_usec;
  elapsed.tv_sec = timer->end.tv_sec - timer->start.tv_sec;

  GETTIME (timer->start);

  if (timer->start.tv_usec < elapsed.tv_usec)
    {
      timer->start.tv_usec += G_USEC_PER_SEC;
      timer->start.tv_sec--;
    }

  timer->start.tv_usec -= elapsed.tv_usec;
  timer->start.tv_sec -= elapsed.tv_sec;

#endif /* !G_OS_WIN32 */

  timer->active = TRUE;
}

gdouble
g_timer_elapsed (GTimer *timer,
		 gulong *microseconds)
{
  gdouble total;
#ifndef G_OS_WIN32
  struct timeval elapsed;
#endif /* G_OS_WIN32 */

  g_return_val_if_fail (timer != NULL, 0);

#ifdef G_OS_WIN32
  if (timer->active)
    timer->end = GetTickCount ();

  /* Check for wraparound, which happens every 49.7 days. */
  if (timer->end < timer->start)
    total = (UINT_MAX - (timer->start - timer->end - 1)) / 1000.0;
  else
    total = (timer->end - timer->start) / 1000.0;

  if (microseconds)
    {
      if (timer->end < timer->start)
	*microseconds =
	  ((UINT_MAX - (timer->start - timer->end - 1)) % 1000) * 1000;
      else
	*microseconds =
	  ((timer->end - timer->start) % 1000) * 1000;
    }
#else /* !G_OS_WIN32 */
  if (timer->active)
    gettimeofday (&timer->end, NULL);

  if (timer->start.tv_usec > timer->end.tv_usec)
    {
      timer->end.tv_usec += G_USEC_PER_SEC;
      timer->end.tv_sec--;
    }

  elapsed.tv_usec = timer->end.tv_usec - timer->start.tv_usec;
  elapsed.tv_sec = timer->end.tv_sec - timer->start.tv_sec;

  total = elapsed.tv_sec + ((gdouble) elapsed.tv_usec / 1e6);
  if (total < 0)
    {
      total = 0;

      if (microseconds)
	*microseconds = 0;
    }
  else if (microseconds)
    *microseconds = elapsed.tv_usec;

#endif /* !G_OS_WIN32 */

  return total;
}

void
g_usleep (gulong microseconds)
{
#ifdef G_OS_WIN32
  Sleep (microseconds / 1000);
#else /* !G_OS_WIN32 */
# ifdef HAVE_NANOSLEEP
  struct timespec request, remaining;
  request.tv_sec = microseconds / G_USEC_PER_SEC;
  request.tv_nsec = 1000 * (microseconds % G_USEC_PER_SEC);
  while (nanosleep (&request, &remaining) == EINTR)
    request = remaining;
# else /* !HAVE_NANOSLEEP */
  if (g_thread_supported ())
    {
      static GStaticMutex mutex = G_STATIC_MUTEX_INIT;
      static GCond* cond = NULL;
      GTimeVal end_time;
      
      g_get_current_time (&end_time);
      if (microseconds > G_MAXLONG)
	{
	  microseconds -= G_MAXLONG;
	  g_time_val_add (&end_time, G_MAXLONG);
	}
      g_time_val_add (&end_time, microseconds);

      g_static_mutex_lock (&mutex);
      
      if (!cond)
	cond = g_cond_new ();
      
      while (g_cond_timed_wait (cond, g_static_mutex_get_mutex (&mutex), 
				&end_time))
	/* do nothing */;
      
      g_static_mutex_unlock (&mutex);
    }
  else
    {
      struct timeval tv;
      tv.tv_sec = microseconds / G_USEC_PER_SEC;
      tv.tv_usec = microseconds % G_USEC_PER_SEC;
      select(0, NULL, NULL, NULL, &tv);
    }
# endif /* !HAVE_NANOSLEEP */
#endif /* !G_OS_WIN32 */
}

/**
 * g_time_val_add:
 * @time_: a #GTimeVal
 * @microseconds: number of microseconds to add to @time
 *
 * Adds the given number of microseconds to @time_. @microseconds can
 * also be negative to decrease the value of @time_.
 **/
void 
g_time_val_add (GTimeVal *time_, glong microseconds)
{
  g_return_if_fail (time_->tv_usec >= 0 && time_->tv_usec < G_USEC_PER_SEC);

  if (microseconds >= 0)
    {
      time_->tv_usec += microseconds % G_USEC_PER_SEC;
      time_->tv_sec += microseconds / G_USEC_PER_SEC;
      if (time_->tv_usec >= G_USEC_PER_SEC)
       {
         time_->tv_usec -= G_USEC_PER_SEC;
         time_->tv_sec++;
       }
    }
  else
    {
      microseconds *= -1;
      time_->tv_usec -= microseconds % G_USEC_PER_SEC;
      time_->tv_sec -= microseconds / G_USEC_PER_SEC;
      if (time_->tv_usec < 0)
       {
         time_->tv_usec += G_USEC_PER_SEC;
         time_->tv_sec--;
       }      
    }
}
