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

#if !defined (__GIO_GIO_H_INSIDE__) && !defined (GIO_COMPILATION)
#error "Only <gio/gio.h> can be included directly."
#endif

#ifndef __G_FILE_ATTRIBUTE_H__
#define __G_FILE_ATTRIBUTE_H__

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * GFileAttributeType:
 * @G_FILE_ATTRIBUTE_TYPE_INVALID: indicates an invalid or uninitalized type.
 * @G_FILE_ATTRIBUTE_TYPE_STRING: a null terminated UTF8 string.
 * @G_FILE_ATTRIBUTE_TYPE_BYTE_STRING: a zero terminated string of non-zero bytes.
 * @G_FILE_ATTRIBUTE_TYPE_BOOLEAN: a boolean value.
 * @G_FILE_ATTRIBUTE_TYPE_UINT32: an unsigned 4-byte/32-bit integer.
 * @G_FILE_ATTRIBUTE_TYPE_INT32: a signed 4-byte/32-bit integer.
 * @G_FILE_ATTRIBUTE_TYPE_UINT64: an unsigned 8-byte/64-bit integer.
 * @G_FILE_ATTRIBUTE_TYPE_INT64: a signed 8-byte/64-bit integer.
 * @G_FILE_ATTRIBUTE_TYPE_OBJECT: a #GObject.
 * 
 * The data type for #GFileAttributeValue<!-- -->s. 
 **/ 
typedef enum {
  G_FILE_ATTRIBUTE_TYPE_INVALID = 0,
  G_FILE_ATTRIBUTE_TYPE_STRING,
  G_FILE_ATTRIBUTE_TYPE_BYTE_STRING, /* zero terminated string of non-zero bytes */
  G_FILE_ATTRIBUTE_TYPE_BOOLEAN,
  G_FILE_ATTRIBUTE_TYPE_UINT32,
  G_FILE_ATTRIBUTE_TYPE_INT32,
  G_FILE_ATTRIBUTE_TYPE_UINT64,
  G_FILE_ATTRIBUTE_TYPE_INT64,
  G_FILE_ATTRIBUTE_TYPE_OBJECT
} GFileAttributeType;

/**
 * GFileAttributeFlags:
 * @G_FILE_ATTRIBUTE_FLAGS_NONE: no flags set.
 * @G_FILE_ATTRIBUTE_FLAGS_COPY_WITH_FILE: copy the attribute values when the file is copied.
 * @G_FILE_ATTRIBUTE_FLAGS_COPY_WHEN_MOVED: copy the attribute values when the file is moved.
 * 
 * A flag specifying the behaviour of an attribute.
 **/
typedef enum {
  G_FILE_ATTRIBUTE_FLAGS_NONE = 0,
  G_FILE_ATTRIBUTE_FLAGS_COPY_WITH_FILE = 1 << 0,
  G_FILE_ATTRIBUTE_FLAGS_COPY_WHEN_MOVED = 1 << 1
} GFileAttributeFlags;

/**
 * GFileAttributeStatus:
 * @G_FILE_ATTRIBUTE_STATUS_UNSET: Attribute value is unset (empty).
 * @G_FILE_ATTRIBUTE_STATUS_SET: Attribute value is set.
 * @G_FILE_ATTRIBUTE_STATUS_ERROR_SETTING: Indicates an error in setting the value.
 * 
 * Used by g_file_set_attributes_from_info() when setting file attributes.
 **/
typedef enum {
  G_FILE_ATTRIBUTE_STATUS_UNSET = 0,
  G_FILE_ATTRIBUTE_STATUS_SET,
  G_FILE_ATTRIBUTE_STATUS_ERROR_SETTING
} GFileAttributeStatus;

#define G_FILE_ATTRIBUTE_VALUE_INIT {0}

/**
 * GFileAttributeValue:
 * @type: a #GFileAttributeType.
 * @status: a #GFileAttributeStatus.
 *
 * Contains the value data for the Key-Value pair.
 **/
typedef struct  {
  GFileAttributeType type : 8;
  GFileAttributeStatus status : 8;
  union {
    gboolean boolean;
    gint32 int32;
    guint32 uint32;
    gint64 int64;
    guint64 uint64;
    char *string;
    GQuark quark;
    GObject *obj;
  } u;
} GFileAttributeValue;

/**
 * GFileAttributeInfo:
 * @name: the name of the attribute.
 * @type: the #GFileAttributeType type of the attribute.
 * @flags: a set of #GFileAttributeFlags.
 * 
 * Information about a specific attribute. 
 **/
typedef struct {
  char *name;
  GFileAttributeType type;
  GFileAttributeFlags flags;
} GFileAttributeInfo;

/**
 * GFileAttributeInfoList:
 * @infos: an array of #GFileAttributeInfo<!-- -->s.
 * @n_infos: the number of values in the array.
 * 
 * Acts as a lightweight registry for possible valid file attributes.
 * The registry stores Key-Value pair formats as #GFileAttributeInfo<!-- -->s.
 **/
typedef struct {
  GFileAttributeInfo *infos;
  int n_infos;
} GFileAttributeInfoList;

GFileAttributeValue *g_file_attribute_value_new             (void);
void                 g_file_attribute_value_free            (GFileAttributeValue *attr);
void                 g_file_attribute_value_clear           (GFileAttributeValue *attr);
void                 g_file_attribute_value_set             (GFileAttributeValue *attr,
							     const GFileAttributeValue *new_value);
GFileAttributeValue *g_file_attribute_value_dup             (const GFileAttributeValue *other);

char *               g_file_attribute_value_as_string       (const GFileAttributeValue *attr);

const char *         g_file_attribute_value_get_string      (const GFileAttributeValue *attr);
const char *         g_file_attribute_value_get_byte_string (const GFileAttributeValue *attr);
gboolean             g_file_attribute_value_get_boolean     (const GFileAttributeValue *attr);
guint32              g_file_attribute_value_get_uint32      (const GFileAttributeValue *attr);
gint32               g_file_attribute_value_get_int32       (const GFileAttributeValue *attr);
guint64              g_file_attribute_value_get_uint64      (const GFileAttributeValue *attr);
gint64               g_file_attribute_value_get_int64       (const GFileAttributeValue *attr);
GObject *            g_file_attribute_value_get_object      (const GFileAttributeValue *attr);

void                 g_file_attribute_value_set_string      (GFileAttributeValue *attr,
							     const char          *string);
void                 g_file_attribute_value_set_byte_string (GFileAttributeValue *attr,
							     const char          *string);
void                 g_file_attribute_value_set_boolean     (GFileAttributeValue *attr,
							     gboolean             value);
void                 g_file_attribute_value_set_uint32      (GFileAttributeValue *attr,
							     guint32              value);
void                 g_file_attribute_value_set_int32       (GFileAttributeValue *attr,
							     gint32               value);
void                 g_file_attribute_value_set_uint64      (GFileAttributeValue *attr,
							     guint64              value);
void                 g_file_attribute_value_set_int64       (GFileAttributeValue *attr,
							     gint64               value);
void                 g_file_attribute_value_set_object      (GFileAttributeValue *attr,
							     GObject             *obj);

GFileAttributeInfoList *  g_file_attribute_info_list_new    (void);
GFileAttributeInfoList *  g_file_attribute_info_list_ref    (GFileAttributeInfoList *list);
void                      g_file_attribute_info_list_unref  (GFileAttributeInfoList *list);
GFileAttributeInfoList *  g_file_attribute_info_list_dup    (GFileAttributeInfoList *list);
const GFileAttributeInfo *g_file_attribute_info_list_lookup (GFileAttributeInfoList *list,
							     const char             *name);
void                      g_file_attribute_info_list_add    (GFileAttributeInfoList *list,
							     const char             *name,
							     GFileAttributeType      type,
							     GFileAttributeFlags     flags);

G_END_DECLS


#endif /* __G_FILE_INFO_H__ */
