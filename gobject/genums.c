/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 1998, 1999, 2000 Tim Janik and Red Hat, Inc.
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
#include	"genums.h"


/* --- prototypes --- */
extern void	g_enum_types_init		(void);
static void	g_enum_class_init		(GEnumClass	*class,
						 gpointer	 class_data);
static void	g_flags_class_init		(GFlagsClass	*class,
						 gpointer	 class_data);


/* --- functions --- */
void
g_enum_types_init (void)	/* sync with glib-gtype.c */
{
  static gboolean initialized = FALSE;
  static const GTypeFundamentalInfo finfo = {
    G_TYPE_FLAG_CLASSED | G_TYPE_FLAG_DERIVABLE,
    0		/* n_collect_bytes */,
    NULL	/* GTypeParamCollector */,
  };
  static GTypeInfo info = {
    0	/* class_size */,
    NULL	/* base_init */,
    NULL	/* base_finalize */,
    NULL	/* class_init */,
    NULL	/* class_finalize */,
    NULL	/* class_data */,
  };
  GType type;
  
  g_return_if_fail (initialized == FALSE);
  initialized = TRUE;
  
  /* G_TYPE_ENUM
   */
  info.class_size = sizeof (GEnumClass);
  type = g_type_register_fundamental (G_TYPE_ENUM, "GEnum", &finfo, &info);
  g_assert (type == G_TYPE_ENUM);
  
  /* G_TYPE_FLAGS
   */
  info.class_size = sizeof (GFlagsClass);
  type = g_type_register_fundamental (G_TYPE_FLAGS, "GFlags", &finfo, &info);
  g_assert (type == G_TYPE_FLAGS);
}

GType
g_enum_register_static (const gchar	 *name,
			const GEnumValue *const_static_values)
{
  GTypeInfo enum_type_info = {
    sizeof (GEnumClass),
    NULL	/* base_init */,
    NULL	/* base_finalize */,
    (GClassInitFunc) g_enum_class_init,
    NULL	/* class_finalize */,
    NULL	/* class_data */,
  };
  GType type;
  
  g_return_val_if_fail (name != NULL, 0);
  g_return_val_if_fail (const_static_values != NULL, 0);
  
  enum_type_info.class_data = const_static_values;
  
  type = g_type_register_static (G_TYPE_ENUM, name, &enum_type_info);
  
  return type;
}

GType
g_flags_register_static (const gchar	   *name,
			 const GFlagsValue *const_static_values)
{
  GTypeInfo flags_type_info = {
    sizeof (GFlagsClass),
    NULL	/* base_init */,
    NULL	/* base_finalize */,
    (GClassInitFunc) g_flags_class_init,
    NULL	/* class_finalize */,
    NULL	/* class_data */,
  };
  GType type;
  
  g_return_val_if_fail (name != NULL, 0);
  g_return_val_if_fail (const_static_values != NULL, 0);
  
  flags_type_info.class_data = const_static_values;
  
  type = g_type_register_static (G_TYPE_FLAGS, name, &flags_type_info);
  
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
      
      for (flags_value = flags_class->values; flags_value->value_name; flags_value++)
	if ((flags_value->value & value) > 0)
	  return flags_value;
    }
  
  return NULL;
}
