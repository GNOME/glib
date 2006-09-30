/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 1997-1999, 2000-2001 Tim Janik and Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * MT safe
 */

#include	"gvaluetypes.h"

#include	"gvaluecollector.h"

#include "gobject.h"
#include "gparam.h"
#include "gboxed.h"
#include "genums.h"

#include	"gobjectalias.h"
#include	<string.h>
#include	<stdlib.h>	/* qsort() */


/* --- value functions --- */
static void
value_init_long0 (GValue *value)
{
  value->data[0].v_long = 0;
}

static void
value_copy_long0 (const GValue *src_value,
		  GValue       *dest_value)
{
  dest_value->data[0].v_long = src_value->data[0].v_long;
}

static gchar*
value_lcopy_char (const GValue *value,
		  guint         n_collect_values,
		  GTypeCValue  *collect_values,
		  guint         collect_flags)
{
  gint8 *int8_p = collect_values[0].v_pointer;
  
  if (!int8_p)
    return g_strdup_printf ("value location for `%s' passed as NULL", G_VALUE_TYPE_NAME (value));
  
  *int8_p = value->data[0].v_int;
  
  return NULL;
}

static gchar*
value_lcopy_boolean (const GValue *value,
		     guint         n_collect_values,
		     GTypeCValue  *collect_values,
		     guint         collect_flags)
{
  gboolean *bool_p = collect_values[0].v_pointer;
  
  if (!bool_p)
    return g_strdup_printf ("value location for `%s' passed as NULL", G_VALUE_TYPE_NAME (value));
  
  *bool_p = value->data[0].v_int;
  
  return NULL;
}

static gchar*
value_collect_int (GValue      *value,
		   guint        n_collect_values,
		   GTypeCValue *collect_values,
		   guint        collect_flags)
{
  value->data[0].v_int = collect_values[0].v_int;
  
  return NULL;
}

static gchar*
value_lcopy_int (const GValue *value,
		 guint         n_collect_values,
		 GTypeCValue  *collect_values,
		 guint         collect_flags)
{
  gint *int_p = collect_values[0].v_pointer;
  
  if (!int_p)
    return g_strdup_printf ("value location for `%s' passed as NULL", G_VALUE_TYPE_NAME (value));
  
  *int_p = value->data[0].v_int;
  
  return NULL;
}

static gchar*
value_collect_long (GValue      *value,
		    guint        n_collect_values,
		    GTypeCValue *collect_values,
		    guint        collect_flags)
{
  value->data[0].v_long = collect_values[0].v_long;
  
  return NULL;
}

static gchar*
value_lcopy_long (const GValue *value,
		  guint         n_collect_values,
		  GTypeCValue  *collect_values,
		  guint         collect_flags)
{
  glong *long_p = collect_values[0].v_pointer;
  
  if (!long_p)
    return g_strdup_printf ("value location for `%s' passed as NULL", G_VALUE_TYPE_NAME (value));
  
  *long_p = value->data[0].v_long;
  
  return NULL;
}

static void
value_init_int64 (GValue *value)
{
  value->data[0].v_int64 = 0;
}

static void
value_copy_int64 (const GValue *src_value,
		  GValue       *dest_value)
{
  dest_value->data[0].v_int64 = src_value->data[0].v_int64;
}

static gchar*
value_collect_int64 (GValue      *value,
		     guint        n_collect_values,
		     GTypeCValue *collect_values,
		     guint        collect_flags)
{
  value->data[0].v_int64 = collect_values[0].v_int64;
  
  return NULL;
}

static gchar*
value_lcopy_int64 (const GValue *value,
		   guint         n_collect_values,
		   GTypeCValue  *collect_values,
		   guint         collect_flags)
{
  gint64 *int64_p = collect_values[0].v_pointer;
  
  if (!int64_p)
    return g_strdup_printf ("value location for `%s' passed as NULL", G_VALUE_TYPE_NAME (value));
  
  *int64_p = value->data[0].v_int64;
  
  return NULL;
}

static void
value_init_float (GValue *value)
{
  value->data[0].v_float = 0.0;
}

static void
value_copy_float (const GValue *src_value,
		  GValue       *dest_value)
{
  dest_value->data[0].v_float = src_value->data[0].v_float;
}

static gchar*
value_collect_float (GValue      *value,
		     guint        n_collect_values,
		     GTypeCValue *collect_values,
		     guint        collect_flags)
{
  value->data[0].v_float = collect_values[0].v_double;
  
  return NULL;
}

static gchar*
value_lcopy_float (const GValue *value,
		   guint         n_collect_values,
		   GTypeCValue  *collect_values,
		   guint         collect_flags)
{
  gfloat *float_p = collect_values[0].v_pointer;
  
  if (!float_p)
    return g_strdup_printf ("value location for `%s' passed as NULL", G_VALUE_TYPE_NAME (value));
  
  *float_p = value->data[0].v_float;
  
  return NULL;
}

static void
value_init_double (GValue *value)
{
  value->data[0].v_double = 0.0;
}

static void
value_copy_double (const GValue *src_value,
		   GValue	*dest_value)
{
  dest_value->data[0].v_double = src_value->data[0].v_double;
}

static gchar*
value_collect_double (GValue	  *value,
		      guint        n_collect_values,
		      GTypeCValue *collect_values,
		      guint        collect_flags)
{
  value->data[0].v_double = collect_values[0].v_double;
  
  return NULL;
}

static gchar*
value_lcopy_double (const GValue *value,
		    guint         n_collect_values,
		    GTypeCValue  *collect_values,
		    guint         collect_flags)
{
  gdouble *double_p = collect_values[0].v_pointer;
  
  if (!double_p)
    return g_strdup_printf ("value location for `%s' passed as NULL", G_VALUE_TYPE_NAME (value));
  
  *double_p = value->data[0].v_double;
  
  return NULL;
}

static void
value_init_string (GValue *value)
{
  value->data[0].v_pointer = NULL;
}

static void
value_free_string (GValue *value)
{
  if (!(value->data[1].v_uint & G_VALUE_NOCOPY_CONTENTS))
    g_free (value->data[0].v_pointer);
}

static void
value_copy_string (const GValue *src_value,
		   GValue	*dest_value)
{
  dest_value->data[0].v_pointer = g_strdup (src_value->data[0].v_pointer);
}

static gchar*
value_collect_string (GValue	  *value,
		      guint        n_collect_values,
		      GTypeCValue *collect_values,
		      guint        collect_flags)
{
  if (!collect_values[0].v_pointer)
    value->data[0].v_pointer = NULL;
  else if (collect_flags & G_VALUE_NOCOPY_CONTENTS)
    {
      value->data[0].v_pointer = collect_values[0].v_pointer;
      value->data[1].v_uint = G_VALUE_NOCOPY_CONTENTS;
    }
  else
    value->data[0].v_pointer = g_strdup (collect_values[0].v_pointer);
  
  return NULL;
}

static gchar*
value_lcopy_string (const GValue *value,
		    guint         n_collect_values,
		    GTypeCValue  *collect_values,
		    guint         collect_flags)
{
  gchar **string_p = collect_values[0].v_pointer;
  
  if (!string_p)
    return g_strdup_printf ("value location for `%s' passed as NULL", G_VALUE_TYPE_NAME (value));
  
  if (!value->data[0].v_pointer)
    *string_p = NULL;
  else if (collect_flags & G_VALUE_NOCOPY_CONTENTS)
    *string_p = value->data[0].v_pointer;
  else
    *string_p = g_strdup (value->data[0].v_pointer);
  
  return NULL;
}

static void
value_init_pointer (GValue *value)
{
  value->data[0].v_pointer = NULL;
}

static void
value_copy_pointer (const GValue *src_value,
		    GValue       *dest_value)
{
  dest_value->data[0].v_pointer = src_value->data[0].v_pointer;
}

static gpointer
value_peek_pointer0 (const GValue *value)
{
  return value->data[0].v_pointer;
}

static gchar*
value_collect_pointer (GValue      *value,
		       guint        n_collect_values,
		       GTypeCValue *collect_values,
		       guint        collect_flags)
{
  value->data[0].v_pointer = collect_values[0].v_pointer;

  return NULL;
}

static gchar*
value_lcopy_pointer (const GValue *value,
		     guint         n_collect_values,
		     GTypeCValue  *collect_values,
		     guint         collect_flags)
{
  gpointer *pointer_p = collect_values[0].v_pointer;

  if (!pointer_p)
    return g_strdup_printf ("value location for `%s' passed as NULL", G_VALUE_TYPE_NAME (value));

  *pointer_p = value->data[0].v_pointer;

  return NULL;
}


/* --- type initialization --- */
void
g_value_types_init (void)
{
  GTypeInfo info = {
    0,				/* class_size */
    NULL,			/* base_init */
    NULL,			/* base_destroy */
    NULL,			/* class_init */
    NULL,			/* class_destroy */
    NULL,			/* class_data */
    0,				/* instance_size */
    0,				/* n_preallocs */
    NULL,			/* instance_init */
    NULL,			/* value_table */
  };
  const GTypeFundamentalInfo finfo = { G_TYPE_FLAG_DERIVABLE, };
  GType type;
  
  /* G_TYPE_CHAR / G_TYPE_UCHAR
   */
  {
    static const GTypeValueTable value_table = {
      value_init_long0,		/* value_init */
      NULL,			/* value_free */
      value_copy_long0,		/* value_copy */
      NULL,			/* value_peek_pointer */
      "i",			/* collect_format */
      value_collect_int,	/* collect_value */
      "p",			/* lcopy_format */
      value_lcopy_char,		/* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_CHAR, g_intern_static_string ("gchar"), &info, &finfo, 0);
    g_assert (type == G_TYPE_CHAR);
    type = g_type_register_fundamental (G_TYPE_UCHAR, g_intern_static_string ("guchar"), &info, &finfo, 0);
    g_assert (type == G_TYPE_UCHAR);
  }

  /* G_TYPE_BOOLEAN
   */
  {
    static const GTypeValueTable value_table = {
      value_init_long0,		 /* value_init */
      NULL,			 /* value_free */
      value_copy_long0,		 /* value_copy */
      NULL,                      /* value_peek_pointer */
      "i",			 /* collect_format */
      value_collect_int,	 /* collect_value */
      "p",			 /* lcopy_format */
      value_lcopy_boolean,	 /* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_BOOLEAN, g_intern_static_string ("gboolean"), &info, &finfo, 0);
    g_assert (type == G_TYPE_BOOLEAN);
  }
  
  /* G_TYPE_INT / G_TYPE_UINT
   */
  {
    static const GTypeValueTable value_table = {
      value_init_long0,		/* value_init */
      NULL,			/* value_free */
      value_copy_long0,		/* value_copy */
      NULL,                     /* value_peek_pointer */
      "i",			/* collect_format */
      value_collect_int,	/* collect_value */
      "p",			/* lcopy_format */
      value_lcopy_int,		/* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_INT, g_intern_static_string ("gint"), &info, &finfo, 0);
    g_assert (type == G_TYPE_INT);
    type = g_type_register_fundamental (G_TYPE_UINT, g_intern_static_string ("guint"), &info, &finfo, 0);
    g_assert (type == G_TYPE_UINT);
  }

  /* G_TYPE_LONG / G_TYPE_ULONG
   */
  {
    static const GTypeValueTable value_table = {
      value_init_long0,		/* value_init */
      NULL,			/* value_free */
      value_copy_long0,		/* value_copy */
      NULL,                     /* value_peek_pointer */
      "l",			/* collect_format */
      value_collect_long,	/* collect_value */
      "p",			/* lcopy_format */
      value_lcopy_long,		/* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_LONG, g_intern_static_string ("glong"), &info, &finfo, 0);
    g_assert (type == G_TYPE_LONG);
    type = g_type_register_fundamental (G_TYPE_ULONG, g_intern_static_string ("gulong"), &info, &finfo, 0);
    g_assert (type == G_TYPE_ULONG);
  }
  
  /* G_TYPE_INT64 / G_TYPE_UINT64
   */
  {
    static const GTypeValueTable value_table = {
      value_init_int64,		/* value_init */
      NULL,			/* value_free */
      value_copy_int64,		/* value_copy */
      NULL,                     /* value_peek_pointer */
      "q",			/* collect_format */
      value_collect_int64,	/* collect_value */
      "p",			/* lcopy_format */
      value_lcopy_int64,	/* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_INT64, g_intern_static_string ("gint64"), &info, &finfo, 0);
    g_assert (type == G_TYPE_INT64);
    type = g_type_register_fundamental (G_TYPE_UINT64, g_intern_static_string ("guint64"), &info, &finfo, 0);
    g_assert (type == G_TYPE_UINT64);
  }
  
  /* G_TYPE_FLOAT
   */
  {
    static const GTypeValueTable value_table = {
      value_init_float,		 /* value_init */
      NULL,			 /* value_free */
      value_copy_float,		 /* value_copy */
      NULL,                      /* value_peek_pointer */
      "d",			 /* collect_format */
      value_collect_float,	 /* collect_value */
      "p",			 /* lcopy_format */
      value_lcopy_float,	 /* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_FLOAT, g_intern_static_string ("gfloat"), &info, &finfo, 0);
    g_assert (type == G_TYPE_FLOAT);
  }
  
  /* G_TYPE_DOUBLE
   */
  {
    static const GTypeValueTable value_table = {
      value_init_double,	/* value_init */
      NULL,			/* value_free */
      value_copy_double,	/* value_copy */
      NULL,                     /* value_peek_pointer */
      "d",			/* collect_format */
      value_collect_double,	/* collect_value */
      "p",			/* lcopy_format */
      value_lcopy_double,	/* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_DOUBLE, g_intern_static_string ("gdouble"), &info, &finfo, 0);
    g_assert (type == G_TYPE_DOUBLE);
  }

  /* G_TYPE_STRING
   */
  {
    static const GTypeValueTable value_table = {
      value_init_string,	/* value_init */
      value_free_string,	/* value_free */
      value_copy_string,	/* value_copy */
      value_peek_pointer0,	/* value_peek_pointer */
      "p",			/* collect_format */
      value_collect_string,	/* collect_value */
      "p",			/* lcopy_format */
      value_lcopy_string,	/* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_STRING, g_intern_static_string ("gchararray"), &info, &finfo, 0);
    g_assert (type == G_TYPE_STRING);
  }

  /* G_TYPE_POINTER
   */
  {
    static const GTypeValueTable value_table = {
      value_init_pointer,	/* value_init */
      NULL,			/* value_free */
      value_copy_pointer,	/* value_copy */
      value_peek_pointer0,	/* value_peek_pointer */
      "p",			/* collect_format */
      value_collect_pointer,	/* collect_value */
      "p",			/* lcopy_format */
      value_lcopy_pointer,	/* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_POINTER, g_intern_static_string ("gpointer"), &info, &finfo, 0);
    g_assert (type == G_TYPE_POINTER);
  }
}


/* --- GValue functions --- */
void
g_value_set_char (GValue *value,
		  gchar	  v_char)
{
  g_return_if_fail (G_VALUE_HOLDS_CHAR (value));
  
  value->data[0].v_int = v_char;
}

gchar
g_value_get_char (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_CHAR (value), 0);
  
  return value->data[0].v_int;
}

void
g_value_set_uchar (GValue *value,
		   guchar  v_uchar)
{
  g_return_if_fail (G_VALUE_HOLDS_UCHAR (value));
  
  value->data[0].v_uint = v_uchar;
}

guchar
g_value_get_uchar (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_UCHAR (value), 0);
  
  return value->data[0].v_uint;
}

void
g_value_set_boolean (GValue  *value,
		     gboolean v_boolean)
{
  g_return_if_fail (G_VALUE_HOLDS_BOOLEAN (value));
  
  value->data[0].v_int = v_boolean != FALSE;
}

gboolean
g_value_get_boolean (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_BOOLEAN (value), 0);
  
  return value->data[0].v_int;
}

void
g_value_set_int (GValue *value,
		 gint	 v_int)
{
  g_return_if_fail (G_VALUE_HOLDS_INT (value));
  
  value->data[0].v_int = v_int;
}

gint
g_value_get_int (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_INT (value), 0);
  
  return value->data[0].v_int;
}

void
g_value_set_uint (GValue *value,
		  guint	  v_uint)
{
  g_return_if_fail (G_VALUE_HOLDS_UINT (value));
  
  value->data[0].v_uint = v_uint;
}

guint
g_value_get_uint (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_UINT (value), 0);
  
  return value->data[0].v_uint;
}

void
g_value_set_long (GValue *value,
		  glong	  v_long)
{
  g_return_if_fail (G_VALUE_HOLDS_LONG (value));
  
  value->data[0].v_long = v_long;
}

glong
g_value_get_long (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_LONG (value), 0);
  
  return value->data[0].v_long;
}

void
g_value_set_ulong (GValue *value,
		   gulong  v_ulong)
{
  g_return_if_fail (G_VALUE_HOLDS_ULONG (value));
  
  value->data[0].v_ulong = v_ulong;
}

gulong
g_value_get_ulong (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_ULONG (value), 0);
  
  return value->data[0].v_ulong;
}

void
g_value_set_int64 (GValue *value,
		   gint64  v_int64)
{
  g_return_if_fail (G_VALUE_HOLDS_INT64 (value));
  
  value->data[0].v_int64 = v_int64;
}

gint64
g_value_get_int64 (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_INT64 (value), 0);
  
  return value->data[0].v_int64;
}

void
g_value_set_uint64 (GValue *value,
		    guint64 v_uint64)
{
  g_return_if_fail (G_VALUE_HOLDS_UINT64 (value));
  
  value->data[0].v_uint64 = v_uint64;
}

guint64
g_value_get_uint64 (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_UINT64 (value), 0);
  
  return value->data[0].v_uint64;
}

void
g_value_set_float (GValue *value,
		   gfloat  v_float)
{
  g_return_if_fail (G_VALUE_HOLDS_FLOAT (value));
  
  value->data[0].v_float = v_float;
}

gfloat
g_value_get_float (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_FLOAT (value), 0);
  
  return value->data[0].v_float;
}

void
g_value_set_double (GValue *value,
		    gdouble v_double)
{
  g_return_if_fail (G_VALUE_HOLDS_DOUBLE (value));
  
  value->data[0].v_double = v_double;
}

gdouble
g_value_get_double (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_DOUBLE (value), 0);
  
  return value->data[0].v_double;
}

void
g_value_set_string (GValue	*value,
		    const gchar *v_string)
{
  gchar *new_val;

  g_return_if_fail (G_VALUE_HOLDS_STRING (value));

  new_val = g_strdup (v_string);

  if (value->data[1].v_uint & G_VALUE_NOCOPY_CONTENTS)
    value->data[1].v_uint = 0;
  else
    g_free (value->data[0].v_pointer);

  value->data[0].v_pointer = new_val;
}

void
g_value_set_static_string (GValue      *value,
			   const gchar *v_string)
{
  g_return_if_fail (G_VALUE_HOLDS_STRING (value));
  
  if (!(value->data[1].v_uint & G_VALUE_NOCOPY_CONTENTS))
    g_free (value->data[0].v_pointer);
  value->data[1].v_uint = G_VALUE_NOCOPY_CONTENTS;
  value->data[0].v_pointer = (gchar*) v_string;
}

void
g_value_set_string_take_ownership (GValue *value,
				   gchar  *v_string)
{
  g_value_take_string (value, v_string);
}

void
g_value_take_string (GValue *value,
		     gchar  *v_string)
{
  g_return_if_fail (G_VALUE_HOLDS_STRING (value));
  
  if (value->data[1].v_uint & G_VALUE_NOCOPY_CONTENTS)
    value->data[1].v_uint = 0;
  else
    g_free (value->data[0].v_pointer);
  value->data[0].v_pointer = v_string;
}

G_CONST_RETURN gchar*
g_value_get_string (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_STRING (value), NULL);
  
  return value->data[0].v_pointer;
}

gchar*
g_value_dup_string (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_STRING (value), NULL);
  
  return g_strdup (value->data[0].v_pointer);
}

void
g_value_set_pointer (GValue  *value,
		     gpointer v_pointer)
{
  g_return_if_fail (G_VALUE_HOLDS_POINTER (value));

  value->data[0].v_pointer = v_pointer;
}

gpointer
g_value_get_pointer (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_POINTER (value), NULL);

  return value->data[0].v_pointer;
}

GType
g_gtype_get_type (void)
{
  static const GTypeInfo type_info = { 0, };
  static GType type;
  if (!type)
    type = g_type_register_static (G_TYPE_POINTER, g_intern_static_string ("GType"), &type_info, 0);
  return type;
}

void 
g_value_set_gtype (GValue *value,
		   GType   v_gtype)
{
  g_return_if_fail (G_VALUE_HOLDS_GTYPE (value));

  value->data[0].v_long = v_gtype;
  
}

GType	              
g_value_get_gtype (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_GTYPE (value), 0);

  return value->data[0].v_long;
}


gchar*
g_strdup_value_contents (const GValue *value)
{
  const gchar *src;
  gchar *contents;

  g_return_val_if_fail (G_IS_VALUE (value), NULL);
  
  if (G_VALUE_HOLDS_STRING (value))
    {
      src = g_value_get_string (value);
      
      if (!src)
	contents = g_strdup ("NULL");
      else
	{
	  gchar *s = g_strescape (src, NULL);

	  contents = g_strdup_printf ("\"%s\"", s);
	  g_free (s);
	}
    }
  else if (g_value_type_transformable (G_VALUE_TYPE (value), G_TYPE_STRING))
    {
      GValue tmp_value = { 0, };
      gchar *s;

      g_value_init (&tmp_value, G_TYPE_STRING);
      g_value_transform (value, &tmp_value);
      s = g_strescape (g_value_get_string (&tmp_value), NULL);
      g_value_unset (&tmp_value);
      if (G_VALUE_HOLDS_ENUM (value) || G_VALUE_HOLDS_FLAGS (value))
	contents = g_strdup_printf ("((%s) %s)",
				    g_type_name (G_VALUE_TYPE (value)),
				    s);
      else
	contents = g_strdup (s ? s : "NULL");
      g_free (s);
    }
  else if (g_value_fits_pointer (value))
    {
      gpointer p = g_value_peek_pointer (value);

      if (!p)
	contents = g_strdup ("NULL");
      else if (G_VALUE_HOLDS_OBJECT (value))
	contents = g_strdup_printf ("((%s*) %p)", G_OBJECT_TYPE_NAME (p), p);
      else if (G_VALUE_HOLDS_PARAM (value))
	contents = g_strdup_printf ("((%s*) %p)", G_PARAM_SPEC_TYPE_NAME (p), p);
      else if (G_VALUE_HOLDS_BOXED (value))
	contents = g_strdup_printf ("((%s*) %p)", g_type_name (G_VALUE_TYPE (value)), p);
      else if (G_VALUE_HOLDS_POINTER (value))
	contents = g_strdup_printf ("((gpointer) %p)", p);
      else
	contents = g_strdup ("???");
    }
  else
    contents = g_strdup ("???");

  return contents;
}

GType
g_pointer_type_register_static (const gchar *name)
{
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
    NULL		/* value_table */
  };
  GType type;

  g_return_val_if_fail (name != NULL, 0);
  g_return_val_if_fail (g_type_from_name (name) == 0, 0);

  type = g_type_register_static (G_TYPE_POINTER, name, &type_info, 0);

  return type;
}

#define __G_VALUETYPES_C__
#include "gobjectaliasdef.c"
