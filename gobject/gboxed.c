/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 2000 Red Hat, Inc.
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
#include	"gboxed.h"

#include	"gbsearcharray.h"
#include	"gvalue.h"
#include	"gvaluecollector.h"

#include <string.h>

/* --- typedefs & structures --- */
typedef struct
{
  GType		 type;
  GBoxedCopyFunc copy;
  GBoxedFreeFunc free;
} BoxedNode;


/* --- prototypes --- */
static gint	boxed_nodes_cmp		(gconstpointer	p1,
					 gconstpointer	p2);


/* --- variables --- */
static GBSearchArray boxed_bsa = { boxed_nodes_cmp, sizeof (BoxedNode), 0, 0, NULL };


/* --- functions --- */
static gint
boxed_nodes_cmp	(gconstpointer p1,
		 gconstpointer p2)
{
  const BoxedNode *node1 = p1, *node2 = p2;

  return G_BSEARCH_ARRAY_CMP (node1->type, node2->type);
}

void
g_boxed_type_init (void)  /* sync with gtype.c */
{
  static const GTypeInfo info = {
    0,                          /* class_size */
    NULL,                       /* base_init */
    NULL,                       /* base_destroy */
    NULL,                       /* class_init */
    NULL,                       /* class_destroy */
    NULL,                       /* class_data */
    0,                          /* instance_size */
    0,                          /* n_preallocs */
    NULL,                       /* instance_init */
    NULL,                       /* value_table */
  };
  const GTypeFundamentalInfo finfo = { G_TYPE_FLAG_DERIVABLE, };
  GType type;

  /* G_TYPE_BOXED
   */
  type = g_type_register_fundamental (G_TYPE_BOXED, "GBoxed", &info, &finfo, G_TYPE_FLAG_ABSTRACT);
  g_assert (type == G_TYPE_BOXED);
}

static void
boxed_proxy_value_init (GValue *value)
{
  value->data[0].v_pointer = 0;
}

static void
boxed_proxy_value_free (GValue *value)
{
  if (value->data[0].v_pointer && !(value->data[1].v_uint & G_VALUE_STATIC_TAG))
    {
      BoxedNode key, *node;

      key.type = value->g_type;
      node = g_bsearch_array_lookup (&boxed_bsa, &key);
      node->free (value->data[0].v_pointer);
    }
}

static void
boxed_proxy_value_copy (const GValue *src_value,
			GValue       *dest_value)
{
  if (src_value->data[0].v_pointer)
    {
      BoxedNode key, *node;

      key.type = src_value->g_type;
      node = g_bsearch_array_lookup (&boxed_bsa, &key);
      dest_value->data[0].v_pointer = node->copy (src_value->data[0].v_pointer);
    }
  else
    dest_value->data[0].v_pointer = src_value->data[0].v_pointer;
}

static gpointer
boxed_proxy_value_peek_pointer (const GValue *value)
{
  return value->data[0].v_pointer;
}

static gchar*
boxed_proxy_collect_value (GValue      *value,
			   guint        nth_value,
			   GType       *collect_type,
			   GTypeCValue *collect_value)
{
  BoxedNode key, *node;

  key.type = value->g_type;
  node = g_bsearch_array_lookup (&boxed_bsa, &key);
  value->data[0].v_pointer = node->copy (collect_value->v_pointer);

  *collect_type = 0;
  return NULL;
}

static gchar*
boxed_proxy_lcopy_value (const GValue *value,
			 guint         nth_value,
			 GType        *collect_type,
			 GTypeCValue  *collect_value)
{
  BoxedNode key, *node;
  gpointer *boxed_p = collect_value->v_pointer;

  if (!boxed_p)
    return g_strdup_printf ("value location for `%s' passed as NULL", G_VALUE_TYPE_NAME (value));

  key.type = value->g_type;
  node = g_bsearch_array_lookup (&boxed_bsa, &key);
  *boxed_p = node->copy (value->data[0].v_pointer);

  *collect_type = 0;
  return NULL;
}

GType
g_boxed_type_register_static (const gchar   *name,
			      GBoxedCopyFunc boxed_copy,
			      GBoxedFreeFunc boxed_free)
{
  static const GTypeValueTable vtable = {
    boxed_proxy_value_init,
    boxed_proxy_value_free,
    boxed_proxy_value_copy,
    boxed_proxy_value_peek_pointer,
    G_VALUE_COLLECT_POINTER,
    boxed_proxy_collect_value,
    G_VALUE_COLLECT_POINTER,
    boxed_proxy_lcopy_value,
  };
  static const GTypeInfo type_info = {
    0,			/* class_size */
    NULL,		/* base_init */
    NULL,		/* base_finalize */
    NULL,		/* class_init */
    NULL,		/* class_finalize */
    NULL,		/* class_data */
    0,			/* instance_size */
    0,			/* n_preallocs */
    NULL,		/* instance_init */
    &vtable,		/* value_table */
  };
  GType type;

  g_return_val_if_fail (name != NULL, 0);
  g_return_val_if_fail (boxed_copy != NULL, 0);
  g_return_val_if_fail (boxed_free != NULL, 0);
  g_return_val_if_fail (g_type_from_name (name) == 0, 0);

  type = g_type_register_static (G_TYPE_BOXED, name, &type_info, 0);

  /* install proxy functions upon successfull registration */
  if (type)
    {
      BoxedNode key;

      key.type = type;
      key.copy = boxed_copy;
      key.free = boxed_free;
      g_bsearch_array_insert (&boxed_bsa, &key, TRUE);
    }

  return type;
}

GBoxed*
g_boxed_copy (GType         boxed_type,
	      gconstpointer src_boxed)
{
  GTypeValueTable *value_table;
  gpointer dest_boxed;

  g_return_val_if_fail (G_TYPE_IS_BOXED (boxed_type), NULL);
  g_return_val_if_fail (G_TYPE_IS_ABSTRACT (boxed_type) == FALSE, NULL);
  g_return_val_if_fail (src_boxed != NULL, NULL);

  value_table = g_type_value_table_peek (boxed_type);
  if (!value_table)
    g_return_val_if_fail (G_TYPE_IS_VALUE_TYPE (boxed_type), NULL);

  /* check if our proxying implementation is used, we can short-cut here */
  if (value_table->value_copy == boxed_proxy_value_copy)
    {
      BoxedNode key, *node;

      key.type = boxed_type;
      node = g_bsearch_array_lookup (&boxed_bsa, &key);
      dest_boxed = node->copy ((gpointer) src_boxed);
    }
  else
    {
      GValue src_value, dest_value;
      
      /* we heavil rely on the gvalue.c implementation here */

      memset (&src_value.data, 0, sizeof (src_value.data));
      memset (&dest_value.data, 0, sizeof (dest_value.data));
      dest_value.g_type = boxed_type;
      src_value.g_type = boxed_type;
      src_value.data[0].v_pointer = (gpointer) src_boxed;
      value_table->value_copy (&src_value, &dest_value);
      if (dest_value.data[1].v_ulong ||
	  dest_value.data[2].v_ulong ||
	  dest_value.data[3].v_ulong)
	g_warning ("the copy_value() implementation of type `%s' seems to make use of reserved GValue fields",
		   g_type_name (boxed_type));

      dest_boxed = dest_value.data[0].v_pointer;
    }

  return dest_boxed;
}

void
g_boxed_free (GType    boxed_type,
	      gpointer boxed)
{
  GTypeValueTable *value_table;

  g_return_if_fail (G_TYPE_IS_BOXED (boxed_type));
  g_return_if_fail (G_TYPE_IS_ABSTRACT (boxed_type) == FALSE);
  g_return_if_fail (boxed != NULL);

  value_table = g_type_value_table_peek (boxed_type);
  if (!value_table)
    g_return_if_fail (G_TYPE_IS_VALUE_TYPE (boxed_type));

  /* check if our proxying implementation is used, we can short-cut here */
  if (value_table->value_free == boxed_proxy_value_free)
    {
      BoxedNode key, *node;

      key.type = boxed_type;
      node = g_bsearch_array_lookup (&boxed_bsa, &key);
      node->free (boxed);
    }
  else
    {
      GValue value;

      /* we heavil rely on the gvalue.c implementation here */
      memset (&value.data, 0, sizeof (value.data));
      value.g_type = boxed_type;
      value.data[0].v_pointer = boxed;
      value_table->value_free (&value);
    }
}

void
g_value_set_boxed (GValue       *value,
		   gconstpointer boxed)
{
  g_return_if_fail (G_IS_VALUE_BOXED (value));
  g_return_if_fail (G_TYPE_IS_VALUE (G_VALUE_TYPE (value)));

  if (value->data[0].v_pointer && !(value->data[1].v_uint & G_VALUE_STATIC_TAG))
    g_boxed_free (G_VALUE_TYPE (value), value->data[0].v_pointer);
  value->data[0].v_pointer = boxed ? g_boxed_copy (G_VALUE_TYPE (value), boxed) : NULL;
  value->data[1].v_uint = 0;
}

void
g_value_set_static_boxed (GValue       *value,
			  gconstpointer boxed)
{
  g_return_if_fail (G_IS_VALUE_BOXED (value));
  g_return_if_fail (G_TYPE_IS_VALUE (G_VALUE_TYPE (value)));

  if (value->data[0].v_pointer && !(value->data[1].v_uint & G_VALUE_STATIC_TAG))
    g_boxed_free (G_VALUE_TYPE (value), value->data[0].v_pointer);
  value->data[0].v_pointer = (gpointer) boxed;
  value->data[1].v_uint = boxed ? G_VALUE_STATIC_TAG : 0;
}

gpointer
g_value_get_boxed (const GValue *value)
{
  g_return_val_if_fail (G_IS_VALUE_BOXED (value), NULL);
  g_return_val_if_fail (G_TYPE_IS_VALUE (G_VALUE_TYPE (value)), NULL);

  return value->data[0].v_pointer;
}

gpointer
g_value_dup_boxed (GValue *value)
{
  g_return_val_if_fail (G_IS_VALUE_BOXED (value), NULL);
  g_return_val_if_fail (G_TYPE_IS_VALUE (G_VALUE_TYPE (value)), NULL);

  return value->data[0].v_pointer ? g_boxed_copy (G_VALUE_TYPE (value), value->data[0].v_pointer) : NULL;
}
