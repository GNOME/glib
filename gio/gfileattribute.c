/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
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
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include <config.h>

#include <string.h>

#include "gfileattribute.h"
#include <glib-object.h>
#include "glibintl.h"

/**
 * g_file_attribute_value_free:
 * @attr: a #GFileAttributeValue. 
 * 
 * Frees the memory used by @attr.
 *
 **/
void
g_file_attribute_value_free (GFileAttributeValue *attr)
{
  g_return_if_fail (attr != NULL);

  g_file_attribute_value_clear (attr);
  g_free (attr);
}

/**
 * g_file_attribute_value_clear:
 * @attr: a #GFileAttributeValue.
 *
 * Clears the value of @attr and sets its type to 
 * %G_FILE_ATTRIBUTE_TYPE_INVALID.
 * 
 **/
void
g_file_attribute_value_clear (GFileAttributeValue *attr)
{
  g_return_if_fail (attr != NULL);

  if (attr->type == G_FILE_ATTRIBUTE_TYPE_STRING ||
      attr->type == G_FILE_ATTRIBUTE_TYPE_BYTE_STRING)
    g_free (attr->u.string);
  
  if (attr->type == G_FILE_ATTRIBUTE_TYPE_OBJECT &&
      attr->u.obj != NULL)
    g_object_unref (attr->u.obj);
  
  attr->type = G_FILE_ATTRIBUTE_TYPE_INVALID;
}

/**
 * g_file_attribute_value_set:
 * @attr: a #GFileAttributeValue.
 * @new_value:
 * 
 **/
void
g_file_attribute_value_set (GFileAttributeValue *attr,
			    const GFileAttributeValue *new_value)
{
  g_return_if_fail (attr != NULL);
  g_return_if_fail (new_value != NULL);

  g_file_attribute_value_clear (attr);
  *attr = *new_value;

  if (attr->type == G_FILE_ATTRIBUTE_TYPE_STRING ||
      attr->type == G_FILE_ATTRIBUTE_TYPE_BYTE_STRING)
    attr->u.string = g_strdup (attr->u.string);
  
  if (attr->type == G_FILE_ATTRIBUTE_TYPE_OBJECT &&
      attr->u.obj != NULL)
    g_object_ref (attr->u.obj);
}

/**
 * g_file_attribute_value_new:
 * 
 * Returns: a new #GFileAttributeValue.
 **/
GFileAttributeValue *
g_file_attribute_value_new (void)
{
  GFileAttributeValue *attr;

  attr = g_new (GFileAttributeValue, 1);
  attr->type = G_FILE_ATTRIBUTE_TYPE_INVALID;
  return attr;
}


/**
 * g_file_attribute_value_dup:
 * @other: a #GFileAttributeValue to duplicate.
 * 
 * Returns: a duplicate of the @other.
 **/
GFileAttributeValue *
g_file_attribute_value_dup (const GFileAttributeValue *other)
{
  GFileAttributeValue *attr;

  g_return_val_if_fail (other != NULL, NULL);

  attr = g_new (GFileAttributeValue, 1);
  attr->type = G_FILE_ATTRIBUTE_TYPE_INVALID;
  g_file_attribute_value_set (attr, other);
  return attr;
}

static gboolean
valid_char (char c)
{
  return c >= 32 && c <= 126 && c != '\\';
}

static char *
escape_byte_string (const char *str)
{
  size_t len;
  int num_invalid, i;
  char *escaped_val, *p;
  unsigned char c;
  char *hex_digits = "0123456789abcdef";
  
  len = strlen (str);
  
  num_invalid = 0;
  for (i = 0; i < len; i++)
    {
      if (!valid_char (str[i]))
	num_invalid++;
    }
	
  if (num_invalid == 0)
    return g_strdup (str);
  else
    {
      escaped_val = g_malloc (len + num_invalid*3 + 1);

      p = escaped_val;
      for (i = 0; i < len; i++)
	{
	  c = str[i];
	  if (valid_char (c))
	    *p++ = c;
	  else
	    {
	      *p++ = '\\';
	      *p++ = 'x';
	      *p++ = hex_digits[(c >> 8) & 0xf];
	      *p++ = hex_digits[c & 0xf];
	    }
	}
      *p++ = 0;
      return escaped_val;
    }
}

/**
 * g_file_attribute_value_as_string:
 * @attr: a #GFileAttributeValue.
 *
 * Converts a #GFileAttributeValue to a string for display.
 * The returned string should be freed when no longer needed
 * 
 * Returns: a string from the @attr, %NULL on error, or "&lt;invalid&gt;" if 
 * @attr is of type %G_FILE_ATTRIBUTE_TYPE_INVALID.
 **/
char *
g_file_attribute_value_as_string (const GFileAttributeValue *attr)
{
  char *str;

  g_return_val_if_fail (attr != NULL, NULL);

  switch (attr->type)
    {
    case G_FILE_ATTRIBUTE_TYPE_STRING:
      str = g_strdup (attr->u.string);
      break;
    case G_FILE_ATTRIBUTE_TYPE_BYTE_STRING:
      str = escape_byte_string (attr->u.string);
      break;
    case G_FILE_ATTRIBUTE_TYPE_BOOLEAN:
      str = g_strdup_printf ("%s", attr->u.boolean?"TRUE":"FALSE");
      break;
    case G_FILE_ATTRIBUTE_TYPE_UINT32:
      str = g_strdup_printf ("%u", (unsigned int)attr->u.uint32);
      break;
    case G_FILE_ATTRIBUTE_TYPE_INT32:
      str = g_strdup_printf ("%i", (int)attr->u.int32);
      break;
    case G_FILE_ATTRIBUTE_TYPE_UINT64:
      str = g_strdup_printf ("%"G_GUINT64_FORMAT, attr->u.uint64);
      break;
    case G_FILE_ATTRIBUTE_TYPE_INT64:
      str = g_strdup_printf ("%"G_GINT64_FORMAT, attr->u.int64);
      break;
    case G_FILE_ATTRIBUTE_TYPE_OBJECT:
      str = g_strdup_printf ("%s:%p", g_type_name_from_instance
                                          ((GTypeInstance *) attr->u.obj),
                                      attr->u.obj);
      break;
    default:
      g_warning ("Invalid type in GFileInfo attribute");
      str = g_strdup ("<invalid>");
      break;
    }
  
  return str;
}

/**
 * g_file_attribute_value_get_string:
 * @attr: a #GFileAttributeValue.
 * 
 * Returns: 
 **/
const char *
g_file_attribute_value_get_string (const GFileAttributeValue *attr)
{
  if (attr == NULL)
    return NULL;

  g_return_val_if_fail (attr->type == G_FILE_ATTRIBUTE_TYPE_STRING, NULL);

  return attr->u.string;
}

/**
 * g_file_attribute_value_get_byte_string:
 * @attr: a #GFileAttributeValue.
 * 
 * Returns: 
 **/
const char *
g_file_attribute_value_get_byte_string (const GFileAttributeValue *attr)
{
  if (attr == NULL)
    return NULL;

  g_return_val_if_fail (attr->type == G_FILE_ATTRIBUTE_TYPE_BYTE_STRING, NULL);

  return attr->u.string;
}
  
/**
 * g_file_attribute_value_get_boolean:
 * @attr: a #GFileAttributeValue.
 * 
 * Returns: 
 **/
gboolean
g_file_attribute_value_get_boolean (const GFileAttributeValue *attr)
{
  if (attr == NULL)
    return FALSE;

  g_return_val_if_fail (attr->type == G_FILE_ATTRIBUTE_TYPE_BOOLEAN, FALSE);

  return attr->u.boolean;
}
  
/**
 * g_file_attribute_value_get_uint32:
 * @attr: a #GFileAttributeValue.
 * 
 * Returns: 
 **/
guint32
g_file_attribute_value_get_uint32 (const GFileAttributeValue *attr)
{
  if (attr == NULL)
    return 0;

  g_return_val_if_fail (attr->type == G_FILE_ATTRIBUTE_TYPE_UINT32, 0);

  return attr->u.uint32;
}

/**
 * g_file_attribute_value_get_int32:
 * @attr: a #GFileAttributeValue.
 * 
 * Returns: 
 **/
gint32
g_file_attribute_value_get_int32 (const GFileAttributeValue *attr)
{
  if (attr == NULL)
    return 0;

  g_return_val_if_fail (attr->type == G_FILE_ATTRIBUTE_TYPE_INT32, 0);

  return attr->u.int32;
}

/**
 * g_file_attribute_value_get_uint64:
 * @attr: a #GFileAttributeValue.
 * 
 * Returns: 
 **/  
guint64
g_file_attribute_value_get_uint64 (const GFileAttributeValue *attr)
{
  if (attr == NULL)
    return 0;

  g_return_val_if_fail (attr->type == G_FILE_ATTRIBUTE_TYPE_UINT64, 0);

  return attr->u.uint64;
}

/**
 * g_file_attribute_value_get_int64:
 * @attr: a #GFileAttributeValue.
 * 
 * Returns: 
 **/
gint64
g_file_attribute_value_get_int64 (const GFileAttributeValue *attr)
{
  if (attr == NULL)
    return 0;

  g_return_val_if_fail (attr->type == G_FILE_ATTRIBUTE_TYPE_INT64, 0);

  return attr->u.int64;
}

/**
 * g_file_attribute_value_get_object:
 * @attr: a #GFileAttributeValue.
 * 
 * Returns: 
 **/
GObject *
g_file_attribute_value_get_object (const GFileAttributeValue *attr)
{
  if (attr == NULL)
    return NULL;

  g_return_val_if_fail (attr->type == G_FILE_ATTRIBUTE_TYPE_OBJECT, NULL);

  return attr->u.obj;
}
  
/**
 * g_file_attribute_value_set_string:
 * @attr: a #GFileAttributeValue.
 * @string:
 * 
 **/
void
g_file_attribute_value_set_string (GFileAttributeValue *attr,
				   const char          *string)
{
  g_return_if_fail (attr != NULL);
  g_return_if_fail (string != NULL);

  g_file_attribute_value_clear (attr);
  attr->type = G_FILE_ATTRIBUTE_TYPE_STRING;
  attr->u.string = g_strdup (string);
}

/**
 * g_file_attribute_value_set_byte_string:
 * @attr: a #GFileAttributeValue.
 * @string:
 * 
 **/
void
g_file_attribute_value_set_byte_string (GFileAttributeValue *attr,
					const char          *string)
{
  g_return_if_fail (attr != NULL);
  g_return_if_fail (string != NULL);

  g_file_attribute_value_clear (attr);
  attr->type = G_FILE_ATTRIBUTE_TYPE_BYTE_STRING;
  attr->u.string = g_strdup (string);
}

/**
 * g_file_attribute_value_set_boolean:
 * @attr: a #GFileAttributeValue.
 * @value:
 *  
 **/
void
g_file_attribute_value_set_boolean (GFileAttributeValue *attr,
				    gboolean             value)
{
  g_return_if_fail (attr != NULL);

  g_file_attribute_value_clear (attr);
  attr->type = G_FILE_ATTRIBUTE_TYPE_BOOLEAN;
  attr->u.boolean = !!value;
}

/**
 * g_file_attribute_value_set_uint32:
 * @attr: a #GFileAttributeValue.
 * @value:
 * 
 **/ 
void
g_file_attribute_value_set_uint32 (GFileAttributeValue *attr,
				   guint32              value)
{
  g_return_if_fail (attr != NULL);

  g_file_attribute_value_clear (attr);
  attr->type = G_FILE_ATTRIBUTE_TYPE_UINT32;
  attr->u.uint32 = value;
}

/**
 * g_file_attribute_value_set_int32:
 * @attr: a #GFileAttributeValue.
 * @value:
 *  
 **/
void
g_file_attribute_value_set_int32 (GFileAttributeValue *attr,
				  gint32               value)
{
  g_return_if_fail (attr != NULL);

  g_file_attribute_value_clear (attr);
  attr->type = G_FILE_ATTRIBUTE_TYPE_INT32;
  attr->u.int32 = value;
}

/**
 * g_file_attribute_value_set_uint64:
 * @attr: a #GFileAttributeValue.
 * @value:
 * 
 **/
void
g_file_attribute_value_set_uint64 (GFileAttributeValue *attr,
				   guint64              value)
{
  g_return_if_fail (attr != NULL);

  g_file_attribute_value_clear (attr);
  attr->type = G_FILE_ATTRIBUTE_TYPE_UINT64;
  attr->u.uint64 = value;
}

/**
 * g_file_attribute_value_set_int64:
 * @attr: a #GFileAttributeValue.
 * @value: a #gint64 to set the value to.
 *  
 **/
void
g_file_attribute_value_set_int64 (GFileAttributeValue *attr,
				  gint64               value)
{
  g_return_if_fail (attr != NULL);

  g_file_attribute_value_clear (attr);
  attr->type = G_FILE_ATTRIBUTE_TYPE_INT64;
  attr->u.int64 = value;
}

/**
 * g_file_attribute_value_set_object:
 * @attr: a #GFileAttributeValue.
 * @obj: a #GObject.
 *
 * Sets the file attribute @attr to contain the value @obj.
 * The @attr references the object internally.
 * 
 **/
void
g_file_attribute_value_set_object (GFileAttributeValue *attr,
				   GObject             *obj)
{
  g_return_if_fail (attr != NULL);
  g_return_if_fail (obj != NULL);

  g_file_attribute_value_clear (attr);
  attr->type = G_FILE_ATTRIBUTE_TYPE_OBJECT;
  attr->u.obj = g_object_ref (obj);
}

typedef struct {
  GFileAttributeInfoList public;
  GArray *array;
  int ref_count;
} GFileAttributeInfoListPriv;

static void
list_update_public (GFileAttributeInfoListPriv *priv)
{
  priv->public.infos = (GFileAttributeInfo *)priv->array->data;
  priv->public.n_infos = priv->array->len;
}

/**
 * g_file_attribute_info_list_new:
 * 
 * Returns a new #GFileAttributeInfoList.
 **/
GFileAttributeInfoList *
g_file_attribute_info_list_new (void)
{
  GFileAttributeInfoListPriv *priv;

  priv = g_new0 (GFileAttributeInfoListPriv, 1);
  
  priv->ref_count = 1;
  priv->array = g_array_new (TRUE, FALSE, sizeof (GFileAttributeInfo));
  
  list_update_public (priv);
  
  return (GFileAttributeInfoList *)priv;
}

/**
 * g_file_attribute_info_list_dup:
 * @list: a #GFileAttributeInfoList to duplicate.
 * 
 * Returns a duplicate of the given @list. 
 **/
GFileAttributeInfoList *
g_file_attribute_info_list_dup (GFileAttributeInfoList *list)
{
  GFileAttributeInfoListPriv *new;
  int i;
  
  g_return_val_if_fail (list != NULL, NULL);

  new = g_new0 (GFileAttributeInfoListPriv, 1);
  new->ref_count = 1;
  new->array = g_array_new (TRUE, FALSE, sizeof (GFileAttributeInfo));

  g_array_set_size (new->array, list->n_infos);
  list_update_public (new);
  for (i = 0; i < list->n_infos; i++)
    {
      new->public.infos[i].name = g_strdup (list->infos[i].name);
      new->public.infos[i].type = list->infos[i].type;
      new->public.infos[i].flags = list->infos[i].flags;
    }
  
  return (GFileAttributeInfoList *)new;
}

/**
 * g_file_attribute_info_list_ref:
 * @list: a #GFileAttributeInfoList to reference.
 * 
 * Returns: #GFileAttributeInfoList or %NULL on error.
 **/
GFileAttributeInfoList *
g_file_attribute_info_list_ref (GFileAttributeInfoList *list)
{
  GFileAttributeInfoListPriv *priv = (GFileAttributeInfoListPriv *)list;
  
  g_return_val_if_fail (list != NULL, NULL);
  g_return_val_if_fail (priv->ref_count > 0, NULL);
  
  g_atomic_int_inc (&priv->ref_count);
  
  return list;
}

/**
 * g_file_attribute_info_list_unref:
 * @list: The #GFileAttributeInfoList to unreference.
 * 
 * Removes a reference from the given @list. If the reference count
 * falls to zero, the @list is deleted.
 **/
void
g_file_attribute_info_list_unref (GFileAttributeInfoList *list)
{
  GFileAttributeInfoListPriv *priv = (GFileAttributeInfoListPriv *)list;
  int i;
  
  g_return_if_fail (list != NULL);
  g_return_if_fail (priv->ref_count > 0);
  
  if (g_atomic_int_dec_and_test (&priv->ref_count))
    {
      for (i = 0; i < list->n_infos; i++)
        g_free (list->infos[i].name);
      g_array_free (priv->array, TRUE);
    }
}

static int
g_file_attribute_info_list_bsearch (GFileAttributeInfoList *list,
				    const char             *name)
{
  int start, end, mid;
  
  start = 0;
  end = list->n_infos;

  while (start != end)
    {
      mid = start + (end - start) / 2;

      if (strcmp (name, list->infos[mid].name) < 0)
	end = mid;
      else if (strcmp (name, list->infos[mid].name) > 0)
	start = mid + 1;
      else
	return mid;
    }
  return start;
}

/**
 * g_file_attribute_info_list_lookup:
 * @list: a #GFileAttributeInfoList.
 * @name: the name of the attribute to lookup.
 * 
 * Returns: a #GFileAttributeInfo for the @name, or %NULL if an 
 * attribute isn't found.
 **/
const GFileAttributeInfo *
g_file_attribute_info_list_lookup (GFileAttributeInfoList *list,
				   const char             *name)
{
  int i;
  
  g_return_val_if_fail (list != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);
  
  i = g_file_attribute_info_list_bsearch (list, name);

  if (i < list->n_infos && strcmp (list->infos[i].name, name) == 0)
    return &list->infos[i];
  
  return NULL;
}

/**
 * g_file_attribute_info_list_add:
 * @list: a #GFileAttributeInfoList.
 * @name: the name of the attribute to add.
 * @type: the #GFileAttributeType for the attribute.
 * @flags: #GFileAttributeFlags for the attribute.
 * 
 * Adds a new attribute with @name to the @list, setting
 * its @type and @flags. 
 * 
 **/
void
g_file_attribute_info_list_add    (GFileAttributeInfoList *list,
				   const char             *name,
				   GFileAttributeType      type,
				   GFileAttributeFlags     flags)
{
  GFileAttributeInfoListPriv *priv = (GFileAttributeInfoListPriv *)list;
  GFileAttributeInfo info;
  int i;
  
  g_return_if_fail (list != NULL);
  g_return_if_fail (name != NULL);

  i = g_file_attribute_info_list_bsearch (list, name);

  if (i < list->n_infos && strcmp (list->infos[i].name, name) == 0)
    {
      list->infos[i].type = type;
      return;
    }

  info.name = g_strdup (name);
  info.type = type;
  info.flags = flags;
  g_array_insert_vals (priv->array, i, &info, 1);

  list_update_public (priv);
}
