/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gmutex.c: MT safety related functions
 * Copyright 1998 Sebastian Wilhelmi; University of Karlsruhe
 *                Owen Taylor
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
 * Modified by the GLib Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

/* 
 * MT safe
 */

#include "config.h"
#include "glib.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

typedef struct _GRealThread GRealThread;

struct  _GRealThread
{
  GThread thread;
  GThreadFunc func;
  gpointer arg;
  gpointer system_thread;
  gpointer private_data;
};

typedef struct _GStaticPrivateNode GStaticPrivateNode;

struct _GStaticPrivateNode
{
  gpointer       data;
  GDestroyNotify destroy;
};

static void g_thread_cleanup (gpointer data);
static void g_thread_fail (void);

/* Global variables */

gboolean g_thread_use_default_impl = TRUE;
gboolean g_threads_got_initialized = FALSE;

#if defined(G_OS_WIN32) && defined(__GNUC__)
__declspec(dllexport)
#endif
GThreadFunctions g_thread_functions_for_glib_use = {
  (GMutex*(*)())g_thread_fail,                 /* mutex_new */
  NULL,                                        /* mutex_lock */
  NULL,                                        /* mutex_trylock */
  NULL,                                        /* mutex_unlock */
  NULL,                                        /* mutex_free */
  (GCond*(*)())g_thread_fail,                  /* cond_new */
  NULL,                                        /* cond_signal */
  NULL,                                        /* cond_broadcast */
  NULL,                                        /* cond_wait */
  NULL,                                        /* cond_timed_wait  */
  NULL,                                        /* cond_free */
  (GPrivate*(*)(GDestroyNotify))g_thread_fail, /* private_new */
  NULL,                                        /* private_get */
  NULL,                                        /* private_set */
  (gpointer(*)(GThreadFunc, gpointer, gulong, 
	       gboolean, gboolean, 
	       GThreadPriority))g_thread_fail, /* thread_create */
  NULL,                                        /* thread_yield */
  NULL,                                        /* thread_join */
  NULL,                                        /* thread_exit */
  NULL,                                        /* thread_set_priority */
  NULL                                         /* thread_self */
}; 

/* Local data */

static GMutex   *g_mutex_protect_static_mutex_allocation = NULL;
static GMutex   *g_thread_specific_mutex = NULL;
static GPrivate *g_thread_specific_private = NULL;

/* This must be called only once, before any threads are created.
 * It will only be called from g_thread_init() in -lgthread.
 */
void
g_mutex_init (void)
{
  gpointer private_old;
 
  /* We let the main thread (the one that calls g_thread_init) inherit
   * the data, that it set before calling g_thread_init
   */
  private_old = g_thread_specific_private;

  g_thread_specific_private = g_private_new (g_thread_cleanup);

  /* we can not use g_private_set here, as g_threads_got_initialized is not
   * yet set TRUE, whereas the private_set function is already set.
   */
  g_thread_functions_for_glib_use.private_set (g_thread_specific_private, 
					       private_old);

  g_mutex_protect_static_mutex_allocation = g_mutex_new();
  g_thread_specific_mutex = g_mutex_new();
  
}

GMutex *
g_static_mutex_get_mutex_impl (GMutex** mutex)
{
  if (!g_thread_supported ())
    return NULL;

  g_assert (g_mutex_protect_static_mutex_allocation);

  g_mutex_lock (g_mutex_protect_static_mutex_allocation);

  if (!(*mutex)) 
    *mutex = g_mutex_new(); 

  g_mutex_unlock (g_mutex_protect_static_mutex_allocation);
  
  return *mutex;
}

#ifndef g_static_rec_mutex_lock
/* That means, that g_static_rec_mutex_lock is not defined to be 
 * g_static_mutex_lock, we have to provide an implementation ourselves.
 */
void
g_static_rec_mutex_lock (GStaticRecMutex* mutex)
{
  guint counter = GPOINTER_TO_UINT (g_static_private_get (&mutex->counter));
  if (counter == 0)
    {
      g_static_mutex_lock (&mutex->mutex);
    }
  counter++;
  g_static_private_set (&mutex->counter, GUINT_TO_POINTER (counter), NULL);
}

gboolean
g_static_rec_mutex_trylock (GStaticRecMutex* mutex)
{
  guint counter = GPOINTER_TO_UINT (g_static_private_get (&mutex->counter));
  if (counter == 0)
    {
      if (!g_static_mutex_trylock (&mutex->mutex)) return FALSE;
    }
  counter++;
  g_static_private_set (&mutex->counter, GUINT_TO_POINTER (counter), NULL);
  return TRUE;
}

void
g_static_rec_mutex_unlock (GStaticRecMutex* mutex)
{
  guint counter = GPOINTER_TO_UINT (g_static_private_get (&mutex->counter));
  if (counter == 1)
    {
      g_static_mutex_unlock (&mutex->mutex);
    }
  counter--;
  g_static_private_set (&mutex->counter, GUINT_TO_POINTER (counter), NULL);
}
#endif /* g_static_rec_mutex_lock */

gpointer
g_static_private_get (GStaticPrivate *private_key)
{
  return g_static_private_get_for_thread (private_key, g_thread_self ());
}

gpointer
g_static_private_get_for_thread (GStaticPrivate *private_key,
				 GThread        *thread)
{
  GArray *array;
  GRealThread *self = (GRealThread*) thread;

  g_return_val_if_fail (thread, NULL);

  array = self->private_data;
  if (!array)
    return NULL;

  if (!private_key->index)
    return NULL;
  else if (private_key->index <= array->len)
    return g_array_index (array, GStaticPrivateNode, private_key->index - 1).data;
  else
    return NULL;
}

void
g_static_private_set (GStaticPrivate *private_key, 
		      gpointer        data,
		      GDestroyNotify  notify)
{
  g_static_private_set_for_thread (private_key, g_thread_self (), 
				   data, notify);
}

void
g_static_private_set_for_thread (GStaticPrivate *private_key, 
				 GThread        *thread,
				 gpointer        data,
				 GDestroyNotify  notify)
{
  GArray *array;
  GRealThread *self =(GRealThread*) thread;
  static guint next_index = 0;
  GStaticPrivateNode *node;

  g_return_if_fail (thread);
  
  array = self->private_data;
  if (!array)
    {
      array = g_array_new (FALSE, TRUE, sizeof (GStaticPrivateNode));
      self->private_data = array;
    }

  if (!private_key->index)
    {
      g_mutex_lock (g_thread_specific_mutex);

      if (!private_key->index)
	private_key->index = ++next_index;

      g_mutex_unlock (g_thread_specific_mutex);
    }

  if (private_key->index > array->len)
    g_array_set_size (array, private_key->index);

  node = &g_array_index (array, GStaticPrivateNode, private_key->index - 1);
  if (node->destroy)
    {
      gpointer ddata = node->data;
      GDestroyNotify ddestroy = node->destroy;

      node->data = data;
      node->destroy = notify;

      ddestroy (ddata);
    }
  else
    {
      node->data = data;
      node->destroy = notify;
    }
}

static void
g_thread_cleanup (gpointer data)
{
  if (data)
    {
      GRealThread* thread = data;
      if (thread->private_data)
	{
	  GArray* array = thread->private_data;
	  guint i;
	  
	  for (i = 0; i < array->len; i++ )
	    {
	      GStaticPrivateNode *node = 
		&g_array_index (array, GStaticPrivateNode, i);
	      if (node->destroy)
		node->destroy (node->data);
	    }
	  g_array_free (array, TRUE);
	}
      /* We only free the thread structure, if it isn't joinable. If
         it is, the structure is freed in g_thread_join */
      if (!thread->thread.joinable)
	{
	  /* Just to make sure, this isn't used any more */
	  thread->system_thread = NULL;
	  g_free (thread);
	}
    }
}

static void
g_thread_fail (void)
{
  g_error ("The thread system is not yet initialized.");
}

G_LOCK_DEFINE_STATIC (g_thread_create);

static void 
g_thread_create_proxy (gpointer data)
{
  GRealThread* thread = data;

  g_assert (data);

  /* the lock makes sure, that thread->system_thread is written,
     before thread->func is called. See g_thread_create */

  G_LOCK (g_thread_create);
  g_private_set (g_thread_specific_private, data);
  G_UNLOCK (g_thread_create);

  thread->func (thread->arg);
}

GThread* 
g_thread_create (GThreadFunc 		 thread_func,
		 gpointer 		 arg,
		 gulong 		 stack_size,
		 gboolean 		 joinable,
		 gboolean 		 bound,
		 GThreadPriority 	 priority)
{
  GRealThread* result = g_new0 (GRealThread,1);

  g_return_val_if_fail (thread_func, NULL);
  
  result->thread.joinable = joinable;
  result->thread.bound = bound;
  result->thread.priority = priority;
  result->func = thread_func;
  result->arg = arg;
  G_LOCK (g_thread_create);
  result->system_thread = G_THREAD_UF (thread_create, (g_thread_create_proxy, 
						       result, stack_size, 
						       joinable, bound, 
						       priority));
  G_UNLOCK (g_thread_create);
  return (GThread*) result;
}

void 
g_thread_join (GThread* thread)
{
  GRealThread* real = (GRealThread*) thread;

  g_return_if_fail (thread);
  g_return_if_fail (thread->joinable);
  g_return_if_fail (real->system_thread);

  G_THREAD_UF (thread_join, (real->system_thread));

  /* Just to make sure, this isn't used any more */
  thread->joinable = 0;
  real->system_thread = NULL;

  /* the thread structure for non-joinable threads is freed upon
     thread end. We free the memory here. This will leave loose end,
     if a joinable thread is not joined. */

  g_free (thread);
}

void
g_thread_set_priority (GThread* thread, 
		       GThreadPriority priority)
{
  GRealThread* real = (GRealThread*) thread;

  g_return_if_fail (thread);
  g_return_if_fail (real->system_thread);

  thread->priority = priority;
  G_THREAD_CF (thread_set_priority, (void)0, (real->system_thread, priority));
}

GThread*
g_thread_self()
{
  GRealThread* thread = g_private_get (g_thread_specific_private);

  if (!thread)
    {  
      /* If no thread data is available, provide and set one.  This
         can happen for the main thread and for threads, that are not
         created by glib. */
      thread = g_new (GRealThread,1);
      thread->thread.joinable = FALSE; /* This is a save guess */
      thread->thread.bound = TRUE; /* This isn't important at all */
      thread->thread.priority = G_THREAD_PRIORITY_NORMAL; /* This is
							     just a guess */
      thread->func = NULL;
      thread->arg = NULL;
      thread->system_thread = NULL;
      thread->private_data = NULL;
      g_private_set (g_thread_specific_private, thread);
    }
     
  if (g_thread_supported () && !thread->system_thread)
    {
      thread->system_thread = g_thread_functions_for_glib_use.thread_self();
    }

  return (GThread*)thread;
}

static void inline g_static_rw_lock_wait (GCond** cond, GStaticMutex* mutex)
{
  if (!*cond)
      *cond = g_cond_new ();
  g_cond_wait (*cond, g_static_mutex_get_mutex (mutex));
}

static void inline g_static_rw_lock_signal (GStaticRWLock* lock)
{
  if (lock->want_to_write && lock->write_cond)
    g_cond_signal (lock->write_cond);
  else if (lock->read_cond)
    g_cond_signal (lock->read_cond);
}

void g_static_rw_lock_reader_lock (GStaticRWLock* lock)
{
  g_return_if_fail (lock);

  if (!g_threads_got_initialized)
    return;

  g_static_mutex_lock (&lock->mutex);
  while (lock->write || lock->want_to_write) 
    g_static_rw_lock_wait (&lock->read_cond, &lock->mutex);
  lock->read_counter++;
  g_static_mutex_unlock (&lock->mutex);
}

gboolean g_static_rw_lock_reader_trylock (GStaticRWLock* lock)
{
  gboolean ret_val = FALSE;

  g_return_val_if_fail (lock, FALSE);

  if (!g_threads_got_initialized)
    return TRUE;

  g_static_mutex_lock (&lock->mutex);
  if (!lock->write && !lock->want_to_write)
    {
      lock->read_counter++;
      ret_val = TRUE;
    }
  g_static_mutex_unlock (&lock->mutex);
  return ret_val;
}

void g_static_rw_lock_reader_unlock  (GStaticRWLock* lock)
{
  g_return_if_fail (lock);

  if (!g_threads_got_initialized)
    return;

  g_static_mutex_lock (&lock->mutex);
  lock->read_counter--;
  g_static_rw_lock_signal (lock);
  g_static_mutex_unlock (&lock->mutex);
}

void g_static_rw_lock_writer_lock (GStaticRWLock* lock)
{
  g_return_if_fail (lock);

  if (!g_threads_got_initialized)
    return;

  g_static_mutex_lock (&lock->mutex);
  lock->want_to_write++;
  while (lock->write || lock->read_counter)
    g_static_rw_lock_wait (&lock->write_cond, &lock->mutex);
  lock->want_to_write--;
  lock->write = TRUE;
  g_static_mutex_unlock (&lock->mutex);
}

gboolean g_static_rw_lock_writer_trylock (GStaticRWLock* lock)
{
  gboolean ret_val = FALSE;

  g_return_val_if_fail (lock, FALSE);
  
  if (!g_threads_got_initialized)
    return TRUE;

  g_static_mutex_lock (&lock->mutex);
  if (!lock->write && !lock->read_counter)
    {
      lock->write = TRUE;
      ret_val = TRUE;
    }
  g_static_mutex_unlock (&lock->mutex);
  return ret_val;
}

void g_static_rw_lock_writer_unlock (GStaticRWLock* lock)
{
  g_return_if_fail (lock);
  
  if (!g_threads_got_initialized)
    return;

  g_static_mutex_lock (&lock->mutex);
  lock->write = FALSE; 
  g_static_rw_lock_signal (lock);
  g_static_mutex_unlock (&lock->mutex);
}

void g_static_rw_lock_free (GStaticRWLock* lock)
{
  g_return_if_fail (lock);
  
  if (lock->read_cond)
    g_cond_free (lock->read_cond);
  if (lock->write_cond)
    g_cond_free (lock->write_cond);
  
}

