/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 1997, 1998, 1999, 2000 Tim Janik and Red Hat, Inc.
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
#include	"gvaluetypes.h"

#include	"gvaluecollector.h"
#include	<string.h>


/* --- value functions --- */
static void
value_long0_init (GValue *value)
{
  value->data[0].v_long = 0;
}

static void
value_long0_copy (const GValue *src_value,
		  GValue       *dest_value)
{
  dest_value->data[0].v_long = src_value->data[0].v_long;
}

static gchar*
value_char_lcopy_value (const GValue *value,
			guint	      nth_value,
			GType	     *collect_type,
			GTypeCValue  *collect_value)
{
  gint8 *int8_p = collect_value->v_pointer;
  
  if (!int8_p)
    return g_strdup_printf ("value location for `%s' passed as NULL", G_VALUE_TYPE_NAME (value));
  
  *int8_p = value->data[0].v_int;
  
  *collect_type = 0;
  return NULL;
}

static gchar*
value_boolean_lcopy_value (const GValue *value,
			   guint	 nth_value,
			   GType	*collect_type,
			   GTypeCValue	*collect_value)
{
  gboolean *bool_p = collect_value->v_pointer;
  
  if (!bool_p)
    return g_strdup_printf ("value location for `%s' passed as NULL", G_VALUE_TYPE_NAME (value));
  
  *bool_p = value->data[0].v_int;
  
  *collect_type = 0;
  return NULL;
}

static gchar*
value_int_collect_value (GValue	     *value,
			 guint	      nth_value,
			 GType	     *collect_type,
			 GTypeCValue *collect_value)
{
  value->data[0].v_int = collect_value->v_int;
  
  *collect_type = 0;
  return NULL;
}

static gchar*
value_int_lcopy_value (const GValue *value,
		       guint	     nth_value,
		       GType	    *collect_type,
		       GTypeCValue  *collect_value)
{
  gint *int_p = collect_value->v_pointer;
  
  if (!int_p)
    return g_strdup_printf ("value location for `%s' passed as NULL", G_VALUE_TYPE_NAME (value));
  
  *int_p = value->data[0].v_int;
  
  *collect_type = 0;
  return NULL;
}

static gchar*
value_long_collect_value (GValue      *value,
			  guint	       nth_value,
			  GType	      *collect_type,
			  GTypeCValue *collect_value)
{
  value->data[0].v_long = collect_value->v_long;
  
  *collect_type = 0;
  return NULL;
}

static gchar*
value_long_lcopy_value (const GValue *value,
			guint	      nth_value,
			GType	     *collect_type,
			GTypeCValue  *collect_value)
{
  glong *long_p = collect_value->v_pointer;
  
  if (!long_p)
    return g_strdup_printf ("value location for `%s' passed as NULL", G_VALUE_TYPE_NAME (value));
  
  *long_p = value->data[0].v_long;
  
  *collect_type = 0;
  return NULL;
}

static void
value_float_init (GValue *value)
{
  value->data[0].v_float = 0.0;
}

static void
value_float_copy (const GValue *src_value,
		  GValue       *dest_value)
{
  dest_value->data[0].v_float = src_value->data[0].v_float;
}

static gchar*
value_float_collect_value (GValue      *value,
			   guint	nth_value,
			   GType       *collect_type,
			   GTypeCValue *collect_value)
{
  value->data[0].v_float = collect_value->v_double;
  
  *collect_type = 0;
  return NULL;
}

static gchar*
value_float_lcopy_value (const GValue *value,
			 guint	       nth_value,
			 GType	      *collect_type,
			 GTypeCValue  *collect_value)
{
  gfloat *float_p = collect_value->v_pointer;
  
  if (!float_p)
    return g_strdup_printf ("value location for `%s' passed as NULL", G_VALUE_TYPE_NAME (value));
  
  *float_p = value->data[0].v_float;
  
  *collect_type = 0;
  return NULL;
}

static void
value_double_init (GValue *value)
{
  value->data[0].v_double = 0.0;
}

static void
value_double_copy (const GValue *src_value,
		   GValue	*dest_value)
{
  dest_value->data[0].v_double = src_value->data[0].v_double;
}

static gchar*
value_double_collect_value (GValue	*value,
			    guint	 nth_value,
			    GType	*collect_type,
			    GTypeCValue *collect_value)
{
  value->data[0].v_double = collect_value->v_double;
  
  *collect_type = 0;
  return NULL;
}

static gchar*
value_double_lcopy_value (const GValue *value,
			  guint		nth_value,
			  GType	       *collect_type,
			  GTypeCValue  *collect_value)
{
  gdouble *double_p = collect_value->v_pointer;
  
  if (!double_p)
    return g_strdup_printf ("value location for `%s' passed as NULL", G_VALUE_TYPE_NAME (value));
  
  *double_p = value->data[0].v_double;
  
  *collect_type = 0;
  return NULL;
}

static void
value_string_init (GValue *value)
{
  value->data[0].v_pointer = NULL;
}

static void
value_string_free_value (GValue *value)
{
  g_free (value->data[0].v_pointer);
}

static void
value_string_copy_value (const GValue *src_value,
			 GValue	      *dest_value)
{
  dest_value->data[0].v_pointer = g_strdup (src_value->data[0].v_pointer);
}

static gchar*
value_string_collect_value (GValue	*value,
			    guint	 nth_value,
			    GType	*collect_type,
			    GTypeCValue *collect_value)
{
  value->data[0].v_pointer = g_strdup (collect_value->v_pointer);
  
  *collect_type = 0;
  return NULL;
}

static gchar*
value_string_lcopy_value (const GValue *value,
			  guint		nth_value,
			  GType	       *collect_type,
			  GTypeCValue  *collect_value)
{
  gchar **string_p = collect_value->v_pointer;
  
  if (!string_p)
    return g_strdup_printf ("value location for `%s' passed as NULL", G_VALUE_TYPE_NAME (value));
  
  *string_p = g_strdup (value->data[0].v_pointer);
  
  *collect_type = 0;
  return NULL;
}


/* --- type initialization --- */
void
g_value_types_init (void)  /* sync with gtype.c */
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
      value_long0_init,		/* value_init */
      NULL,			/* value_free */
      value_long0_copy,		/* value_copy */
      G_VALUE_COLLECT_INT,	/* collect_type */
      value_int_collect_value,	/* collect_value */
      G_VALUE_COLLECT_POINTER,	/* lcopy_type */
      value_char_lcopy_value,	/* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_CHAR, "gchar", &info, &finfo);
    g_assert (type == G_TYPE_CHAR);
    type = g_type_register_fundamental (G_TYPE_UCHAR, "guchar", &info, &finfo);
    g_assert (type == G_TYPE_UCHAR);
  }

  /* G_TYPE_BOOLEAN
   */
  {
    static const GTypeValueTable value_table = {
      value_long0_init,		 /* value_init */
      NULL,			 /* value_free */
      value_long0_copy,		 /* value_copy */
      G_VALUE_COLLECT_INT,	 /* collect_type */
      value_int_collect_value,	 /* collect_value */
      G_VALUE_COLLECT_POINTER,	 /* lcopy_type */
      value_boolean_lcopy_value, /* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_BOOLEAN, "gboolean", &info, &finfo);
    g_assert (type == G_TYPE_BOOLEAN);
  }
  
  /* G_TYPE_INT / G_TYPE_UINT
   */
  {
    static const GTypeValueTable value_table = {
      value_long0_init,		/* value_init */
      NULL,			/* value_free */
      value_long0_copy,		/* value_copy */
      G_VALUE_COLLECT_INT,	/* collect_type */
      value_int_collect_value,	/* collect_value */
      G_VALUE_COLLECT_POINTER,	/* lcopy_type */
      value_int_lcopy_value,	/* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_INT, "gint", &info, &finfo);
    g_assert (type == G_TYPE_INT);
    type = g_type_register_fundamental (G_TYPE_UINT, "guint", &info, &finfo);
    g_assert (type == G_TYPE_UINT);
  }

  /* G_TYPE_LONG / G_TYPE_ULONG
   */
  {
    static const GTypeValueTable value_table = {
      value_long0_init,		/* value_init */
      NULL,			/* value_free */
      value_long0_copy,		/* value_copy */
      G_VALUE_COLLECT_LONG,	/* collect_type */
      value_long_collect_value,	/* collect_value */
      G_VALUE_COLLECT_POINTER,	/* lcopy_type */
      value_long_lcopy_value,	/* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_LONG, "glong", &info, &finfo);
    g_assert (type == G_TYPE_LONG);
    type = g_type_register_fundamental (G_TYPE_ULONG, "gulong", &info, &finfo);
    g_assert (type == G_TYPE_ULONG);
  }
  
  /* G_TYPE_FLOAT
   */
  {
    static const GTypeValueTable value_table = {
      value_float_init,		 /* value_init */
      NULL,			 /* value_free */
      value_float_copy,		 /* value_copy */
      G_VALUE_COLLECT_DOUBLE,	 /* collect_type */
      value_float_collect_value, /* collect_value */
      G_VALUE_COLLECT_POINTER,	 /* lcopy_type */
      value_float_lcopy_value,	 /* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_FLOAT, "gfloat", &info, &finfo);
    g_assert (type == G_TYPE_FLOAT);
  }
  
  /* G_TYPE_DOUBLE
   */
  {
    static const GTypeValueTable value_table = {
      value_double_init,	  /* value_init */
      NULL,			  /* value_free */
      value_double_copy,	  /* value_copy */
      G_VALUE_COLLECT_DOUBLE,	  /* collect_type */
      value_double_collect_value, /* collect_value */
      G_VALUE_COLLECT_POINTER,	  /* lcopy_type */
      value_double_lcopy_value,	  /* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_DOUBLE, "gdouble", &info, &finfo);
    g_assert (type == G_TYPE_DOUBLE);
  }

  /* G_TYPE_STRING
   */
  {
    static const GTypeValueTable value_table = {
      value_string_init,	  /* value_init */
      value_string_free_value,	  /* value_free */
      value_string_copy_value,	  /* value_copy */
      G_VALUE_COLLECT_POINTER,	  /* collect_type */
      value_string_collect_value, /* collect_value */
      G_VALUE_COLLECT_POINTER,	  /* lcopy_type */
      value_string_lcopy_value,	  /* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_STRING, "gstring", &info, &finfo);
    g_assert (type == G_TYPE_STRING);
  }
}


/* --- GValue functions --- */
void
g_value_set_char (GValue *value,
		  gint8	  v_char)
{
  g_return_if_fail (G_IS_VALUE_CHAR (value));
  
  value->data[0].v_int = v_char;
}

gint8
g_value_get_char (GValue *value)
{
  g_return_val_if_fail (G_IS_VALUE_CHAR (value), 0);
  
  return value->data[0].v_int;
}

void
g_value_set_uchar (GValue *value,
		   guint8  v_uchar)
{
  g_return_if_fail (G_IS_VALUE_UCHAR (value));
  
  value->data[0].v_uint = v_uchar;
}

guint8
g_value_get_uchar (GValue *value)
{
  g_return_val_if_fail (G_IS_VALUE_UCHAR (value), 0);
  
  return value->data[0].v_uint;
}

void
g_value_set_boolean (GValue  *value,
		     gboolean v_boolean)
{
  g_return_if_fail (G_IS_VALUE_BOOLEAN (value));
  
  value->data[0].v_int = v_boolean;
}

gboolean
g_value_get_boolean (GValue *value)
{
  g_return_val_if_fail (G_IS_VALUE_BOOLEAN (value), 0);
  
  return value->data[0].v_int;
}

void
g_value_set_int (GValue *value,
		 gint	 v_int)
{
  g_return_if_fail (G_IS_VALUE_INT (value));
  
  value->data[0].v_int = v_int;
}

gint
g_value_get_int (GValue *value)
{
  g_return_val_if_fail (G_IS_VALUE_INT (value), 0);
  
  return value->data[0].v_int;
}

void
g_value_set_uint (GValue *value,
		  guint	  v_uint)
{
  g_return_if_fail (G_IS_VALUE_UINT (value));
  
  value->data[0].v_uint = v_uint;
}

guint
g_value_get_uint (GValue *value)
{
  g_return_val_if_fail (G_IS_VALUE_UINT (value), 0);
  
  return value->data[0].v_uint;
}

void
g_value_set_long (GValue *value,
		  glong	  v_long)
{
  g_return_if_fail (G_IS_VALUE_LONG (value));
  
  value->data[0].v_long = v_long;
}

glong
g_value_get_long (GValue *value)
{
  g_return_val_if_fail (G_IS_VALUE_LONG (value), 0);
  
  return value->data[0].v_long;
}

void
g_value_set_ulong (GValue *value,
		   gulong  v_ulong)
{
  g_return_if_fail (G_IS_VALUE_ULONG (value));
  
  value->data[0].v_ulong = v_ulong;
}

gulong
g_value_get_ulong (GValue *value)
{
  g_return_val_if_fail (G_IS_VALUE_ULONG (value), 0);
  
  return value->data[0].v_ulong;
}

void
g_value_set_float (GValue *value,
		   gfloat  v_float)
{
  g_return_if_fail (G_IS_VALUE_FLOAT (value));
  
  value->data[0].v_float = v_float;
}

gfloat
g_value_get_float (GValue *value)
{
  g_return_val_if_fail (G_IS_VALUE_FLOAT (value), 0);
  
  return value->data[0].v_float;
}

void
g_value_set_double (GValue *value,
		    gdouble v_double)
{
  g_return_if_fail (G_IS_VALUE_DOUBLE (value));
  
  value->data[0].v_double = v_double;
}

gdouble
g_value_get_double (GValue *value)
{
  g_return_val_if_fail (G_IS_VALUE_DOUBLE (value), 0);
  
  return value->data[0].v_double;
}

void
g_value_set_string (GValue	*value,
		    const gchar *v_string)
{
  g_return_if_fail (G_IS_VALUE_STRING (value));
  
  g_free (value->data[0].v_pointer);
  value->data[0].v_pointer = g_strdup (v_string);
}

gchar*
g_value_get_string (GValue *value)
{
  g_return_val_if_fail (G_IS_VALUE_STRING (value), NULL);
  
  return value->data[0].v_pointer;
}

gchar*
g_value_dup_string (GValue *value)
{
  g_return_val_if_fail (G_IS_VALUE_STRING (value), NULL);
  
  return g_strdup (value->data[0].v_pointer);
}
