/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 1998-1999, 2000-2001 Tim Janik and Red Hat, Inc.
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

#include        <string.h>

#include	"genums.h"

#include	"gvalue.h"
#include	"gvaluecollector.h"

#include	"gobjectalias.h"


/* --- prototypes --- */
static void	g_enum_class_init		(GEnumClass	*class,
						 gpointer	 class_data);
static void	g_flags_class_init		(GFlagsClass	*class,
						 gpointer	 class_data);
static void	value_flags_enum_init		(GValue		*value);
static void	value_flags_enum_copy_value	(const GValue	*src_value,
						 GValue		*dest_value);
static gchar*	value_flags_enum_collect_value  (GValue		*value,
						 guint           n_collect_values,
						 GTypeCValue    *collect_values,
						 guint           collect_flags);
static gchar*	value_flags_enum_lcopy_value	(const GValue	*value,
						 guint           n_collect_values,
						 GTypeCValue    *collect_values,
						 guint           collect_flags);

/* --- functions --- */
void
g_enum_types_init (void)
{
  static gboolean initialized = FALSE;
  static const GTypeValueTable flags_enum_value_table = {
    value_flags_enum_init,	    /* value_init */
    NULL,			    /* value_free */
    value_flags_enum_copy_value,    /* value_copy */
    NULL,			    /* value_peek_pointer */
    "i",			    /* collect_format */
    value_flags_enum_collect_value, /* collect_value */
    "p",			    /* lcopy_format */
    value_flags_enum_lcopy_value,   /* lcopy_value */
  };
  static GTypeInfo info = {
    0,                          /* class_size */
    NULL,                       /* base_init */
    NULL,                       /* base_destroy */
    NULL,                       /* class_init */
    NULL,                       /* class_destroy */
    NULL,                       /* class_data */
    0,                          /* instance_size */
    0,                          /* n_preallocs */
    NULL,                       /* instance_init */
    &flags_enum_value_table,    /* value_table */
  };
  static const GTypeFundamentalInfo finfo = {
    G_TYPE_FLAG_CLASSED | G_TYPE_FLAG_DERIVABLE,
  };
  GType type;
  
  g_return_if_fail (initialized == FALSE);
  initialized = TRUE;
  
  /* G_TYPE_ENUM
   */
  info.class_size = sizeof (GEnumClass);
  type = g_type_register_fundamental (G_TYPE_ENUM, g_intern_static_string ("GEnum"), &info, &finfo,
				      G_TYPE_FLAG_ABSTRACT | G_TYPE_FLAG_VALUE_ABSTRACT);
  g_assert (type == G_TYPE_ENUM);
  
  /* G_TYPE_FLAGS
   */
  info.class_size = sizeof (GFlagsClass);
  type = g_type_register_fundamental (G_TYPE_FLAGS, g_intern_static_string ("GFlags"), &info, &finfo,
				      G_TYPE_FLAG_ABSTRACT | G_TYPE_FLAG_VALUE_ABSTRACT);
  g_assert (type == G_TYPE_FLAGS);
}

static void
value_flags_enum_init (GValue *value)
{
  value->data[0].v_long = 0;
}

static void
value_flags_enum_copy_value (const GValue *src_value,
			     GValue	  *dest_value)
{
  dest_value->data[0].v_long = src_value->data[0].v_long;
}

static gchar*
value_flags_enum_collect_value (GValue      *value,
				guint        n_collect_values,
				GTypeCValue *collect_values,
				guint        collect_flags)
{
  value->data[0].v_long = collect_values[0].v_int;

  return NULL;
}

static gchar*
value_flags_enum_lcopy_value (const GValue *value,
			      guint         n_collect_values,
			      GTypeCValue  *collect_values,
			      guint         collect_flags)
{
  gint *int_p = collect_values[0].v_pointer;
  
  if (!int_p)
    return g_strdup_printf ("value location for `%s' passed as NULL", G_VALUE_TYPE_NAME (value));
  
  *int_p = value->data[0].v_long;
  
  return NULL;
}

GType
g_enum_register_static (const gchar	 *name,
			const GEnumValue *const_static_values)
{
  GTypeInfo enum_type_info = {
    sizeof (GEnumClass), /* class_size */
    NULL,                /* base_init */
    NULL,                /* base_finalize */
    (GClassInitFunc) g_enum_class_init,
    NULL,                /* class_finalize */
    NULL,                /* class_data */
    0,                   /* instance_size */
    0,                   /* n_preallocs */
    NULL,                /* instance_init */
    NULL,		 /* value_table */
  };
  GType type;
  
  g_return_val_if_fail (name != NULL, 0);
  g_return_val_if_fail (const_static_values != NULL, 0);
  
  enum_type_info.class_data = const_static_values;
  
  type = g_type_register_static (G_TYPE_ENUM, name, &enum_type_info, 0);
  
  return type;
}

GType
g_flags_register_static (const gchar	   *name,
			 const GFlagsValue *const_static_values)
{
  GTypeInfo flags_type_info = {
    sizeof (GFlagsClass), /* class_size */
    NULL,                 /* base_init */
    NULL,                 /* base_finalize */
    (GClassInitFunc) g_flags_class_init,
    NULL,                 /* class_finalize */
    NULL,                 /* class_data */
    0,                    /* instance_size */
    0,                    /* n_preallocs */
    NULL,                 /* instance_init */
    NULL,		  /* value_table */
  };
  GType type;
  
  g_return_val_if_fail (name != NULL, 0);
  g_return_val_if_fail (const_static_values != NULL, 0);
  
  flags_type_info.class_data = const_static_values;
  
  type = g_type_register_static (G_TYPE_FLAGS, name, &flags_type_info, 0);
  
  return type;
}

void
g_enum_complete_type_info (GType	     g_enum_type,
			   GTypeInfo	    *info,
			   const GEnumValue *const_values)
{
  g_return_if_fail (G_TYPE_IS_ENUM (g_enum_type));
  g_return_if_fail (info != NULL);
  g_return_if_fail (const_values != NULL);
  
  info->class_size = sizeof (GEnumClass);
  info->base_init = NULL;
  info->base_finalize = NULL;
  info->class_init = (GClassInitFunc) g_enum_class_init;
  info->class_finalize = NULL;
  info->class_data = const_values;
}

void
g_flags_complete_type_info (GType	       g_flags_type,
			    GTypeInfo	      *info,
			    const GFlagsValue *const_values)
{
  g_return_if_fail (G_TYPE_IS_FLAGS (g_flags_type));
  g_return_if_fail (info != NULL);
  g_return_if_fail (const_values != NULL);
  
  info->class_size = sizeof (GFlagsClass);
  info->base_init = NULL;
  info->base_finalize = NULL;
  info->class_init = (GClassInitFunc) g_flags_class_init;
  info->class_finalize = NULL;
  info->class_data = const_values;
}

static void
g_enum_class_init (GEnumClass *class,
		   gpointer    class_data)
{
  g_return_if_fail (G_IS_ENUM_CLASS (class));
  
  class->minimum = 0;
  class->maximum = 0;
  class->n_values = 0;
  class->values = class_data;
  
  if (class->values)
    {
      GEnumValue *values;
      
      class->minimum = class->values->value;
      class->maximum = class->values->value;
      for (values = class->values; values->value_name; values++)
	{
	  class->minimum = MIN (class->minimum, values->value);
	  class->maximum = MAX (class->maximum, values->value);
	  class->n_values++;
	}
    }
}

static void
g_flags_class_init (GFlagsClass *class,
		    gpointer	 class_data)
{
  g_return_if_fail (G_IS_FLAGS_CLASS (class));
  
  class->mask = 0;
  class->n_values = 0;
  class->values = class_data;
  
  if (class->values)
    {
      GFlagsValue *values;
      
      for (values = class->values; values->value_name; values++)
	{
	  class->mask |= values->value;
	  class->n_values++;
	}
    }
}

GEnumValue*
g_enum_get_value_by_name (GEnumClass  *enum_class,
			  const gchar *name)
{
  g_return_val_if_fail (G_IS_ENUM_CLASS (enum_class), NULL);
  g_return_val_if_fail (name != NULL, NULL);
  
  if (enum_class->n_values)
    {
      GEnumValue *enum_value;
      
      for (enum_value = enum_class->values; enum_value->value_name; enum_value++)
	if (strcmp (name, enum_value->value_name) == 0)
	  return enum_value;
    }
  
  return NULL;
}

GFlagsValue*
g_flags_get_value_by_name (GFlagsClass *flags_class,
			   const gchar *name)
{
  g_return_val_if_fail (G_IS_FLAGS_CLASS (flags_class), NULL);
  g_return_val_if_fail (name != NULL, NULL);
  
  if (flags_class->n_values)
    {
      GFlagsValue *flags_value;
      
      for (flags_value = flags_class->values; flags_value->value_name; flags_value++)
	if (strcmp (name, flags_value->value_name) == 0)
	  return flags_value;
    }
  
  return NULL;
}

GEnumValue*
g_enum_get_value_by_nick (GEnumClass  *enum_class,
			  const gchar *nick)
{
  g_return_val_if_fail (G_IS_ENUM_CLASS (enum_class), NULL);
  g_return_val_if_fail (nick != NULL, NULL);
  
  if (enum_class->n_values)
    {
      GEnumValue *enum_value;
      
      for (enum_value = enum_class->values; enum_value->value_name; enum_value++)
	if (enum_value->value_nick && strcmp (nick, enum_value->value_nick) == 0)
	  return enum_value;
    }
  
  return NULL;
}

GFlagsValue*
g_flags_get_value_by_nick (GFlagsClass *flags_class,
			   const gchar *nick)
{
  g_return_val_if_fail (G_IS_FLAGS_CLASS (flags_class), NULL);
  g_return_val_if_fail (nick != NULL, NULL);
  
  if (flags_class->n_values)
    {
      GFlagsValue *flags_value;
      
      for (flags_value = flags_class->values; flags_value->value_nick; flags_value++)
	if (flags_value->value_nick && strcmp (nick, flags_value->value_nick) == 0)
	  return flags_value;
    }
  
  return NULL;
}

GEnumValue*
g_enum_get_value (GEnumClass *enum_class,
		  gint	      value)
{
  g_return_val_if_fail (G_IS_ENUM_CLASS (enum_class), NULL);
  
  if (enum_class->n_values)
    {
      GEnumValue *enum_value;
      
      for (enum_value = enum_class->values; enum_value->value_name; enum_value++)
	if (enum_value->value == value)
	  return enum_value;
    }
  
  return NULL;
}

GFlagsValue*
g_flags_get_first_value (GFlagsClass *flags_class,
			 guint	      value)
{
  g_return_val_if_fail (G_IS_FLAGS_CLASS (flags_class), NULL);
  
  if (flags_class->n_values)
    {
      GFlagsValue *flags_value;

      if (value == 0)
        {
          for (flags_value = flags_class->values; flags_value->value_name; flags_value++)
            if (flags_value->value == 0)
              return flags_value;
        }
      else
        {
          for (flags_value = flags_class->values; flags_value->value_name; flags_value++)
            if (flags_value->value != 0 && (flags_value->value & value) == flags_value->value)
              return flags_value;
        }      
    }
  
  return NULL;
}

void
g_value_set_enum (GValue *value,
		  gint    v_enum)
{
  g_return_if_fail (G_VALUE_HOLDS_ENUM (value));
  
  value->data[0].v_long = v_enum;
}

gint
g_value_get_enum (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_ENUM (value), 0);
  
  return value->data[0].v_long;
}

void
g_value_set_flags (GValue *value,
		   guint   v_flags)
{
  g_return_if_fail (G_VALUE_HOLDS_FLAGS (value));
  
  value->data[0].v_ulong = v_flags;
}

guint
g_value_get_flags (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_FLAGS (value), 0);
  
  return value->data[0].v_ulong;
}

#define __G_ENUMS_C__
#include "gobjectaliasdef.c"
