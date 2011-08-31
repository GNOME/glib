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

#include G_THREAD_SOURCE

void
g_thread_init (GThreadFunctions *init)
{
  static gboolean already_done;

  if (init != NULL)
    g_warning ("GThread system no longer supports custom thread implementations.");

  if (already_done)
    return;

  already_done = TRUE;

#ifdef HAVE_G_THREAD_IMPL_INIT
  g_thread_impl_init();
#endif /* HAVE_G_THREAD_IMPL_INIT */

  g_thread_functions_for_glib_use = g_thread_functions_for_glib_use_default;
  g_thread_init_glib ();
}

void
g_thread_init_with_errorcheck_mutexes (GThreadFunctions *vtable)
{
  g_assert (vtable == NULL);

  g_thread_init (NULL);
}
