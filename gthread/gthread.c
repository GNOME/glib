/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gthread.c: thread related functions
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

static const char *g_log_domain_gthread = "GThread";
static gboolean thread_system_already_initialized = FALSE;

#include G_THREAD_SOURCE

void g_mutex_init (void);
void g_mem_init (void);
void g_messages_init (void);

void
g_thread_init(GThreadFunctions* init)
{
  gboolean supported;

  if (thread_system_already_initialized)
    g_error ("the glib thread system may only be initialized once.");
    
  thread_system_already_initialized = TRUE;

  if (init == NULL)
    init = &g_thread_functions_for_glib_use_default;
  else
    g_thread_use_default_impl = FALSE;

  g_thread_functions_for_glib_use = *init;

  /* It is important, that g_thread_supported is not set before the
     thread initialization functions of the different modules are
     called */

  supported = 
    init->mutex_new &&  
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
    init->private_get;

  /* if somebody is calling g_thread_init (), it means that he wants to
     have thread support, so check this */

  if (!supported)
    {
      if (g_thread_use_default_impl)
	g_error ("Threads are not supported on this platform.");
      else
	g_error ("The supplied thread function vector is invalid.");
    }

  /* now call the thread initialization functions of the different
     glib modules. BTW: order does matter, g_mutex_init MUST be first */

  g_mutex_init ();
  g_mem_init ();
  g_messages_init ();

  /* now we can set g_thread_supported and thus enable all the thread
     functions */

  g_thread_supported = TRUE;
}
