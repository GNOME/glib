/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GAsyncQueue: asyncronous queue implementation, based on Gqueue.
 * Copyright (C) 2000 Sebastian Wilhelmi; University of Karlsruhe
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

/*
 * MT safe
 */

#include "glib.h"

struct _GAsyncQueue
{
  GMutex *mutex;
  GCond *cond;
  GQueue *queue;
  guint waiting_threads;
  guint ref_count;
};

GAsyncQueue*
g_async_queue_new ()
{
  GAsyncQueue* retval = g_new (GAsyncQueue, 1);
  retval->mutex = g_mutex_new ();
  retval->cond = g_cond_new ();
  retval->queue = g_queue_new ();
  retval->waiting_threads = 0;
  retval->ref_count = 1;
  return retval;
}

void 
g_async_queue_ref (GAsyncQueue *queue)
{
  g_return_if_fail (queue);
  g_return_if_fail (queue->ref_count > 0);
  
  g_mutex_lock (queue->mutex);
  queue->ref_count++;
  g_mutex_unlock (queue->mutex);
}

void 
g_async_queue_ref_unlocked (GAsyncQueue *queue)
{
  g_return_if_fail (queue);
  g_return_if_fail (queue->ref_count > 0);
  
  queue->ref_count++;
}

void 
g_async_queue_unref_and_unlock (GAsyncQueue *queue)
{
  gboolean stop;

  g_return_if_fail (queue);
  g_return_if_fail (queue->ref_count > 0);

  queue->ref_count--;
  stop = (queue->ref_count == 0);
  g_mutex_unlock (queue->mutex);
  
  if (stop)
    {
      g_return_if_fail (queue->waiting_threads == 0);
      g_mutex_free (queue->mutex);
      g_cond_free (queue->cond);
      g_queue_free (queue->queue);
      g_free (queue);
    }
}

void 
g_async_queue_unref (GAsyncQueue *queue)
{
  g_return_if_fail (queue);
  g_return_if_fail (queue->ref_count > 0);

  g_mutex_lock (queue->mutex);
  g_async_queue_unref_and_unlock (queue);
}

void
g_async_queue_lock (GAsyncQueue *queue)
{
  g_return_if_fail (queue);
  g_return_if_fail (queue->ref_count > 0);

  g_mutex_lock (queue->mutex);
}

void 
g_async_queue_unlock (GAsyncQueue *queue)
{
  g_return_if_fail (queue);
  g_return_if_fail (queue->ref_count > 0);

  g_mutex_unlock (queue->mutex);
}

void
g_async_queue_push (GAsyncQueue* queue, gpointer data)
{
  g_return_if_fail (queue);
  g_return_if_fail (queue->ref_count > 0);
  g_return_if_fail (data);

  g_mutex_lock (queue->mutex);
  g_async_queue_push_unlocked (queue, data);
  g_mutex_unlock (queue->mutex);
}

void
g_async_queue_push_unlocked (GAsyncQueue* queue, gpointer data)
{
  g_return_if_fail (queue);
  g_return_if_fail (queue->ref_count > 0);
  g_return_if_fail (data);

  g_queue_push_head (queue->queue, data);
  g_cond_signal (queue->cond);
}

static gpointer
g_async_queue_pop_intern_unlocked (GAsyncQueue* queue, gboolean try, 
				   GTimeVal *end_time)
{
  gpointer retval;

  if (!g_queue_peek_tail (queue->queue))
    {
      if (try)
	return NULL;
      if (!end_time)
        {
          queue->waiting_threads++;
	  while (!g_queue_peek_tail (queue->queue))
            g_cond_wait(queue->cond, queue->mutex);
          queue->waiting_threads--;
        }
      else
        {
          queue->waiting_threads++;
          while (!g_queue_peek_tail (queue->queue))
            if (!g_cond_timed_wait (queue->cond, queue->mutex, end_time))
              break;
          queue->waiting_threads--;
          if (!g_queue_peek_tail (queue->queue))
	    return NULL;
        }
    }

  retval = g_queue_pop_tail (queue->queue);

  g_assert (retval);

  return retval;
}

gpointer
g_async_queue_pop (GAsyncQueue* queue)
{
  gpointer retval;

  g_return_val_if_fail (queue, NULL);
  g_return_val_if_fail (queue->ref_count > 0, NULL);

  g_mutex_lock (queue->mutex);
  retval = g_async_queue_pop_intern_unlocked (queue, FALSE, NULL);
  g_mutex_unlock (queue->mutex);

  return retval;
}

gpointer
g_async_queue_pop_unlocked (GAsyncQueue* queue)
{
  g_return_val_if_fail (queue, NULL);
  g_return_val_if_fail (queue->ref_count > 0, NULL);

  return g_async_queue_pop_intern_unlocked (queue, FALSE, NULL);
}

gpointer
g_async_queue_try_pop (GAsyncQueue* queue)
{
  gpointer retval;

  g_return_val_if_fail (queue, NULL);
  g_return_val_if_fail (queue->ref_count > 0, NULL);

  g_mutex_lock (queue->mutex);
  retval = g_async_queue_pop_intern_unlocked (queue, TRUE, NULL);
  g_mutex_unlock (queue->mutex);

  return retval;
}

gpointer
g_async_queue_try_pop_unlocked (GAsyncQueue* queue)
{
  g_return_val_if_fail (queue, NULL);
  g_return_val_if_fail (queue->ref_count > 0, NULL);

  return g_async_queue_pop_intern_unlocked (queue, TRUE, NULL);
}

gpointer
g_async_queue_timed_pop (GAsyncQueue* queue, GTimeVal *end_time)
{
  gpointer retval;

  g_return_val_if_fail (queue, NULL);
  g_return_val_if_fail (queue->ref_count > 0, NULL);

  g_mutex_lock (queue->mutex);
  retval = g_async_queue_pop_intern_unlocked (queue, FALSE, end_time);
  g_mutex_unlock (queue->mutex);

  return retval;  
}

gpointer
g_async_queue_timed_pop_unlocked (GAsyncQueue* queue, GTimeVal *end_time)
{
  g_return_val_if_fail (queue, NULL);
  g_return_val_if_fail (queue->ref_count > 0, NULL);

  return g_async_queue_pop_intern_unlocked (queue, FALSE, end_time);
}

gint
g_async_queue_length_unlocked (GAsyncQueue* queue)
{
  g_return_val_if_fail (queue, 0);
  g_return_val_if_fail (queue->ref_count > 0, 0);

  return queue->queue->length - queue->waiting_threads;
}

gint
g_async_queue_length(GAsyncQueue* queue)
{
  glong retval;

  g_return_val_if_fail (queue, 0);
  g_return_val_if_fail (queue->ref_count > 0, 0);

  g_mutex_lock (queue->mutex);
  retval = queue->queue->length - queue->waiting_threads;
  g_mutex_unlock (queue->mutex);

  return retval;
}

