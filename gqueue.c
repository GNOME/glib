/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1999 Free Software Foundation, Inc.
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


#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>




GQueue *
g_queue_new (void)
{
  GQueue *q = g_new (GQueue, 1);

  q->list = q->list_end = NULL;
  q->list_size = 0;

  return q;
}


void
g_queue_free (GQueue *q)
{
  if (q)
    {
      if (q->list)
        g_list_free (q->list);
      g_free (q);
    }
}


guint
g_queue_get_size (GQueue *q)
{
  return (q == NULL) ? 0 : q->list_size;
}


void
g_queue_push_front (GQueue *q, gpointer data)
{
  if (q)
    {
      q->list = g_list_prepend (q->list, data);

      if (q->list_end == NULL)
        q->list_end = q->list;

      q->list_size++;
    }
}


void
g_queue_push_back (GQueue *q, gpointer data)
{
  if (q)
    {
      q->list_end = g_list_append (q->list_end, data);

      if (! q->list)
        q->list = q->list_end;
      else
        q->list_end = q->list_end->next;

      q->list_size++;
    }
}


gpointer
g_queue_pop_front (GQueue *q)
{
  gpointer data = NULL;

  if ((q) && (q->list))
    {
      GList *node;

      node = q->list;
      data = node->data;

      if (! node->next)
        {
          q->list = q->list_end = NULL;
          q->list_size = 0;
        }
      else
        {
          q->list = node->next;
          q->list->prev = NULL;
          q->list_size--;
        }

      g_list_free_1 (node);
    }

  return data;
}


gpointer
g_queue_pop_back (GQueue *q)
{
  gpointer data = NULL;

  if ((q) && (q->list))
    {
      GList *node;

      node = q->list_end;
      data = node->data;

      if (! node->prev)
	{
          q->list = q->list_end = NULL;
          q->list_size = 0;
        }
      else
	{
          q->list_end = node->prev;
          q->list_end->next = NULL;
          q->list_size--;
        }

      g_list_free_1 (node);
    }

  return data;
}


