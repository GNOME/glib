/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gthread.c: thread related functions
 * Copyright 1998 Sebastian Wilhelmi; University of Karlsruhe
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#include "glib.h"
#include "gthreadprivate.h"

static gboolean thread_system_already_initialized = FALSE;
static gint g_thread_priority_map [G_THREAD_PRIORITY_URGENT + 1];

#include G_THREAD_SOURCE

#ifndef PRIORITY_LOW_VALUE
# define PRIORITY_LOW_VALUE 0
#endif

#ifndef PRIORITY_URGENT_VALUE
# define PRIORITY_URGENT_VALUE 0
#endif

#ifndef PRIORITY_NORMAL_VALUE
# define PRIORITY_NORMAL_VALUE						\
  ((PRIORITY_LOW_VALUE * 6 + PRIORITY_URGENT_VALUE * 4) / 10)
#endif /* PRIORITY_NORMAL_VALUE */

#ifndef PRIORITY_HIGH_VALUE
# define PRIORITY_HIGH_VALUE						\
  ((PRIORITY_NORMAL_VALUE + PRIORITY_URGENT_VALUE * 2) / 3)
#endif /* PRIORITY_HIGH_VALUE */

void
g_thread_init (GThreadFunctions* init)
{
  gboolean supported;

  if (thread_system_already_initialized)
    {
      if (init != NULL)
	g_warning ("GThread system already initialized, ignoring custom thread implementation.");

      return;
    }

  thread_system_already_initialized = TRUE;

  if (init == NULL)
    {
#ifdef HAVE_G_THREAD_IMPL_INIT
      /* now do any initialization stuff required by the
       * implementation, but only if called with a NULL argument, of
       * course. Otherwise it's up to the user to do so. */
      g_thread_impl_init();
#endif /* HAVE_G_THREAD_IMPL_INIT */
      init = &g_thread_functions_for_glib_use_default;
    }
  else
    g_thread_use_default_impl = FALSE;

  g_thread_functions_for_glib_use = *init;

  supported = (init->mutex_new &&
	       init->mutex_lock &&
	       init->mutex_trylock &&
	       init->mutex_unlock &&
	       init->mutex_free &&
	       init->cond_new &&
	       init->cond_signal &&
	       init->cond_broadcast &&
	       init->cond_wait &&
	       init->cond_timed_wait &&
	       init->cond_free &&
	       init->private_new &&
	       init->private_get &&
	       init->private_set &&
	       init->thread_create &&
	       init->thread_yield &&
	       init->thread_join &&
	       init->thread_exit &&
	       init->thread_set_priority &&
	       init->thread_self);

  /* if somebody is calling g_thread_init (), it means that he wants to
   * have thread support, so check this
   */
  if (!supported)
    {
      if (g_thread_use_default_impl)
	g_error ("Threads are not supported on this platform.");
      else
	g_error ("The supplied thread function vector is invalid.");
    }

  g_thread_priority_map [G_THREAD_PRIORITY_LOW] = PRIORITY_LOW_VALUE;
  g_thread_priority_map [G_THREAD_PRIORITY_NORMAL] = PRIORITY_NORMAL_VALUE;
  g_thread_priority_map [G_THREAD_PRIORITY_HIGH] = PRIORITY_HIGH_VALUE;
  g_thread_priority_map [G_THREAD_PRIORITY_URGENT] = PRIORITY_URGENT_VALUE;

  g_thread_init_glib ();
}

void
g_thread_init_with_errorcheck_mutexes (GThreadFunctions *vtable)
{
  g_assert (vtable == NULL);

  g_thread_init (NULL);
}
