/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 2000-2001 Tim Janik
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#define G_IMPLEMENT_INLINES 1
#define __G_BSEARCHARRAY_C__
#include "gbsearcharray.h"

#include	<string.h>


/* --- structures --- */
static inline guint
upper_power2 (guint number)
{
#ifdef	DISABLE_MEM_POOLS
  return number;
#else	/* !DISABLE_MEM_POOLS */
  return number ? 1 << g_bit_storage (number - 1) : 0;
#endif	/* !DISABLE_MEM_POOLS */
}

GBSearchArray*
g_bsearch_array_new (GBSearchConfig *bconfig)
{
  GBSearchArray *barray;
  guint size;

  g_return_val_if_fail (bconfig != NULL, NULL);

  size = sizeof (GBSearchArray) + bconfig->sizeof_node;
  if (bconfig->flags & G_BSEARCH_ARRAY_ALIGN_POWER2)
    size = upper_power2 (size);
  barray = g_malloc0 (size);
  barray->n_nodes = 0;

  return barray;
}

void
g_bsearch_array_destroy (GBSearchArray  *barray,
			 GBSearchConfig *bconfig)
{
  g_return_if_fail (barray != NULL);

  g_free (barray);
}

static inline GBSearchArray*
bsearch_array_insert (GBSearchArray  *barray,
		      GBSearchConfig *bconfig,
		      gconstpointer   key_node,
		      gboolean        replace)
{
  gint sizeof_node;
  guint8 *check;
  
  sizeof_node = bconfig->sizeof_node;
  if (barray->n_nodes == 0)
    {
      guint new_size = sizeof (GBSearchArray) + sizeof_node;
      
      if (bconfig->flags & G_BSEARCH_ARRAY_ALIGN_POWER2)
	new_size = upper_power2 (new_size);
      barray = g_realloc (barray, new_size);
      barray->n_nodes = 1;
      check = G_BSEARCH_ARRAY_NODES (barray);
    }
  else
    {
      GBSearchCompareFunc cmp_nodes = bconfig->cmp_nodes;
      guint n_nodes = barray->n_nodes;
      guint8 *nodes = G_BSEARCH_ARRAY_NODES (barray);
      gint cmp;
      guint i;
      
      nodes -= sizeof_node;
      do
	{
	  i = (n_nodes + 1) >> 1;
	  check = nodes + i * sizeof_node;
	  cmp = cmp_nodes (key_node, check);
	  if (cmp > 0)
	    {
	      n_nodes -= i;
	      nodes = check;
	    }
	  else if (cmp < 0)
	    n_nodes = i - 1;
	  else /* if (cmp == 0) */
	    {
	      if (replace)
		memcpy (check, key_node, sizeof_node);
	      return barray;
	    }
	}
      while (n_nodes);
      /* grow */
      if (cmp > 0)
	check += sizeof_node;
      i = (check - ((guint8*) G_BSEARCH_ARRAY_NODES (barray))) / sizeof_node;
      n_nodes = barray->n_nodes++;
      if (bconfig->flags & G_BSEARCH_ARRAY_ALIGN_POWER2)
	{
	  guint new_size = upper_power2 (sizeof (GBSearchArray) + barray->n_nodes * sizeof_node);
	  guint old_size = upper_power2 (sizeof (GBSearchArray) + n_nodes * sizeof_node);
	  
	  if (new_size != old_size)
	    barray = g_realloc (barray, new_size);
	}
      else
	barray = g_realloc (barray, sizeof (GBSearchArray) + barray->n_nodes * sizeof_node);
      check = ((guint8*) G_BSEARCH_ARRAY_NODES (barray)) + i * sizeof_node;
      g_memmove (check + sizeof_node, check, (n_nodes - i) * sizeof_node);
    }
  memcpy (check, key_node, sizeof_node);
  
  return barray;
}

GBSearchArray*
g_bsearch_array_insert (GBSearchArray  *barray,
			GBSearchConfig *bconfig,
			gconstpointer   key_node,
			gboolean        replace_existing)
{
  g_return_val_if_fail (barray != NULL, NULL);
  g_return_val_if_fail (bconfig != NULL, barray);
  g_return_val_if_fail (key_node != NULL, barray);
  
  return bsearch_array_insert (barray, bconfig, key_node, replace_existing);
}

GBSearchArray*
g_bsearch_array_remove_node (GBSearchArray  *barray,
			     GBSearchConfig *bconfig,
			     gpointer       _node_in_array)
{
  guint8 *nodes, *bound, *node_in_array = _node_in_array;
  guint old_size;
  
  g_return_val_if_fail (barray != NULL, NULL);
  g_return_val_if_fail (bconfig != NULL, barray);
  
  nodes = G_BSEARCH_ARRAY_NODES (barray);
  old_size = bconfig->sizeof_node;
  old_size *= barray->n_nodes;  /* beware of int widths */
  bound = nodes + old_size;
  
  g_return_val_if_fail (node_in_array >= nodes && node_in_array < bound, barray);

  bound -= bconfig->sizeof_node;
  barray->n_nodes -= 1;
  g_memmove (node_in_array, node_in_array + bconfig->sizeof_node, (bound - node_in_array) / bconfig->sizeof_node);
  
  if ((bconfig->flags & G_BSEARCH_ARRAY_DEFER_SHRINK) == 0)
    {
      guint new_size = bound - nodes;   /* old_size - bconfig->sizeof_node */
      
      if (bconfig->flags & G_BSEARCH_ARRAY_ALIGN_POWER2)
	{
	  new_size = upper_power2 (sizeof (GBSearchArray) + new_size);
	  old_size = upper_power2 (sizeof (GBSearchArray) + old_size);
	  if (old_size != new_size)
	    barray = g_realloc (barray, new_size);
	}
      else
	barray = g_realloc (barray, sizeof (GBSearchArray) + new_size);
    }
  return barray;
}

GBSearchArray*
g_bsearch_array_remove (GBSearchArray  *barray,
			GBSearchConfig *bconfig,
			gconstpointer   key_node)
{
  gpointer node_in_array;
  
  g_return_val_if_fail (barray != NULL, NULL);
  g_return_val_if_fail (bconfig != NULL, barray);
  
  node_in_array = g_bsearch_array_lookup (barray, bconfig, key_node);
  if (!node_in_array)
    {
      g_warning (G_STRLOC ": unable to remove unexistant node");
      return barray;
    }
  else
    return g_bsearch_array_remove_node (barray, bconfig, node_in_array);
}
