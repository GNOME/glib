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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

static gboolean thread_system_already_initialized = FALSE;
static gint g_thread_priority_map [G_THREAD_PRIORITY_URGENT];

#include G_THREAD_SOURCE

#ifndef PRIORITY_LOW_VALUE
# define PRIORITY_LOW_VALUE 0
#endif

#ifndef PRIORITY_URGENT_VALUE
# define PRIORITY_URGENT_VALUE 0
#endif

#ifndef PRIORITY_NORMAL_VALUE
# define PRIORITY_NORMAL_VALUE 						\
  PRIORITY_LOW_VALUE + (PRIORITY_URGENT_VALUE - PRIORITY_LOW_VALUE) * 40 / 100 
#endif /* PRIORITY_NORMAL_VALUE */

#ifndef PRIORITY_HIGH_VALUE
# define PRIORITY_HIGH_VALUE 						\
  PRIORITY_LOW_VALUE + (PRIORITY_URGENT_VALUE - PRIORITY_LOW_VALUE) * 80 / 100 
#endif /* PRIORITY_HIGH_VALUE */

void g_mutex_init (void);
void g_mem_init (void);
void g_messages_init (void);

#define G_MUTEX_DEBUG_INFO(mutex) (*((gpointer*)(((char*)mutex)+G_MUTEX_SIZE)))

typedef struct _ErrorCheckInfo ErrorCheckInfo;
struct _ErrorCheckInfo
{
  gchar *name;
  gchar *location;
  GThread *owner;
};

static GMutex *
g_mutex_new_errorcheck_impl (void)
{
  GMutex *retval = g_thread_functions_for_glib_use_default.mutex_new ();
  retval = g_realloc (retval, G_MUTEX_SIZE + sizeof (gpointer));
  G_MUTEX_DEBUG_INFO (retval) = NULL;
  return retval;
}

static inline void
fill_info (ErrorCheckInfo *info,
	   GThread *self,
	   gulong magic, 
	   gchar *name, 
	   gchar *location)
{
  info->owner = self;
  if (magic == G_MUTEX_DEBUG_MAGIC)
    {
      /* We are used with special instrumented calls, where name and
       * location is provided as well, so use them */
      info->name = name;
      info->location = location;
    }
  else
    info->name = info->location = "unknown";
}

static void
g_mutex_lock_errorcheck_impl (GMutex *mutex, 
			      gulong magic, 
			      gchar *name, 
			      gchar *location)
{
  ErrorCheckInfo *info;
  GThread *self = g_thread_self ();
  if (G_MUTEX_DEBUG_INFO (mutex) == NULL)
    {
      /* if the debug info is NULL, we have not yet locked that mutex,
       * so we do it now */
      g_thread_functions_for_glib_use_default.mutex_lock (mutex);
      /* Now we have to check again, because anothe thread might mave
       * locked the mutex at the same time, we did. This technique is
       * not 100% save on systems without decent cache coherence,
       * but we have no choice */ 
      if (G_MUTEX_DEBUG_INFO (mutex) == NULL)
	{
	  info = G_MUTEX_DEBUG_INFO (mutex) = g_new0 (ErrorCheckInfo, 1);
	}
      g_thread_functions_for_glib_use_default.mutex_unlock (mutex);
    }
  
  info = G_MUTEX_DEBUG_INFO (mutex);
  if (info->owner == self)
    g_error ("Trying to recursivly lock the mutex '%s' at '%s', "
	     "previously locked by name '%s' at '%s'", 
	     name, location, info->name, info->location);

  g_thread_functions_for_glib_use_default.mutex_lock (mutex);

  fill_info (info, self, magic, name, location);
}

static gboolean
g_mutex_trylock_errorcheck_impl (GMutex *mutex, 
				 gulong magic, 
				 gchar *name, 
				 gchar *location)
{
  ErrorCheckInfo *info = G_MUTEX_DEBUG_INFO (mutex);
  GThread *self = g_thread_self ();
  if (!info)
    {
      /* This mutex has not yet been used, so simply lock and return TRUE */
      g_mutex_lock_errorcheck_impl (mutex, magic, name, location);
      return TRUE;
    }
  if (info->owner == self)
    g_error ("Trying to recursivly lock the mutex '%s' at '%s', "
	     "previously locked by name '%s' at '%s'", 
	     name, location, info->name, info->location);
  
  if (!g_thread_functions_for_glib_use_default.mutex_trylock (mutex))
    return FALSE;

  fill_info (info, self, magic, name, location);
  return TRUE;
}

static void
g_mutex_unlock_errorcheck_impl (GMutex *mutex, 
				gulong magic, 
				gchar *name, 
				gchar *location)
{
  ErrorCheckInfo *info = G_MUTEX_DEBUG_INFO (mutex);
  GThread *self = g_thread_self ();
  if (magic != G_MUTEX_DEBUG_MAGIC)
    name = location = "unknown";
  if (!info || info->owner != self)
    g_error ("Trying to unlock the unlocked mutex '%s' at '%s'",
	     name, location);
  info->owner = NULL;
  info->name = info->location = NULL;
  g_thread_functions_for_glib_use_default.mutex_unlock (mutex);
}

static void
g_mutex_free_errorcheck_impl (GMutex *mutex)
{
  g_free (G_MUTEX_DEBUG_INFO (mutex));
  g_thread_functions_for_glib_use_default.mutex_free (mutex);  
}

/* unshadow function declaration. See glib.h */
#undef g_thread_init

void 
g_thread_init_with_errorcheck_mutexes (GThreadFunctions* init)
{
  GThreadFunctions errorcheck_functions;
  if (init)
    g_error ("Errorcheck mutexes can only be used for native " 
	     "thread implementations. Sorry." );
  errorcheck_functions = g_thread_functions_for_glib_use_default;
  errorcheck_functions.mutex_new = g_mutex_new_errorcheck_impl;
  errorcheck_functions.mutex_lock = 
    (void (*)(GMutex *)) g_mutex_lock_errorcheck_impl;
  errorcheck_functions.mutex_trylock = 
    (gboolean (*)(GMutex *)) g_mutex_trylock_errorcheck_impl;
  errorcheck_functions.mutex_unlock = 
    (void (*)(GMutex *)) g_mutex_unlock_errorcheck_impl;
  errorcheck_functions.mutex_free = g_mutex_free_errorcheck_impl;
    
  g_thread_init (&errorcheck_functions);
}

void
g_thread_init (GThreadFunctions* init)
{
  gboolean supported;

#ifndef	G_THREADS_ENABLED
  g_error ("GLib thread support is disabled.");
#endif	/* !G_THREADS_ENABLED */

  if (thread_system_already_initialized)
    g_error ("GThread system may only be initialized once.");
    
  thread_system_already_initialized = TRUE;

  if (init == NULL)
    init = &g_thread_functions_for_glib_use_default;
  else
    g_thread_use_default_impl = FALSE;

#if defined (G_OS_WIN32) && defined (__GNUC__)
  memcpy(&g_thread_functions_for_glib_use, init, sizeof (*init));
#else
  g_thread_functions_for_glib_use = *init;
#endif
  /* It is important, that g_threads_got_initialized is not set before the
   * thread initialization functions of the different modules are called
   */

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
	       init->private_get);

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

  /* now do any initialization stuff required by the implementation,
   * but only if called with a NULL argument, of course. Otherwise
   * it's up to the user to do so. */

#ifdef HAVE_G_THREAD_IMPL_INIT
  if (g_thread_use_default_impl)
    g_thread_impl_init();
#endif

  g_thread_priority_map [G_THREAD_PRIORITY_LOW] = PRIORITY_LOW_VALUE;
  g_thread_priority_map [G_THREAD_PRIORITY_NORMAL] = PRIORITY_NORMAL_VALUE;
  g_thread_priority_map [G_THREAD_PRIORITY_HIGH] = PRIORITY_HIGH_VALUE;
  g_thread_priority_map [G_THREAD_PRIORITY_URGENT] = PRIORITY_URGENT_VALUE;

  /* now call the thread initialization functions of the different
   * glib modules. order does matter, g_mutex_init MUST come first.
   */
  g_mutex_init ();
  g_mem_init ();
  g_messages_init ();

  /* now we can set g_threads_got_initialized and thus enable
   * all the thread functions
   */
  g_threads_got_initialized = TRUE;

  /* we want the main thread to run with normal priority */
  g_thread_set_priority (g_thread_self(), G_THREAD_PRIORITY_NORMAL);
}
