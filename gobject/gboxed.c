/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 2000-2001 Red Hat, Inc.
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
#include	"gvaluearray.h"
#include	"gclosure.h"
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
static GBSearchArray *boxed_bsa = NULL;
static const GBSearchConfig boxed_bconfig = {
  sizeof (BoxedNode),
  boxed_nodes_cmp,
  0,
};


/* --- functions --- */
static gint
boxed_nodes_cmp	(gconstpointer p1,
		 gconstpointer p2)
{
  const BoxedNode *node1 = p1, *node2 = p2;

  return G_BSEARCH_ARRAY_CMP (node1->type, node2->type);
}

static inline void              /* keep this function in sync with gvalue.c */
value_meminit (GValue *value,
	       GType   value_type)
{
  value->g_type = value_type;
  memset (value->data, 0, sizeof (value->data));
}

static gpointer
value_copy (gpointer boxed)
{
  const GValue *src_value = boxed;
  GValue *dest_value = g_new0 (GValue, 1);

  if (G_VALUE_TYPE (src_value))
    {
      g_value_init (dest_value, G_VALUE_TYPE (src_value));
      g_value_copy (src_value, dest_value);
    }
  return dest_value;
}

static void
value_free (gpointer boxed)
{
  GValue *value = boxed;

  if (G_VALUE_TYPE (value))
    g_value_unset (value);
  g_free (value);
}

static gpointer
gstring_copy (gpointer boxed)
{
  const GString *src_gstring = boxed;

  return g_string_new_len (src_gstring->str, src_gstring->len);
}

static void
gstring_free (gpointer boxed)
{
  GString *gstring = boxed;

  g_string_free (gstring, TRUE);
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

  boxed_bsa = g_bsearch_array_create (&boxed_bconfig);

  /* G_TYPE_BOXED
   */
  type = g_type_register_fundamental (G_TYPE_BOXED, "GBoxed", &info, &finfo,
				      G_TYPE_FLAG_ABSTRACT | G_TYPE_FLAG_VALUE_ABSTRACT);
  g_assert (type == G_TYPE_BOXED);
}

GType
g_closure_get_type (void)
{
  static GType type_id = 0;

  if (!type_id)
    type_id = g_boxed_type_register_static ("GClosure",
					    (GBoxedCopyFunc) g_closure_ref,
					    (GBoxedFreeFunc) g_closure_unref);
  return type_id;
}

GType
g_value_get_type (void)
{
  static GType type_id = 0;

  if (!type_id)
    type_id = g_boxed_type_register_static ("GValue",
					    value_copy,
					    value_free);
  return type_id;
}

GType
g_value_array_get_type (void)
{
  static GType type_id = 0;

  if (!type_id)
    type_id = g_boxed_type_register_static ("GValueArray",
					    (GBoxedCopyFunc) g_value_array_copy,
					    (GBoxedFreeFunc) g_value_array_free);
  return type_id;
}

GType
g_strv_get_type (void)
{
  static GType type_id = 0;

  if (!type_id)
    type_id = g_boxed_type_register_static ("GStrv",
					    (GBoxedCopyFunc) g_strdupv,
					    (GBoxedFreeFunc) g_strfreev);
  return type_id;
}

GType
g_gstring_get_type (void)
{
  static GType type_id = 0;

  if (!type_id)
    type_id = g_boxed_type_register_static ("GString",	/* the naming is a bit odd, but GString is obviously not G_TYPE_STRING */
					    gstring_copy,
					    gstring_free);
  return type_id;
}

static void
boxed_proxy_value_init (GValue *value)
{
  value->data[0].v_pointer = NULL;
}

static void
boxed_proxy_value_free (GValue *value)
{
  if (value->data[0].v_pointer && !(value->data[1].v_uint & G_VALUE_NOCOPY_CONTENTS))
    {
      BoxedNode key, *node;

      key.type = G_VALUE_TYPE (value);
      node = g_bsearch_array_lookup (boxed_bsa, &boxed_bconfig, &key);
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

      key.type = G_VALUE_TYPE (src_value);
      node = g_bsearch_array_lookup (boxed_bsa, &boxed_bconfig, &key);
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
			   guint        n_collect_values,
			   GTypeCValue *collect_values,
			   guint        collect_flags)
{
  BoxedNode key, *node;

  key.type = G_VALUE_TYPE (value);
  node = g_bsearch_array_lookup (boxed_bsa, &boxed_bconfig, &key);

  if (!collect_values[0].v_pointer)
    value->data[0].v_pointer = NULL;
  else
    {
      if (collect_flags & G_VALUE_NOCOPY_CONTENTS)
	{
	  value->data[0].v_pointer = collect_values[0].v_pointer;
	  value->data[1].v_uint = G_VALUE_NOCOPY_CONTENTS;
	}
      else
	value->data[0].v_pointer = node->copy (collect_values[0].v_pointer);
    }

  return NULL;
}

static gchar*
boxed_proxy_lcopy_value (const GValue *value,
			 guint         n_collect_values,
			 GTypeCValue  *collect_values,
			 guint         collect_flags)
{
  gpointer *boxed_p = collect_values[0].v_pointer;

  if (!boxed_p)
    return g_strdup_printf ("value location for `%s' passed as NULL", G_VALUE_TYPE_NAME (value));

  if (!value->data[0].v_pointer)
    *boxed_p = NULL;
  else if (collect_flags & G_VALUE_NOCOPY_CONTENTS)
    *boxed_p = value->data[0].v_pointer;
  else
    {
      BoxedNode key, *node;

      key.type = G_VALUE_TYPE (value);
      node = g_bsearch_array_lookup (boxed_bsa, &boxed_bconfig, &key);
      *boxed_p = node->copy (value->data[0].v_pointer);
    }

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
    "p",
    boxed_proxy_collect_value,
    "p",
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
      boxed_bsa = g_bsearch_array_insert (boxed_bsa, &boxed_bconfig, &key);
    }

  return type;
}

gpointer
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
      node = g_bsearch_array_lookup (boxed_bsa, &boxed_bconfig, &key);
      dest_boxed = node->copy ((gpointer) src_boxed);
    }
  else
    {
      GValue src_value, dest_value;
      
      /* we heavily rely on third-party boxed type value vtable
       * implementations to follow normal boxed value storage
       * (data[0].v_pointer is the boxed struct, and
       * data[1].v_uint holds the G_VALUE_NOCOPY_CONTENTS flag,
       * rest zero).
       * but then, we can expect that since we layed out the
       * g_boxed_*() API.
       * data[1].v_uint&G_VALUE_NOCOPY_CONTENTS shouldn't be set
       * after a copy.
       */
      /* equiv. to g_value_set_static_boxed() */
      value_meminit (&src_value, boxed_type);
      src_value.data[0].v_pointer = (gpointer) src_boxed;
      src_value.data[1].v_uint = G_VALUE_NOCOPY_CONTENTS;

      /* call third-party code copy function, fingers-crossed */
      value_meminit (&dest_value, boxed_type);
      value_table->value_copy (&src_value, &dest_value);

      /* double check and grouse if things went wrong */
      if (dest_value.data[1].v_ulong)
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
      node = g_bsearch_array_lookup (boxed_bsa, &boxed_bconfig, &key);
      node->free (boxed);
    }
  else
    {
      GValue value;

      /* see g_boxed_copy() on why we think we can do this */
      value_meminit (&value, boxed_type);
      value.data[0].v_pointer = boxed;
      value_table->value_free (&value);
    }
}

gpointer
g_value_get_boxed (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_BOXED (value), NULL);
  g_return_val_if_fail (G_TYPE_IS_VALUE (G_VALUE_TYPE (value)), NULL);

  return value->data[0].v_pointer;
}

gpointer
g_value_dup_boxed (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_BOXED (value), NULL);
  g_return_val_if_fail (G_TYPE_IS_VALUE (G_VALUE_TYPE (value)), NULL);

  return value->data[0].v_pointer ? g_boxed_copy (G_VALUE_TYPE (value), value->data[0].v_pointer) : NULL;
}

static inline void
value_set_boxed_internal (GValue       *value,
			  gconstpointer const_boxed,
			  gboolean      need_copy,
			  gboolean      need_free)
{
  BoxedNode key, *node;
  gpointer boxed = (gpointer) const_boxed;

  if (!boxed)
    {
      /* just resetting to NULL might not be desired, need to
       * have value reinitialized also (for values defaulting
       * to other default value states than a NULL data pointer),
       * g_value_reset() will handle this
       */
      g_value_reset (value);
      return;
    }

  key.type = G_VALUE_TYPE (value);
  node = g_bsearch_array_lookup (boxed_bsa, &boxed_bconfig, &key);

  if (node)
    {
      /* we proxy this type, free contents and copy right away */
      if (value->data[0].v_pointer && !(value->data[1].v_uint & G_VALUE_NOCOPY_CONTENTS))
	node->free (value->data[0].v_pointer);
      value->data[1].v_uint = need_free ? 0 : G_VALUE_NOCOPY_CONTENTS;
      value->data[0].v_pointer = need_copy ? node->copy (boxed) : boxed;
    }
  else
    {
      /* we don't handle this type, free contents and let g_boxed_copy()
       * figure what's required
       */
      if (value->data[0].v_pointer && !(value->data[1].v_uint & G_VALUE_NOCOPY_CONTENTS))
	g_boxed_free (G_VALUE_TYPE (value), value->data[0].v_pointer);
      value->data[1].v_uint = need_free ? 0 : G_VALUE_NOCOPY_CONTENTS;
      value->data[0].v_pointer = need_copy ? g_boxed_copy (G_VALUE_TYPE (value), boxed) : boxed;
    }
}

void
g_value_set_boxed (GValue       *value,
		   gconstpointer boxed)
{
  g_return_if_fail (G_VALUE_HOLDS_BOXED (value));
  g_return_if_fail (G_TYPE_IS_VALUE (G_VALUE_TYPE (value)));

  value_set_boxed_internal (value, boxed, TRUE, TRUE);
}

void
g_value_set_static_boxed (GValue       *value,
			  gconstpointer boxed)
{
  g_return_if_fail (G_VALUE_HOLDS_BOXED (value));
  g_return_if_fail (G_TYPE_IS_VALUE (G_VALUE_TYPE (value)));

  value_set_boxed_internal (value, boxed, FALSE, FALSE);
}

void
g_value_set_boxed_take_ownership (GValue       *value,
				  gconstpointer boxed)
{
  g_value_take_boxed (value, boxed);
}

void
g_value_take_boxed (GValue       *value,
		    gconstpointer boxed)
{
  g_return_if_fail (G_VALUE_HOLDS_BOXED (value));
  g_return_if_fail (G_TYPE_IS_VALUE (G_VALUE_TYPE (value)));

  value_set_boxed_internal (value, boxed, FALSE, TRUE);
}
