/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 2000 Tim Janik
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
 *
 * gbsearcharray.h: binary searchable sorted array maintenance
 */
#ifndef __G_BSEARCH_ARRAY_H__
#define __G_BSEARCH_ARRAY_H__

#include        <gobject/gtype.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* helper macro to avoid signed overflow for value comparisions */
#define	G_BSEARCH_ARRAY_CMP(v1,v2) ((v1) < (v2) ? -1 : (v1) > (v2) ? 1 : 0)


/* --- typedefs --- */
typedef struct _GBSearchArray         GBSearchArray;
typedef gint  (*GBSearchCompareFunc) (gconstpointer bsearch_node1,
				      gconstpointer bsearch_node2);
typedef enum
{
  G_BSEARCH_ALIGN_POWER2	= 1 << 0,
  G_BSEARCH_DEFER_SHRINK	= 1 << 1
} GBSearchFlags;


/* --- structures --- */
struct _GBSearchArray
{
  GBSearchCompareFunc cmp_func;
  guint16             sizeof_node;
  guint16	      flags;
  guint               n_nodes;
  gpointer            nodes;
};


/* --- prototypes --- */
gpointer	g_bsearch_array_insert		(GBSearchArray	*barray,
						 gconstpointer	 key_node,
						 gboolean	 replace_existing);
void		g_bsearch_array_remove		(GBSearchArray	*barray,
						 gconstpointer	 key_node);
void		g_bsearch_array_remove_node	(GBSearchArray	*barray,
						 gpointer	 node_in_array);
G_INLINE_FUNC
gpointer	g_bsearch_array_lookup		(GBSearchArray	*barray,
						 gconstpointer	 key_node);
G_INLINE_FUNC
gpointer	g_bsearch_array_get_nth		(GBSearchArray	*barray,
						 guint		 n);


/* --- implementation details --- */
#if defined (G_CAN_INLINE) || defined (__G_BSEARCHARRAY_C__)
G_INLINE_FUNC gpointer
g_bsearch_array_lookup (GBSearchArray *barray,
			gconstpointer  key_node)
{
  if (barray->n_nodes > 0)
    {
      GBSearchCompareFunc cmp_func = barray->cmp_func;
      gint sizeof_node = barray->sizeof_node;
      guint n_nodes = barray->n_nodes;
      guint8 *nodes = (guint8 *) barray->nodes;
      
      nodes -= sizeof_node;
      do
	{
	  guint8 *check;
	  guint i;
	  register gint cmp;
	  
	  i = (n_nodes + 1) >> 1;
	  check = nodes + i * sizeof_node;
	  cmp = cmp_func (key_node, check);
	  if (cmp == 0)
	    return check;
	  else if (cmp > 0)
	    {
	      n_nodes -= i;
	      nodes = check;
	    }
	  else /* if (cmp < 0) */
	    n_nodes = i - 1;
	}
      while (n_nodes);
    }
  
  return NULL;
}
G_INLINE_FUNC gpointer
g_bsearch_array_get_nth (GBSearchArray *barray,
			 guint		n)
{
  if (n < barray->n_nodes)
    {
      guint8 *nodes = (guint8 *) barray->nodes;

      return nodes + n * barray->sizeof_node;
    }
  else
    return NULL;
}
#endif  /* G_CAN_INLINE && __G_BSEARCHARRAY_C__ */




#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __G_BSEARCH_ARRAY_H__ */
