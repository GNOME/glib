/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GHook: Callback maintenance functions
 * Copyright (C) 1998 Tim Janik
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
#include	"glib.h"


/* --- defines --- */
#define	G_HOOKS_PREALLOC	(16)


/* --- functions --- */
void
g_hook_list_init (GHookList *hook_list,
		  guint	     hook_size)
{
  g_return_if_fail (hook_list != NULL);
  g_return_if_fail (hook_size >= sizeof (GHook));
  
  hook_list->seq_id = 1;
  hook_list->hook_size = hook_size;
  hook_list->is_setup = TRUE;
  hook_list->hooks = NULL;
  hook_list->hook_memchunk = g_mem_chunk_new ("GHook Memchunk",
					      hook_size,
					      hook_size * G_HOOKS_PREALLOC,
					      G_ALLOC_AND_FREE);
}

void
g_hook_list_clear (GHookList *hook_list)
{
  g_return_if_fail (hook_list != NULL);
  
  if (hook_list->is_setup)
    {
      GHook *hook;
      
      hook_list->is_setup = FALSE;
      
      hook = hook_list->hooks;
      if (!hook)
	{
	  g_mem_chunk_destroy (hook_list->hook_memchunk);
	  hook_list->hook_memchunk = NULL;
	}
      else
	do
	  {
	    register GHook *tmp;
	    
	    g_hook_ref (hook_list, hook);
	    g_hook_destroy_link (hook_list, hook);
	    tmp = hook->next;
	    g_hook_unref (hook_list, hook);
	    hook = tmp;
	  }
	while (hook);
    }
}

GHook*
g_hook_alloc (GHookList *hook_list)
{
  GHook *hook;
  
  g_return_val_if_fail (hook_list != NULL, NULL);
  g_return_val_if_fail (hook_list->is_setup, NULL);
  
  hook = g_chunk_new0 (GHook, hook_list->hook_memchunk);
  hook->data = NULL;
  hook->next = NULL;
  hook->prev = NULL;
  hook->flags = G_HOOK_ACTIVE;
  hook->ref_count = 0;
  hook->id = 0;
  hook->func = NULL;
  hook->destroy = NULL;
  
  return hook;
}

void
g_hook_free (GHookList *hook_list,
	     GHook     *hook)
{
  g_return_if_fail (hook_list != NULL);
  g_return_if_fail (hook_list->is_setup);
  g_return_if_fail (hook != NULL);
  g_return_if_fail (G_HOOK_IS_UNLINKED (hook));
  
  g_chunk_free (hook, hook_list->hook_memchunk);
}

void
g_hook_destroy_link (GHookList *hook_list,
		     GHook     *hook)
{
  g_return_if_fail (hook_list != NULL);
  g_return_if_fail (hook != NULL);
  
  if (hook->id)
    {
      hook->id = 0;
      hook->flags &= ~G_HOOK_ACTIVE;
      if (hook->destroy)
	{
	  register GDestroyNotify destroy;
	  
	  destroy = hook->destroy;
	  hook->destroy = NULL;
	  destroy (hook->data);
	}
      g_hook_unref (hook_list, hook); /* counterpart to g_hook_insert_before */
    }
}

gboolean
g_hook_destroy (GHookList   *hook_list,
		guint	     id)
{
  GHook *hook;
  
  g_return_val_if_fail (hook_list != NULL, FALSE);
  g_return_val_if_fail (id > 0, FALSE);
  
  hook = g_hook_get (hook_list, id);
  if (hook)
    {
      g_hook_destroy_link (hook_list, hook);
      return TRUE;
    }
  
  return FALSE;
}

void
g_hook_unref (GHookList *hook_list,
	      GHook	*hook)
{
  g_return_if_fail (hook_list != NULL);
  g_return_if_fail (hook != NULL);
  g_return_if_fail (hook->ref_count > 0);
  
  hook->ref_count--;
  if (!hook->ref_count)
    {
      g_return_if_fail (hook->id == 0);
      g_return_if_fail (!G_HOOK_IS_IN_CALL (hook));
      
      if (hook->prev)
	hook->prev->next = hook->next;
      else
	hook_list->hooks = hook->next;
      if (hook->next)
	{
	  hook->next->prev = hook->prev;
	  hook->next = NULL;
	}
      hook->prev = NULL;
      
      g_chunk_free (hook, hook_list->hook_memchunk);
      
      if (!hook_list->hooks &&
	  !hook_list->is_setup)
	{
	  g_mem_chunk_destroy (hook_list->hook_memchunk);
	  hook_list->hook_memchunk = NULL;
	}
    }
}

void
g_hook_ref (GHookList *hook_list,
	    GHook     *hook)
{
  g_return_if_fail (hook_list != NULL);
  g_return_if_fail (hook != NULL);
  g_return_if_fail (hook->ref_count > 0);
  
  hook->ref_count++;
}

void
g_hook_prepend (GHookList *hook_list,
		GHook	  *hook)
{
  g_return_if_fail (hook_list != NULL);
  
  g_hook_insert_before (hook_list, hook_list->hooks, hook);
}

void
g_hook_insert_before (GHookList *hook_list,
		      GHook	*sibling,
		      GHook	*hook)
{
  g_return_if_fail (hook_list != NULL);
  g_return_if_fail (hook_list->is_setup);
  g_return_if_fail (hook != NULL);
  g_return_if_fail (G_HOOK_IS_UNLINKED (hook));
  g_return_if_fail (hook->func != NULL);
  
  hook->id = hook_list->seq_id++;
  hook->ref_count = 1; /* counterpart to g_hook_destroy_link */
  
  if (sibling)
    {
      if (sibling->prev)
	{
	  hook->prev = sibling->prev;
	  hook->prev->next = hook;
	  hook->next = sibling;
	  sibling->prev = hook;
	}
      else
	{
	  hook_list->hooks = hook;
	  hook->next = sibling;
	  sibling->prev = hook;
	}
    }
  else
    {
      if (hook_list->hooks)
	{
	  sibling = hook_list->hooks;
	  while (sibling->next)
	    sibling = sibling->next;
	  hook->prev = sibling;
	  sibling->next = hook;
	}
      else
	hook_list->hooks = hook;
    }
}

void
g_hook_list_invoke (GHookList *hook_list,
		    gboolean   may_recurse)
{
  GHook *hook;
  
  g_return_if_fail (hook_list != NULL);
  g_return_if_fail (hook_list->is_setup);
  
  hook = g_hook_first_valid (hook_list);
  while (hook)
    {
      register GHook *tmp;
      register GHookFunc func;
      gboolean was_in_call;
      
      g_hook_ref (hook_list, hook);
      func = hook->func;
      
      was_in_call = G_HOOK_IS_IN_CALL (hook);
      hook->flags |= G_HOOK_IN_CALL;
      func (hook->data);
      if (!was_in_call)
	hook->flags &= ~G_HOOK_IN_CALL;
      
      if (may_recurse)
	tmp = g_hook_next_valid (hook);
      else
	do
	  tmp = g_hook_next_valid (hook);
	while (tmp && G_HOOK_IS_IN_CALL (tmp));
      
      g_hook_unref (hook_list, hook);
      hook = tmp;
    }
}

void
g_hook_list_invoke_check (GHookList *hook_list,
			  gboolean   may_recurse)
{
  GHook *hook;
  
  g_return_if_fail (hook_list != NULL);
  g_return_if_fail (hook_list->is_setup);
  
  hook = g_hook_first_valid (hook_list);
  while (hook)
    {
      register GHook *tmp;
      register GHookCheckFunc func;
      gboolean was_in_call;
      register gboolean need_destroy;
      
      g_hook_ref (hook_list, hook);
      func = hook->func;
      
      was_in_call = G_HOOK_IS_IN_CALL (hook);
      hook->flags |= G_HOOK_IN_CALL;
      need_destroy = !func (hook->data);
      if (!was_in_call)
	hook->flags &= ~G_HOOK_IN_CALL;
      if (need_destroy)
	g_hook_destroy_link (hook_list, hook);
      
      if (may_recurse)
	tmp = g_hook_next_valid (hook);
      else
	do
	  tmp = g_hook_next_valid (hook);
	while (tmp && G_HOOK_IS_IN_CALL (tmp));
      
      g_hook_unref (hook_list, hook);
      hook = tmp;
    }
}

void
g_hook_list_marshall (GHookList		     *hook_list,
		      gboolean		      may_recurse,
		      GHookMarshaller	      marshaller,
		      gpointer		      data)
{
  GHook *hook;
  
  g_return_if_fail (hook_list != NULL);
  g_return_if_fail (hook_list->is_setup);
  g_return_if_fail (marshaller != NULL);
  
  hook = g_hook_first_valid (hook_list);
  while (hook)
    {
      register GHook *tmp;
      gboolean was_in_call;
      
      g_hook_ref (hook_list, hook);
      
      was_in_call = G_HOOK_IS_IN_CALL (hook);
      hook->flags |= G_HOOK_IN_CALL;
      marshaller (hook, data);
      if (!was_in_call)
	hook->flags &= ~G_HOOK_IN_CALL;
      
      if (may_recurse)
	tmp = g_hook_next_valid (hook);
      else
	do
	  tmp = g_hook_next_valid (hook);
	while (tmp && G_HOOK_IS_IN_CALL (tmp));
      
      g_hook_unref (hook_list, hook);
      hook = tmp;
    }
}

GHook*
g_hook_first_valid (GHookList *hook_list)
{
  g_return_val_if_fail (hook_list != NULL, NULL);
  
  if (hook_list->is_setup)
    {
      GHook *hook;
      
      hook = hook_list->hooks;
      if (hook)
	{
	  if (G_HOOK_IS_VALID (hook))
	    return hook;
	  else
	    return g_hook_next_valid (hook);
	}
    }
  
  return NULL;
}

GHook*
g_hook_next_valid (GHook *hook)
{
  if (!hook)
    return NULL;
  
  hook = hook->next;
  while (hook)
    {
      if (G_HOOK_IS_VALID (hook))
	return hook;
      hook = hook->next;
    }
  
  return NULL;
}

GHook*
g_hook_get (GHookList *hook_list,
	    guint      id)
{
  GHook *hook;
  
  g_return_val_if_fail (hook_list != NULL, NULL);
  g_return_val_if_fail (id > 0, NULL);
  
  hook = hook_list->hooks;
  while (hook)
    {
      if (hook->id == id)
	return hook;
      hook = hook->next;
    }
  
  return NULL;
}

GHook*
g_hook_find (GHookList	  *hook_list,
	     gboolean	   need_valids,
	     GHookFindFunc func,
	     gpointer	   data)
{
  GHook *hook;
  
  g_return_val_if_fail (hook_list != NULL, NULL);
  g_return_val_if_fail (func != NULL, NULL);
  
  hook = g_hook_first_valid (hook_list);
  while (hook)
    {
      register GHook *tmp;
      
      g_hook_ref (hook_list, hook);
      
      if (func (hook, data) && hook->id && (!need_valids || G_HOOK_IS_ACTIVE (hook)))
	{
	  g_hook_unref (hook_list, hook);
	  
	  return hook;
	}
      tmp = g_hook_next_valid (hook);
      g_hook_unref (hook_list, hook);
      hook = tmp;
    }
  
  return NULL;
}

GHook*
g_hook_find_data (GHookList *hook_list,
		  gboolean   need_valids,
		  gpointer   data)
{
  register GHook *hook;
  
  g_return_val_if_fail (hook_list != NULL, NULL);
  
  hook = g_hook_first_valid (hook_list);
  while (hook)
    {
      if (hook->data == data && hook->id && (!need_valids || G_HOOK_IS_ACTIVE (hook)))
	return hook;
      
      hook = g_hook_next_valid (hook);
    }
  
  return NULL;
}

GHook*
g_hook_find_func (GHookList *hook_list,
		  gboolean   need_valids,
		  gpointer   func)
{
  register GHook *hook;
  
  g_return_val_if_fail (hook_list != NULL, NULL);
  g_return_val_if_fail (func != NULL, NULL);
  
  hook = g_hook_first_valid (hook_list);
  while (hook)
    {
      if (hook->func == func && hook->id && (!need_valids || G_HOOK_IS_ACTIVE (hook)))
	return hook;
      
      hook = g_hook_next_valid (hook);
    }
  
  return NULL;
}

GHook*
g_hook_find_func_data (GHookList *hook_list,
		       gboolean	  need_valids,
		       gpointer	  func,
		       gpointer	  data)
{
  register GHook *hook;
  
  g_return_val_if_fail (hook_list != NULL, NULL);
  g_return_val_if_fail (func != NULL, NULL);
  
  hook = g_hook_first_valid (hook_list);
  while (hook)
    {
      if (hook->data == data && hook->func == func && hook->id && (!need_valids || G_HOOK_IS_ACTIVE (hook)))
	return hook;
      
      hook = g_hook_next_valid (hook);
    }
  
  return NULL;
}

void
g_hook_insert_sorted (GHookList	      *hook_list,
		      GHook	      *hook,
		      GHookCompareFunc func)
{
  GHook *sibling;
  
  g_return_if_fail (hook_list != NULL);
  g_return_if_fail (hook_list->is_setup);
  g_return_if_fail (hook != NULL);
  g_return_if_fail (G_HOOK_IS_UNLINKED (hook));
  g_return_if_fail (hook->func != NULL);
  g_return_if_fail (func != NULL);
  
  sibling = g_hook_first_valid (hook_list);
  
  while (sibling)
    {
      register GHook *tmp;
      
      g_hook_ref (hook_list, sibling);
      if (func (hook, sibling) <= 0 && sibling->id)
	{
	  g_hook_unref (hook_list, sibling);
	  break;
	}
      tmp = g_hook_next_valid (sibling);
      g_hook_unref (hook_list, sibling);
      sibling = tmp;
    }
  
  g_hook_insert_before (hook_list, sibling, hook);
}

gint
g_hook_compare_ids (GHook *new_hook,
		    GHook *sibling)
{
  return ((glong) new_hook->id) - ((glong) sibling->id);
}
