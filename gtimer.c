/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
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
#include <sys/time.h>
#include <unistd.h>
#include "glib.h"


typedef struct _GRealTimer GRealTimer;

struct _GRealTimer
{
  struct timeval start;
  struct timeval end;
  gint active;
};


GTimer*
g_timer_new (void)
{
  GRealTimer *timer;

  timer = g_new (GRealTimer, 1);
  timer->active = TRUE;

  gettimeofday (&timer->start, NULL);

  return ((GTimer*) timer);
}

void
g_timer_destroy (GTimer *timer)
{
  g_assert (timer != NULL);

  g_free (timer);
}

void
g_timer_start (GTimer *timer)
{
  GRealTimer *rtimer;

  g_assert (timer != NULL);

  rtimer = (GRealTimer*) timer;
  gettimeofday (&rtimer->start, NULL);
  rtimer->active = 1;
}

void
g_timer_stop (GTimer *timer)
{
  GRealTimer *rtimer;

  g_assert (timer != NULL);

  rtimer = (GRealTimer*) timer;
  gettimeofday (&rtimer->end, NULL);
  rtimer->active = 0;
}

void
g_timer_reset (GTimer *timer)
{
  GRealTimer *rtimer;

  g_assert (timer != NULL);

  rtimer = (GRealTimer*) timer;
  gettimeofday (&rtimer->start, NULL);
}

gdouble
g_timer_elapsed (GTimer *timer,
		 gulong *microseconds)
{
  GRealTimer *rtimer;
  struct timeval elapsed;
  gdouble total;

  g_assert (timer != NULL);

  rtimer = (GRealTimer*) timer;

  if (rtimer->active)
    gettimeofday (&rtimer->end, NULL);

  if (rtimer->start.tv_usec > rtimer->end.tv_usec)
    {
      rtimer->end.tv_usec += 1000000;
      rtimer->end.tv_sec--;
    }

  elapsed.tv_usec = rtimer->end.tv_usec - rtimer->start.tv_usec;
  elapsed.tv_sec = rtimer->end.tv_sec - rtimer->start.tv_sec;

  total = elapsed.tv_sec + ((gdouble) elapsed.tv_usec / 1e6);

  if (microseconds)
    *microseconds = elapsed.tv_usec;

  return total;
}
