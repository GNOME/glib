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
 *
 * gbsearcharray.h: binary searchable sorted array maintenance
 */
#ifndef __G_BSEARCH_ARRAY_H__
#define __G_BSEARCH_ARRAY_H__

#include        <glib/gtypes.h>
#include        <glib/gutils.h>
#include        <glib/gmem.h>
#include        <glib/gmessages.h>

G_BEGIN_DECLS

/* helper macro to avoid signed overflow for value comparisions */
#define	G_BSEARCH_ARRAY_CMP(v1,v2) ((v1) < (v2) ? -1 : (v1) > (v2))


/* --- typedefs --- */
typedef union  _GBSearchArray         GBSearchArray;
typedef struct _GBSearchConfig        GBSearchConfig;
typedef gint  (*GBSearchCompareFunc) (gconstpointer bsearch_node1,
				      gconstpointer bsearch_node2);
typedef enum
{
  G_BSEARCH_ARRAY_ALIGN_POWER2	= 1 << 0,
  G_BSEARCH_ARRAY_DEFER_SHRINK	= 1 << 1
} GBSearchArrayFlags;


/* --- structures --- */
struct _GBSearchConfig
{
  guint16             sizeof_node;
  GBSearchCompareFunc cmp_nodes;
  guint16	      flags;
};
union _GBSearchArray
{
  guint    n_nodes;
  gpointer alignment_dummy1;
  glong    alignment_dummy2;
  gdouble  alignment_dummy3;
};


/* --- prototypes --- */
GBSearchArray*	g_bsearch_array_new		(GBSearchConfig *bconfig);
void		g_bsearch_array_destroy		(GBSearchArray	*barray,
						 GBSearchConfig *bconfig);
GBSearchArray*	g_bsearch_array_insert		(GBSearchArray	*barray,
						 GBSearchConfig *bconfig,
						 gconstpointer	 key_node,
						 gboolean	 replace_existing);
GBSearchArray*	g_bsearch_array_remove		(GBSearchArray	*barray,
						 GBSearchConfig	*bconfig,
						 gconstpointer	 key_node);
GBSearchArray*	g_bsearch_array_remove_node	(GBSearchArray	*barray,
						 GBSearchConfig *bconfig,
						 gpointer	 node_in_array);
G_INLINE_FUNC
gpointer	g_bsearch_array_lookup		(GBSearchArray	*barray,
						 GBSearchConfig *bconfig,
						 gconstpointer	 key_node);
G_INLINE_FUNC
gpointer	g_bsearch_array_get_nth		(GBSearchArray	*barray,
                                                 GBSearchConfig *bconfig,
						 guint		 n);
G_INLINE_FUNC
guint		g_bsearch_array_get_index	(GBSearchArray	*barray,
						 GBSearchConfig *bconfig,
						 gpointer	 node_in_array);


/* initialization of static arrays */
#define	G_STATIC_BCONFIG(sizeof_node, cmp_nodes, flags) \
  { (sizeof_node), (cmp_nodes), (flags), }


/* --- implementation details --- */
#define G_BSEARCH_ARRAY_NODES(barray)    ((gpointer) (((guint8*) (barray)) + sizeof (GBSearchArray)))
#if defined (G_CAN_INLINE) || defined (__G_BSEARCHARRAY_C__)
G_INLINE_FUNC gpointer
g_bsearch_array_lookup (GBSearchArray  *barray,
			GBSearchConfig *bconfig,
			gconstpointer   key_node)
{
  if (barray->n_nodes > 0)
    {
      GBSearchCompareFunc cmp_nodes = bconfig->cmp_nodes;
      gint sizeof_node = bconfig->sizeof_node;
      guint n_nodes = barray->n_nodes;
      guint8 *nodes = G_BSEARCH_ARRAY_NODES (barray);

      nodes -= sizeof_node;
      do
	{
	  guint8 *check;
	  guint i;
	  register gint cmp;
	  
	  i = (n_nodes + 1) >> 1;
	  check = nodes + i * sizeof_node;
	  cmp = cmp_nodes (key_node, check);
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
g_bsearch_array_get_nth (GBSearchArray  *barray,
			 GBSearchConfig *bconfig,
			 guint		 n)
{
  if (n < barray->n_nodes)
    {
      guint8 *nodes = (guint8*) G_BSEARCH_ARRAY_NODES (barray);

      return nodes + n * bconfig->sizeof_node;
    }
  else
    return NULL;
}
G_INLINE_FUNC
guint
g_bsearch_array_get_index (GBSearchArray  *barray,
			   GBSearchConfig *bconfig,
			   gpointer        node_in_array)
{
  guint distance = ((guint8*) node_in_array) - ((guint8*) G_BSEARCH_ARRAY_NODES (barray));

  distance /= bconfig->sizeof_node;

  return MIN (distance, barray->n_nodes);
}
#endif  /* G_CAN_INLINE || __G_BSEARCHARRAY_C__ */

G_END_DECLS

#endif /* __G_BSEARCH_ARRAY_H__ */
