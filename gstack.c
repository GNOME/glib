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


GStack *
g_stack_new (void)
{
  GStack *s;
  
  s = g_new (GStack, 1);
  if (!s)
    return NULL;

  s->list = NULL;

  return s;
}


void
g_stack_free (GStack *stack)
{
  if (stack)
    {
      if (stack->list)
        g_list_free (stack->list);

      g_free (stack);
    }
}


gpointer
g_stack_pop (GStack *stack)
{
  gpointer data = NULL;

  if ((stack) && (stack->list))
    {
      GList *node = stack->list;

      stack->list = stack->list->next;

      data = node->data;

      g_list_free_1 (node);
    }

  return data;
}


