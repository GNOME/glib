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

/*
 * MT safe
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
value_boolean_lcopy_value (const GValue *value,
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
value_int_collect_value (GValue	     *value,
			 guint        n_collect_values,
			 GTypeCValue *collect_values,
			 guint        collect_flags)
{
  value->data[0].v_int = collect_values[0].v_int;
  
  return NULL;
}

static gchar*
value_int_lcopy_value (const GValue *value,
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
value_long_collect_value (GValue      *value,
			  guint        n_collect_values,
			  GTypeCValue *collect_values,
			  guint        collect_flags)
{
  value->data[0].v_long = collect_values[0].v_long;
  
  return NULL;
}

static gchar*
value_long_lcopy_value (const GValue *value,
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
			   guint        n_collect_values,
			   GTypeCValue *collect_values,
			   guint        collect_flags)
{
  value->data[0].v_float = collect_values[0].v_double;
  
  return NULL;
}

static gchar*
value_float_lcopy_value (const GValue *value,
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
			    guint        n_collect_values,
			    GTypeCValue *collect_values,
			    guint        collect_flags)
{
  value->data[0].v_double = collect_values[0].v_double;
  
  return NULL;
}

static gchar*
value_double_lcopy_value (const GValue *value,
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
value_string_init (GValue *value)
{
  value->data[0].v_pointer = NULL;
}

static void
value_string_free_value (GValue *value)
{
  if (!(value->data[1].v_uint & G_VALUE_NOCOPY_CONTENTS))
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
value_string_lcopy_value (const GValue *value,
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
value_pointer_init (GValue *value)
{
  value->data[0].v_pointer = NULL;
}

static void
value_pointer_copy (const GValue *src_value,
		    GValue       *dest_value)
{
  dest_value->data[0].v_pointer = src_value->data[0].v_pointer;
}

static gpointer
value_pointer_peek_pointer (const GValue *value)
{
  return value->data[0].v_pointer;
}

static gchar*
value_pointer_collect_value (GValue      *value,
			     guint        n_collect_values,
			     GTypeCValue *collect_values,
			     guint        collect_flags)
{
  value->data[0].v_pointer = collect_values[0].v_pointer;

  return NULL;
}

static gchar*
value_pointer_lcopy_value (const GValue *value,
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

static void
value_ccallback_init (GValue *value)
{
  value->data[0].v_pointer = NULL;
  value->data[1].v_pointer = NULL;
}

static void
value_ccallback_copy (const GValue *src_value,
		      GValue       *dest_value)
{
  dest_value->data[0].v_pointer = src_value->data[0].v_pointer;
  dest_value->data[1].v_pointer = src_value->data[1].v_pointer;
}

static gchar*
value_ccallback_collect_value (GValue      *value,
			       guint        n_collect_values,
			       GTypeCValue *collect_values,
			       guint        collect_flags)
{
  gchar *error = NULL;

  if (!collect_values[0].v_pointer)
    error = g_strconcat ("invalid (NULL) pointer callback function for value type `",
			 G_VALUE_TYPE_NAME (value),
			 "'",
			 NULL);
  value->data[0].v_pointer = collect_values[0].v_pointer;
  value->data[1].v_pointer = collect_values[1].v_pointer;

  return error;
}

static gchar*
value_ccallback_lcopy_value (const GValue *value,
			     guint         n_collect_values,
			     GTypeCValue  *collect_values,
			     guint         collect_flags)
{
  gpointer *callback_p = collect_values[0].v_pointer;
  gpointer *data_p = collect_values[1].v_pointer;

  if (!callback_p || !data_p)
    return g_strdup_printf ("%s location for `%s' passed as NULL",
			    callback_p ? "data" : "callback",
			    G_VALUE_TYPE_NAME (value));
  *callback_p = value->data[0].v_pointer;
  *data_p = value->data[1].v_pointer;

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
      NULL,			/* value_peek_pointer */
      "i",			/* collect_format */
      value_int_collect_value,	/* collect_value */
      "p",			/* lcopy_format */
      value_char_lcopy_value,	/* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_CHAR, "gchar", &info, &finfo, 0);
    g_assert (type == G_TYPE_CHAR);
    type = g_type_register_fundamental (G_TYPE_UCHAR, "guchar", &info, &finfo, 0);
    g_assert (type == G_TYPE_UCHAR);
  }

  /* G_TYPE_BOOLEAN
   */
  {
    static const GTypeValueTable value_table = {
      value_long0_init,		 /* value_init */
      NULL,			 /* value_free */
      value_long0_copy,		 /* value_copy */
      NULL,                      /* value_peek_pointer */
      "i",			 /* collect_format */
      value_int_collect_value,	 /* collect_value */
      "p",			 /* lcopy_format */
      value_boolean_lcopy_value, /* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_BOOLEAN, "gboolean", &info, &finfo, 0);
    g_assert (type == G_TYPE_BOOLEAN);
  }
  
  /* G_TYPE_INT / G_TYPE_UINT
   */
  {
    static const GTypeValueTable value_table = {
      value_long0_init,		/* value_init */
      NULL,			/* value_free */
      value_long0_copy,		/* value_copy */
      NULL,                     /* value_peek_pointer */
      "i",			/* collect_format */
      value_int_collect_value,	/* collect_value */
      "p",			/* lcopy_format */
      value_int_lcopy_value,	/* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_INT, "gint", &info, &finfo, 0);
    g_assert (type == G_TYPE_INT);
    type = g_type_register_fundamental (G_TYPE_UINT, "guint", &info, &finfo, 0);
    g_assert (type == G_TYPE_UINT);
  }

  /* G_TYPE_LONG / G_TYPE_ULONG
   */
  {
    static const GTypeValueTable value_table = {
      value_long0_init,		/* value_init */
      NULL,			/* value_free */
      value_long0_copy,		/* value_copy */
      NULL,                     /* value_peek_pointer */
      "l",			/* collect_format */
      value_long_collect_value,	/* collect_value */
      "p",			/* lcopy_format */
      value_long_lcopy_value,	/* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_LONG, "glong", &info, &finfo, 0);
    g_assert (type == G_TYPE_LONG);
    type = g_type_register_fundamental (G_TYPE_ULONG, "gulong", &info, &finfo, 0);
    g_assert (type == G_TYPE_ULONG);
  }
  
  /* G_TYPE_FLOAT
   */
  {
    static const GTypeValueTable value_table = {
      value_float_init,		 /* value_init */
      NULL,			 /* value_free */
      value_float_copy,		 /* value_copy */
      NULL,                      /* value_peek_pointer */
      "d",			 /* collect_format */
      value_float_collect_value, /* collect_value */
      "p",			 /* lcopy_format */
      value_float_lcopy_value,	 /* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_FLOAT, "gfloat", &info, &finfo, 0);
    g_assert (type == G_TYPE_FLOAT);
  }
  
  /* G_TYPE_DOUBLE
   */
  {
    static const GTypeValueTable value_table = {
      value_double_init,	  /* value_init */
      NULL,			  /* value_free */
      value_double_copy,	  /* value_copy */
      NULL,                       /* value_peek_pointer */
      "d",			  /* collect_format */
      value_double_collect_value, /* collect_value */
      "p",			  /* lcopy_format */
      value_double_lcopy_value,	  /* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_DOUBLE, "gdouble", &info, &finfo, 0);
    g_assert (type == G_TYPE_DOUBLE);
  }

  /* G_TYPE_STRING
   */
  {
    static const GTypeValueTable value_table = {
      value_string_init,	  /* value_init */
      value_string_free_value,	  /* value_free */
      value_string_copy_value,	  /* value_copy */
      value_pointer_peek_pointer, /* value_peek_pointer */
      "p",			  /* collect_format */
      value_string_collect_value, /* collect_value */
      "p",			  /* lcopy_format */
      value_string_lcopy_value,	  /* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_STRING, "gstring", &info, &finfo, 0);
    g_assert (type == G_TYPE_STRING);
  }

  /* G_TYPE_POINTER
   */
  {
    static const GTypeValueTable value_table = {
      value_pointer_init,          /* value_init */
      NULL,                        /* value_free */
      value_pointer_copy,          /* value_copy */
      value_pointer_peek_pointer,  /* value_peek_pointer */
      "p",			   /* collect_format */
      value_pointer_collect_value, /* collect_value */
      "p",			   /* lcopy_format */
      value_pointer_lcopy_value,   /* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_POINTER, "gpointer", &info, &finfo, 0);
    g_assert (type == G_TYPE_POINTER);
  }

  /* G_TYPE_CCALLBACK
   */
  {
    static const GTypeValueTable value_table = {
      value_ccallback_init,          /* value_init */
      NULL,                          /* value_free */
      value_ccallback_copy,          /* value_copy */
      NULL,                          /* value_peek_pointer */
      "pp",			     /* collect_format */
      value_ccallback_collect_value, /* collect_value */
      "pp",			     /* lcopy_format */
      value_ccallback_lcopy_value,   /* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_CCALLBACK, "GCCallback", &info, &finfo, 0);
    g_assert (type == G_TYPE_CCALLBACK);
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
g_value_get_char (const GValue *value)
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
g_value_get_uchar (const GValue *value)
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
g_value_get_boolean (const GValue *value)
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
g_value_get_int (const GValue *value)
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
g_value_get_uint (const GValue *value)
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
g_value_get_long (const GValue *value)
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
g_value_get_ulong (const GValue *value)
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
g_value_get_float (const GValue *value)
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
g_value_get_double (const GValue *value)
{
  g_return_val_if_fail (G_IS_VALUE_DOUBLE (value), 0);
  
  return value->data[0].v_double;
}

void
g_value_set_string (GValue	*value,
		    const gchar *v_string)
{
  g_return_if_fail (G_IS_VALUE_STRING (value));
  
  if (value->data[1].v_uint & G_VALUE_NOCOPY_CONTENTS)
    value->data[1].v_uint = 0;
  else
    g_free (value->data[0].v_pointer);
  value->data[0].v_pointer = g_strdup (v_string);
}

void
g_value_set_static_string (GValue      *value,
			   const gchar *v_string)
{
  g_return_if_fail (G_IS_VALUE_STRING (value));
  
  if (!(value->data[1].v_uint & G_VALUE_NOCOPY_CONTENTS))
    g_free (value->data[0].v_pointer);
  value->data[1].v_uint = G_VALUE_NOCOPY_CONTENTS;
  value->data[0].v_pointer = (gchar*) v_string;
}

G_CONST_RETURN gchar*
g_value_get_string (const GValue *value)
{
  g_return_val_if_fail (G_IS_VALUE_STRING (value), NULL);
  
  return value->data[0].v_pointer;
}

gchar*
g_value_dup_string (const GValue *value)
{
  g_return_val_if_fail (G_IS_VALUE_STRING (value), NULL);
  
  return g_strdup (value->data[0].v_pointer);
}

void
g_value_set_pointer (GValue  *value,
		     gpointer v_pointer)
{
  g_return_if_fail (G_IS_VALUE_POINTER (value));

  value->data[0].v_pointer = v_pointer;
}

gpointer
g_value_get_pointer (const GValue *value)
{
  g_return_val_if_fail (G_IS_VALUE_POINTER (value), NULL);

  return value->data[0].v_pointer;
}

void
g_value_set_ccallback (GValue  *value,
		       gpointer callback_func,
		       gpointer callback_data)
{
  g_return_if_fail (G_IS_VALUE_CCALLBACK (value));
  
  value->data[0].v_pointer = callback_func;
  value->data[1].v_pointer = callback_data;
}

void
g_value_get_ccallback (const GValue *value,
		       gpointer     *callback_func,
		       gpointer     *callback_data)
{
  g_return_if_fail (G_IS_VALUE_CCALLBACK (value));

  if (callback_func)
    *callback_func = value->data[0].v_pointer;
  if (callback_data)
    *callback_data = value->data[1].v_pointer;
}
