/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GQueue: Double ended queue implementation, piggy backed on GList.
 * Copyright (C) 1998 Tim Janik
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
 * MT safe
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "glib.h"


G_LOCK_DEFINE_STATIC (queue_memchunk);
static GMemChunk   *queue_memchunk = NULL;
static GTrashStack *free_queue_nodes = NULL;

/**
 * g_queue_new:
 *
 * Creates a new #GQueue. 
 *
 * Returns: a new #GQueue.
 **/
GQueue*
g_queue_new (void)
{
  GQueue *queue;

  G_LOCK (queue_memchunk);
  queue = g_trash_stack_pop (&free_queue_nodes);

  if (!queue)
    {
      if (!queue_memchunk)
	queue_memchunk = g_mem_chunk_new ("GLib GQueue chunk",
					  sizeof (GNode),
					  sizeof (GNode) * 128,
					  G_ALLOC_ONLY);
      queue = g_chunk_new (GQueue, queue_memchunk);
    }
  G_UNLOCK (queue_memchunk);

  queue->head = NULL;
  queue->tail = NULL;
  queue->length = 0;

  return queue;
}

/**
 * g_queue_free:
 * @queue: a #GQueue.
 *
 * Frees the memory allocated for the #GQueue.
 **/
void
g_queue_free (GQueue *queue)
{
  g_return_if_fail (queue != NULL);

  g_list_free (queue->head);

#ifdef ENABLE_GC_FRIENDLY
  queue->head = NULL;
  queue->tail = NULL;
#endif /* ENABLE_GC_FRIENDLY */

  G_LOCK (queue_memchunk);
  g_trash_stack_push (&free_queue_nodes, queue);
  G_UNLOCK (queue_memchunk);
}

/**
 * g_queue_push_head:
 * @queue: a #GQueue.
 * @data: the data for the new element.
 *
 * Adds a new element at the head of the queue.
 **/
void
g_queue_push_head (GQueue  *queue,
		   gpointer data)
{
  g_return_if_fail (queue != NULL);

  queue->head = g_list_prepend (queue->head, data);
  if (!queue->tail)
    queue->tail = queue->head;
  queue->length++;
}

/**
 * g_queue_push_head_link:
 * @queue: a #GQueue.
 * @link: a single #GList element, <emphasis>not</emphasis> a list with
 *   more than one element.
 *
 * Adds a new element at the head of the queue.
 **/
void
g_queue_push_head_link (GQueue *queue,
			GList  *link)
{
  g_return_if_fail (queue != NULL);
  g_return_if_fail (link != NULL);
  g_return_if_fail (link->prev == NULL);
  g_return_if_fail (link->next == NULL);

  link->next = queue->head;
  if (queue->head)
    queue->head->prev = link;
  else
    queue->tail = link;
  queue->head = link;
  queue->length++;
}

/**
 * g_queue_push_tail:
 * @queue: a #GQueue.
 * @data: the data for the new element.
 *
 * Adds a new element at the tail of the queue.
 **/
void
g_queue_push_tail (GQueue  *queue,
		   gpointer data)
{
  g_return_if_fail (queue != NULL);

  queue->tail = g_list_append (queue->tail, data);
  if (queue->tail->next)
    queue->tail = queue->tail->next;
  else
    queue->head = queue->tail;
  queue->length++;
}

/**
 * g_queue_push_tail_link:
 * @queue: a #GQueue.
 * @link: a single #GList element, <emphasis>not</emphasis> a list with
 *   more than one element.
 *
 * Adds a new element at the tail of the queue.
 **/
void
g_queue_push_tail_link (GQueue *queue,
			GList  *link)
{
  g_return_if_fail (queue != NULL);
  g_return_if_fail (link != NULL);
  g_return_if_fail (link->prev == NULL);
  g_return_if_fail (link->next == NULL);

  link->prev = queue->tail;
  if (queue->tail)
    queue->tail->next = link;
  else
    queue->head = link;
  queue->tail = link;
  queue->length++;
}

/**
 * g_queue_pop_head:
 * @queue: a #GQueue.
 *
 * Removes the first element of the queue.
 *
 * Returns: the data of the first element in the queue, or %NULL if the queue
 *   is empty.
 **/
gpointer
g_queue_pop_head (GQueue *queue)
{
  g_return_val_if_fail (queue != NULL, NULL);

  if (queue->head)
    {
      GList *node = queue->head;
      gpointer data = node->data;

      queue->head = node->next;
      if (queue->head)
	queue->head->prev = NULL;
      else
	queue->tail = NULL;
      g_list_free_1 (node);
      queue->length--;

      return data;
    }

  return NULL;
}

/**
 * g_queue_pop_head_link:
 * @queue: a #GQueue.
 *
 * Removes the first element of the queue.
 *
 * Returns: the #GList element at the head of the queue, or %NULL if the queue
 *   is empty.
 **/
GList*
g_queue_pop_head_link (GQueue *queue)
{
  g_return_val_if_fail (queue != NULL, NULL);

  if (queue->head)
    {
      GList *node = queue->head;

      queue->head = node->next;
      if (queue->head)
	{
	  queue->head->prev = NULL;
	  node->next = NULL;
	}
      else
	queue->tail = NULL;
      queue->length--;

      return node;
    }

  return NULL;
}

/**
 * g_queue_pop_tail:
 * @queue: a #GQueue.
 *
 * Removes the last element of the queue.
 *
 * Returns: the data of the last element in the queue, or %NULL if the queue
 *   is empty.
 **/
gpointer
g_queue_pop_tail (GQueue *queue)
{
  g_return_val_if_fail (queue != NULL, NULL);

  if (queue->tail)
    {
      GList *node = queue->tail;
      gpointer data = node->data;

      queue->tail = node->prev;
      if (queue->tail)
	queue->tail->next = NULL;
      else
	queue->head = NULL;
      queue->length--;
      g_list_free_1 (node);

      return data;
    }
  
  return NULL;
}

/**
 * g_queue_pop_tail_link:
 * @queue: a #GQueue.
 *
 * Removes the last element of the queue.
 *
 * Returns: the #GList element at the tail of the queue, or %NULL if the queue
 *   is empty.
 **/
GList*
g_queue_pop_tail_link (GQueue *queue)
{
  g_return_val_if_fail (queue != NULL, NULL);
  
  if (queue->tail)
    {
      GList *node = queue->tail;
      
      queue->tail = node->prev;
      if (queue->tail)
	{
	  queue->tail->next = NULL;
	  node->prev = NULL;
	}
      else
	queue->head = NULL;
      queue->length--;
      
      return node;
    }
  
  return NULL;
}

/**
 * g_queue_is_empty:
 * @queue: a #GQueue.
 *
 * Returns %TRUE if the queue is empty.
 *
 * Returns: %TRUE if the queue is empty.
 **/
gboolean
g_queue_is_empty (GQueue *queue)
{
  g_return_val_if_fail (queue != NULL, TRUE);

  return queue->head == NULL;
}

/**
 * g_queue_peek_head:
 * @queue: a #GQueue.
 *
 * Returns the first element of the queue.
 *
 * Returns: the data of the first element in the queue, or %NULL if the queue
 *   is empty.
 **/
gpointer
g_queue_peek_head (GQueue *queue)
{
  g_return_val_if_fail (queue != NULL, NULL);

  return queue->head ? queue->head->data : NULL;
}

/**
 * g_queue_peek_tail:
 * @queue: a #GQueue.
 *
 * Returns the last element of the queue.
 *
 * Returns: the data of the last element in the queue, or %NULL if the queue
 *   is empty.
 **/
gpointer
g_queue_peek_tail (GQueue *queue)
{
  g_return_val_if_fail (queue != NULL, NULL);

  return queue->tail ? queue->tail->data : NULL;
}
