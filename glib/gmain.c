/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gmain.c: Main loop abstraction, timeouts, and idle functions
 * Copyright 1998 Owen Taylor
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

#include "glib.h"
#include <sys/types.h>
#include <sys/time.h>
#ifdef	GLIB_HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif	/* GLIB_HAVE_SYS_POLL_H */
#include <unistd.h>
#include <errno.h>
#include "config.h"

/* Types */

typedef struct _GIdleData GIdleData;
typedef struct _GTimeoutData GTimeoutData;
typedef struct _GSource GSource;
typedef struct _GPollRec GPollRec;

typedef enum
{
  G_SOURCE_READY = 1 << G_HOOK_FLAG_USER_SHIFT,
  G_SOURCE_CAN_RECURSE = 1 << (G_HOOK_FLAG_USER_SHIFT + 1)
} GSourceFlags;

struct _GSource
{
  GHook hook;
  gint priority;
  gpointer source_data;
};

struct _GMainLoop
{
  gboolean is_running;
};

struct _GIdleData
{
  GSourceFunc callback;
};

struct _GTimeoutData
{
  GTimeVal    expiration;
  gint        interval;
  GSourceFunc callback;
};

struct _GPollRec
{
  gint priority;
  GPollFD *fd;
  GPollRec *next;
};

/* Forward declarations */

static void     g_main_poll               (gint      timeout,
					   gboolean  use_priority, 
					   gint      priority);
static void     g_main_add_poll_unlocked  (gint      priority,
					   GPollFD  *fd);

static gboolean g_timeout_prepare      (gpointer  source_data, 
					GTimeVal *current_time,
					gint     *timeout);
static gboolean g_timeout_check        (gpointer  source_data,
					GTimeVal *current_time);
static gboolean g_timeout_dispatch     (gpointer  source_data,
					GTimeVal *current_time,
					gpointer  user_data);
static gboolean g_idle_prepare         (gpointer  source_data, 
					GTimeVal *current_time,
					gint     *timeout);
static gboolean g_idle_check           (gpointer  source_data,
					GTimeVal *current_time);
static gboolean g_idle_dispatch        (gpointer  source_data,
					GTimeVal *current_time,
					gpointer  user_data);

/* Data */

static GSList *pending_dispatches = NULL;
static GHookList source_list = { 0 };

/* The following lock is used for both the list of sources
 * and the list of poll records
 */
G_LOCK_DECLARE_STATIC (main_loop);

static GSourceFuncs timeout_funcs = {
  g_timeout_prepare,
  g_timeout_check,
  g_timeout_dispatch,
  (GDestroyNotify)g_free
};

static GSourceFuncs idle_funcs = {
  g_idle_prepare,
  g_idle_check,
  g_idle_dispatch,
  (GDestroyNotify)g_free
};

static GPollRec *poll_records = NULL;
static GPollRec *poll_free_list = NULL;
static GMemChunk *poll_chunk;
static guint n_poll_records = 0;

/* this pipe is used to wake up the main loop when a source is added.
 */
static gint wake_up_pipe[2] = { -1, -1 };
static GPollFD wake_up_rec;
static gboolean poll_waiting = FALSE;

#ifdef HAVE_POLL
static GPollFunc poll_func = (GPollFunc) poll;
#else	/* !HAVE_POLL */

/* The following implementation of poll() comes from the GNU C Library.
 * Copyright (C) 1994, 1996, 1997 Free Software Foundation, Inc.
 */

#include <string.h> /* for bzero on BSD systems */

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif /* HAVE_SYS_SELECT_H_ */

#ifndef NO_FD_SET
#  define SELECT_MASK fd_set
#else	/* !NO_FD_SET */
#  ifndef _AIX
typedef long fd_mask;
#  endif
#  if defined(_IBMR2)
#    define SELECT_MASK void
#  else
#    define SELECT_MASK int
#  endif
#endif	/* !NO_FD_SET */

static gint 
g_poll (GPollFD *fds, guint nfds, gint timeout)
{
  struct timeval tv;
  SELECT_MASK rset, wset, xset;
  GPollFD *f;
  int ready;
  int maxfd = 0;

  FD_ZERO (&rset);
  FD_ZERO (&wset);
  FD_ZERO (&xset);

  for (f = fds; f < &fds[nfds]; ++f)
    if (f->fd >= 0)
      {
	if (f->events & G_IO_IN)
	  FD_SET (f->fd, &rset);
	if (f->events & G_IO_OUT)
	  FD_SET (f->fd, &wset);
	if (f->events & G_IO_PRI)
	  FD_SET (f->fd, &xset);
	if (f->fd > maxfd && (f->events & (G_IO_IN|G_IO_OUT|G_IO_PRI)))
	  maxfd = f->fd;
      }

  tv.tv_sec = timeout / 1000;
  tv.tv_usec = (timeout % 1000) * 1000;

  ready = select (maxfd + 1, &rset, &wset, &xset,
		  timeout == -1 ? NULL : &tv);
  if (ready > 0)
    for (f = fds; f < &fds[nfds]; ++f)
      {
	f->revents = 0;
	if (f->fd >= 0)
	  {
	    if (FD_ISSET (f->fd, &rset))
	      f->revents |= G_IO_IN;
	    if (FD_ISSET (f->fd, &wset))
	      f->revents |= G_IO_OUT;
	    if (FD_ISSET (f->fd, &xset))
	      f->revents |= G_IO_PRI;
	  }
      }

  return ready;
}
static GPollFunc poll_func = g_poll;
#endif	/* !HAVE_POLL */

/* Hooks for adding to the main loop */

/* Use knowledge of insert_sorted algorithm here to make
 * sure we insert at the end of equal priority items
 */
static gint
g_source_compare (GHook *a,
		  GHook *b)
{
  GSource *source_a = (GSource *)a;
  GSource *source_b = (GSource *)b;

  return (source_a->priority < source_b->priority) ? -1 : 1;
}

guint 
g_source_add (gint           priority,
	      gboolean       can_recurse,
	      GSourceFuncs  *funcs,
	      gpointer       source_data, 
	      gpointer       user_data,
	      GDestroyNotify notify)
{
  guint return_val;
  GSource *source;

  G_LOCK (main_loop);

  if (!source_list.is_setup)
    g_hook_list_init (&source_list, sizeof(GSource));

  source = (GSource *)g_hook_alloc (&source_list);
  source->priority = priority;
  source->source_data = source_data;
  source->hook.func = funcs;
  source->hook.data = user_data;
  source->hook.destroy = notify;
  
  g_hook_insert_sorted (&source_list, 
			(GHook *)source, 
			g_source_compare);

  if (can_recurse)
    source->hook.flags |= G_SOURCE_CAN_RECURSE;

  return_val = source->hook.hook_id;

  /* Now wake up the main loop if it is waiting in the poll() */

  if (poll_waiting)
    {
      poll_waiting = FALSE;
      write (wake_up_pipe[1], "A", 1);
    }

  G_UNLOCK (main_loop);

  return return_val;
}

void 
g_source_remove (guint tag)
{
  GHook *hook;

  G_LOCK (main_loop);

  hook = g_hook_get (&source_list, tag);
  if (hook)
    {
      GSource *source = (GSource *)hook;

      ((GSourceFuncs *) source->hook.func)->destroy (source->source_data);
      g_hook_destroy_link (&source_list, hook);
    }

  G_UNLOCK (main_loop);
}

void 
g_source_remove_by_user_data (gpointer user_data)
{
  GHook *hook;
  
  G_LOCK (main_loop);
  
  hook = g_hook_find_data (&source_list, TRUE, user_data);
  if (hook)
    {
      GSource *source = (GSource *)hook;

      ((GSourceFuncs *) source->hook.func)->destroy (source->source_data);
      g_hook_destroy_link (&source_list, hook);
    }

  G_UNLOCK (main_loop);
}

static gboolean
g_source_find_source_data (GHook	*hook,
			   gpointer	 data)
{
  GSource *source = (GSource *)hook;

  return (source->source_data == data);
}

void 
g_source_remove_by_source_data (gpointer source_data)
{
  GHook *hook;

  G_LOCK (main_loop);

  hook = g_hook_find (&source_list, TRUE, 
		      g_source_find_source_data, source_data);
  if (hook)
    {
      GSource *source = (GSource *)hook;

      ((GSourceFuncs *) source->hook.func)->destroy (source->source_data);
      g_hook_destroy_link (&source_list, hook);
    }

  G_UNLOCK (main_loop);
}

void
g_get_current_time (GTimeVal *result)
{
  g_return_if_fail (result != NULL);

  gettimeofday ((struct timeval *) result, NULL);
}

/* Running the main loop */

/* HOLDS: main_loop_lock */
static void
g_main_dispatch (GTimeVal *current_time)
{
  while (pending_dispatches != NULL)
    {
      gboolean need_destroy;
      GSource *source = pending_dispatches->data;
      GSList *tmp_list;

      tmp_list = pending_dispatches;
      pending_dispatches = g_slist_remove_link (pending_dispatches, pending_dispatches);
      g_slist_free_1 (tmp_list);

      if (G_HOOK_IS_VALID (source))
	{
	  gboolean was_in_call;
	  gpointer hook_data = source->hook.data;
	  gpointer source_data = source->source_data;
	  gboolean (*dispatch) (gpointer,
				GTimeVal *,
				gpointer);

	  dispatch = ((GSourceFuncs *) source->hook.func)->dispatch;
	  
	  was_in_call = G_HOOK_IN_CALL (source);
	  source->hook.flags |= G_HOOK_FLAG_IN_CALL;

	  G_UNLOCK (main_loop);
	  need_destroy = ! dispatch (source_data,
				     current_time,
				     hook_data);
	  G_LOCK (main_loop);

	  if (!was_in_call)
	    source->hook.flags &= ~G_HOOK_FLAG_IN_CALL;
	  
	  if (need_destroy && G_HOOK_IS_VALID (source))
	    {
	      ((GSourceFuncs *) source->hook.func)->destroy (source->source_data);
	      g_hook_destroy_link (&source_list, (GHook *) source);
	    }
	}

      g_hook_unref (&source_list, (GHook *)source);
    }
}

/* g_main_iterate () runs a single iteration of the mainloop, or,
 * if !dispatch checks to see if any sources need dispatching.
 * basic algorithm for dispatch=TRUE:
 *
 * 1) while the list of currently pending sources is non-empty,
 *    we call (*dispatch) on those that are !IN_CALL or can_recurse,
 *    removing sources from the list after each returns.
 *    the return value of (*dispatch) determines whether the source
 *    itself is kept alive.
 *
 * 2) call (*prepare) for sources that are not yet SOURCE_READY and
 *    are !IN_CALL or can_recurse. a return value of TRUE determines
 *    that the source would like to be dispatched immediatedly, it
 *    is then flagged as SOURCE_READY.
 *
 * 3) poll with the pollfds from all sources at the priority of the
 *    first source flagged as SOURCE_READY. if there are any sources
 *    flagged as SOURCE_READY, we use a timeout of 0 or the minimum
 *    of all timouts otherwise.
 *
 * 4) for each source !IN_CALL or can_recurse, if SOURCE_READY or
 *    (*check) returns true, add the source to the pending list.
 *    once one source returns true, stop after checking all sources
 *    at that priority.
 *
 * 5) while the list of currently pending sources is non-empty,
 *    call (*dispatch) on each source, removing the source
 *    after the call.
 *
 */
static gboolean
g_main_iterate (gboolean block,
		gboolean dispatch)
{
  GHook *hook;
  GTimeVal current_time  ={ 0, 0 };
  gint n_ready = 0;
  gint current_priority = 0;
  gint timeout;
  gboolean retval = FALSE;

  g_return_val_if_fail (!block || dispatch, FALSE);

  g_get_current_time (&current_time);

  G_LOCK (main_loop);
  
  /* If recursing, finish up current dispatch, before starting over */
  if (pending_dispatches)
    {
      if (dispatch)
	g_main_dispatch (&current_time);
      
      G_UNLOCK (main_loop);

      return TRUE;
    }

  /* Prepare all sources */

  timeout = block ? -1 : 0;
  
  hook = g_hook_first_valid (&source_list, TRUE);
  while (hook)
    {
      GSource *source = (GSource *)hook;
      GHook *tmp;
      gint source_timeout = -1;

      if ((n_ready > 0) && (source->priority > current_priority))
	break;
      if (G_HOOK_IN_CALL (hook) && !(hook->flags & G_SOURCE_CAN_RECURSE))
	{
	  hook = g_hook_next_valid (hook, TRUE);
	  continue;
	}

      g_hook_ref (&source_list, hook);
      
      if (hook->flags & G_SOURCE_READY ||
	  ((GSourceFuncs *) hook->func)->prepare (source->source_data,
						  &current_time,
						  &source_timeout))
	{
	  if (!dispatch)
	    {
	      hook->flags |= G_SOURCE_READY;
	      g_hook_unref (&source_list, hook);
	      G_UNLOCK (main_loop);

	      return TRUE;
	    }
	  else
	    {
	      hook->flags |= G_SOURCE_READY;
	      n_ready++;
	      current_priority = source->priority;
	      timeout = 0;
	    }
	}
      
      if (source_timeout >= 0)
	{
	  if (timeout < 0)
	    timeout = source_timeout;
	  else
	    timeout = MIN (timeout, source_timeout);
	}

      tmp = g_hook_next_valid (hook, TRUE);
      
      g_hook_unref (&source_list, hook);
      hook = tmp;
    }

  /* poll(), if necessary */

  g_main_poll (timeout, n_ready > 0, current_priority);

  /* Check to see what sources need to be dispatched */

  n_ready = 0;
  
  hook = g_hook_first_valid (&source_list, TRUE);
  while (hook)
    {
      GSource *source = (GSource *)hook;
      GHook *tmp;

      if ((n_ready > 0) && (source->priority > current_priority))
	break;
      if (G_HOOK_IN_CALL (hook) && !(hook->flags & G_SOURCE_CAN_RECURSE))
	{
	  hook = g_hook_next_valid (hook, TRUE);
	  continue;
	}

      g_hook_ref (&source_list, hook);

      if (hook->flags & G_SOURCE_READY ||
	  ((GSourceFuncs *) hook->func)->check (source->source_data,
						&current_time))
	{
	  if (dispatch)
	    {
	      hook->flags &= ~G_SOURCE_READY;
	      g_hook_ref (&source_list, hook);
	      pending_dispatches = g_slist_prepend (pending_dispatches, source);
	      current_priority = source->priority;
	      n_ready++;
	    }
	  else
	    {
	      g_hook_unref (&source_list, hook);
	      G_UNLOCK (main_loop);

	      return TRUE;
	    }
	}
      
      tmp = g_hook_next_valid (hook, TRUE);
      
      g_hook_unref (&source_list, hook);
      hook = tmp;
    }

  /* Now invoke the callbacks */

  if (pending_dispatches)
    {
      pending_dispatches = g_slist_reverse (pending_dispatches);
      g_main_dispatch (&current_time);
      retval = TRUE;
    }

  G_UNLOCK (main_loop);

  return retval;
}

/* See if any events are pending
 */
gboolean 
g_main_pending ()
{
  return g_main_iterate (FALSE, FALSE);
}

/* Run a single iteration of the mainloop. If block is FALSE,
 * will never block
 */
gboolean
g_main_iteration (gboolean block)
{
  return g_main_iterate (block, TRUE);
}

GMainLoop*
g_main_new (gboolean is_running)
{
  GMainLoop *loop;

  loop = g_new0 (GMainLoop, 1);
  loop->is_running = is_running != FALSE;

  return loop;
}

void 
g_main_run (GMainLoop *loop)
{
  g_return_if_fail (loop != NULL);

  loop->is_running = TRUE;
  while (loop->is_running)
    g_main_iterate (TRUE, TRUE);
}

void 
g_main_quit (GMainLoop *loop)
{
  g_return_if_fail (loop != NULL);

  loop->is_running = FALSE;
}

void 
g_main_destroy (GMainLoop *loop)
{
  g_return_if_fail (loop != NULL);

  g_free (loop);
}

gboolean
g_main_is_running (GMainLoop *loop)
{
  g_return_val_if_fail (loop != NULL, FALSE);

  return loop->is_running;
}

/* HOLDS: main_loop_lock */
static void
g_main_poll (gint     timeout,
	     gboolean use_priority,
	     gint     priority)
{
  GPollFD *fd_array;
  GPollRec *pollrec;

  gint i;
  gint npoll;

  if (wake_up_pipe[0] < 0)
    {
      if (pipe (wake_up_pipe) < 0)
	g_error ("Cannot create pipe main loop wake-up: %s\n",
		 g_strerror (errno));

      wake_up_rec.fd = wake_up_pipe[0];
      wake_up_rec.events = G_IO_IN;
      g_main_add_poll_unlocked (0, &wake_up_rec);
    }
  
  fd_array = g_new (GPollFD, n_poll_records);
 
  pollrec = poll_records;
  i = 0;
  while (pollrec && (!use_priority || priority >= pollrec->priority))
    {
      fd_array[i].fd = pollrec->fd->fd;
      fd_array[i].events = pollrec->fd->events;
      fd_array[i].revents = 0;
	
      pollrec = pollrec->next;
      i++;
    }

  poll_waiting = TRUE;
  
  G_UNLOCK (main_loop);
  npoll = i;
  (*poll_func) (fd_array, npoll, timeout);
  G_LOCK (main_loop);

  if (!poll_waiting)
    {
      gchar c;
      read (wake_up_pipe[0], &c, 1);
    }
  else
    poll_waiting = FALSE;

  pollrec = poll_records;
  i = 0;
  while (i < npoll)
    {
      pollrec->fd->revents = fd_array[i].revents;
      pollrec = pollrec->next;
      i++;
    }

  g_free (fd_array);
}

void 
g_main_add_poll (GPollFD *fd,
		 gint     priority)
{
  G_LOCK (main_loop);
  g_main_add_poll_unlocked (priority, fd);
  G_UNLOCK (main_loop);
}

/* HOLDS: main_loop_lock */
static void 
g_main_add_poll_unlocked (gint     priority,
			  GPollFD *fd)
{
  GPollRec *lastrec, *pollrec, *newrec;

  if (!poll_chunk)
    poll_chunk = g_mem_chunk_create (GPollRec, 32, G_ALLOC_ONLY);

  if (poll_free_list)
    {
      newrec = poll_free_list;
      poll_free_list = newrec->next;
    }
  else
    newrec = g_chunk_new (GPollRec, poll_chunk);

  newrec->fd = fd;
  newrec->priority = priority;

  lastrec = NULL;
  pollrec = poll_records;
  while (pollrec && priority >= pollrec->priority)
    {
      lastrec = pollrec;
      pollrec = pollrec->next;
    }
  
  if (lastrec)
    lastrec->next = newrec;
  else
    poll_records = newrec;

  newrec->next = pollrec;

  n_poll_records++;
}

void 
g_main_remove_poll (GPollFD *fd)
{
  GPollRec *pollrec, *lastrec;

  G_LOCK (main_loop);
  
  lastrec = NULL;
  pollrec = poll_records;

  while (pollrec)
    {
      if (pollrec->fd == fd)
	{
	  if (lastrec != NULL)
	    lastrec->next = pollrec->next;
	  else
	    poll_records = pollrec->next;

	  pollrec->next = poll_free_list;
	  poll_free_list = pollrec;

	  n_poll_records--;
	  break;
	}
      lastrec = pollrec;
      pollrec = pollrec->next;
    }

  G_UNLOCK (main_loop);
}

void 
g_main_set_poll_func (GPollFunc func)
{
  if (func)
    poll_func = func;
  else
#ifdef HAVE_POLL
    poll_func = (GPollFunc)poll;
#else
    poll_func = (GPollFunc)g_poll;
#endif
}

/* Timeouts */

static gboolean 
g_timeout_prepare  (gpointer source_data, 
		    GTimeVal *current_time,
		    gint    *timeout)
{
  glong msec;
  GTimeoutData *data = source_data;

  msec = (data->expiration.tv_sec  - current_time->tv_sec) * 1000 +
         (data->expiration.tv_usec - current_time->tv_usec) / 1000;

  *timeout = (msec <= 0) ? 0 : msec;

  return (msec <= 0);
}

static gboolean 
g_timeout_check    (gpointer source_data,
		    GTimeVal *current_time)
{
  GTimeoutData *data = source_data;

  return (data->expiration.tv_sec < current_time->tv_sec) ||
         ((data->expiration.tv_sec == current_time->tv_sec) &&
	  (data->expiration.tv_usec <= current_time->tv_usec));
}

static gboolean
g_timeout_dispatch (gpointer source_data, 
		    GTimeVal *current_time,
		    gpointer user_data)
{
  GTimeoutData *data = source_data;

  if (data->callback(user_data))
    {
      guint seconds = data->interval / 1000;
      guint msecs = data->interval - seconds * 1000;

      data->expiration.tv_sec = current_time->tv_sec + seconds;
      data->expiration.tv_usec = current_time->tv_usec + msecs * 1000;
      if (data->expiration.tv_usec >= 1000000)
	{
	  data->expiration.tv_usec -= 1000000;
	  data->expiration.tv_sec++;
	}
      return TRUE;
    }
  else
    return FALSE;
}

guint 
g_timeout_add_full (gint           priority,
		    guint          interval, 
		    GSourceFunc    function,
		    gpointer       data,
		    GDestroyNotify notify)
{
  guint seconds;
  guint msecs;
  GTimeoutData *timeout_data = g_new (GTimeoutData, 1);

  timeout_data->interval = interval;
  timeout_data->callback = function;
  g_get_current_time (&timeout_data->expiration);

  seconds = timeout_data->interval / 1000;
  msecs = timeout_data->interval - seconds * 1000;

  timeout_data->expiration.tv_sec += seconds;
  timeout_data->expiration.tv_usec += msecs * 1000;
  if (timeout_data->expiration.tv_usec >= 1000000)
    {
      timeout_data->expiration.tv_usec -= 1000000;
      timeout_data->expiration.tv_sec++;
    }

  return g_source_add (priority, FALSE, &timeout_funcs, timeout_data, data, notify);
}

guint 
g_timeout_add (guint32        interval,
	       GSourceFunc    function,
	       gpointer       data)
{
  return g_timeout_add_full (G_PRIORITY_DEFAULT, 
			     interval, function, data, NULL);
}

/* Idle functions */

static gboolean 
g_idle_prepare  (gpointer source_data, 
		 GTimeVal *current_time,
		 gint     *timeout)
{
  timeout = 0;
  return TRUE;
}

static gboolean 
g_idle_check    (gpointer  source_data,
		 GTimeVal *current_time)
{
  return TRUE;
}

static gboolean
g_idle_dispatch (gpointer source_data, 
		 GTimeVal *current_time,
		 gpointer user_data)
{
  GIdleData *data = source_data;

  return (*data->callback)(user_data);
}

guint 
g_idle_add_full (gint          priority,
		 GSourceFunc    function,
		 gpointer       data,
		 GDestroyNotify notify)
{
  GIdleData *idle_data = g_new (GIdleData, 1);

  idle_data->callback = function;

  return g_source_add (priority, FALSE, &idle_funcs, idle_data, data, notify);
}

guint 
g_idle_add (GSourceFunc    function,
	    gpointer       data)
{
  return g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, function, data, NULL);
}
