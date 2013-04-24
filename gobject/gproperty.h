/* gproperty.h: Property definitions for GObject
 *
 * Copyright © 2013  Emmanuele Bassi
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

#ifndef __G_PROPERTY_H__
#define __G_PROPERTY_H__

#if !defined (__GLIB_GOBJECT_H_INSIDE__) && !defined (GOBJECT_COMPILATION)
#error "Only <glib-object.h> can be included directly."
#endif

#include <glib.h>
#include <gobject/gparam.h>
#include <gobject/gobject.h>

G_BEGIN_DECLS

#define G_TYPE_PROPERTY         (g_property_get_type ())
#define G_PROPERTY(obj)         (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_PROPERTY, GProperty))
#define G_IS_PROPERTY(obj)      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), G_TYPE_PROPERTY))

/**
 * GProperty:
 *
 * The <structname>GProperty</structname> structure is an opaque structure
 * whose members cannot be directly accessed.
 *
 * Since: 2.38
 */
typedef struct _GProperty       GProperty;

/**
 * GPropertyFlags:
 * @G_PROPERTY_READABLE: Whether the property is readable
 * @G_PROPERTY_WRITABLE: Whether the property is writable
 * @G_PROPERTY_READWRITE: Whether the property is readable and writable
 * @G_PROPERTY_COPY_SET: Whether the property will make a copy or
 *   take a reference when being set to a new value
 * @G_PROPERTY_COPY_GET: Whether the property will make a copy or
 *   take a reference when the value is being retrieved
 * @G_PROPERTY_COPY: Whether the property will make a copy, or take a
 *   reference, of the new value being set, and return a copy, or
 *   increase the reference count, of the value being retrieved
 * @G_PROPERTY_DEPRECATED: Whether the property is deprecated and should
 *   not be accessed in newly written code.
 * @G_PROPERTY_CONSTRUCT_ONLY: Whether the property is meant to be set
 *   only during construction. Implies %G_PROPERTY_WRITABLE.
 *
 * Flags for properties declared using #GProperty and relative macros.
 *
 * This enumeration might be extended at later date.
 *
 * Since: 2.38
 */
typedef enum {
  G_PROPERTY_READABLE       = 1 << 0,
  G_PROPERTY_WRITABLE       = 1 << 1,
  G_PROPERTY_READWRITE      = (G_PROPERTY_READABLE | G_PROPERTY_WRITABLE),

  G_PROPERTY_COPY_SET       = 1 << 2,
  G_PROPERTY_COPY_GET       = 1 << 3,
  G_PROPERTY_COPY           = (G_PROPERTY_COPY_SET | G_PROPERTY_COPY_GET),

  G_PROPERTY_DEPRECATED     = 1 << 4,
  G_PROPERTY_CONSTRUCT_ONLY = 1 << 5
} GPropertyFlags;

GLIB_AVAILABLE_IN_2_38
GType           g_property_get_type             (void) G_GNUC_CONST;

/* general purpose API */
GLIB_AVAILABLE_IN_2_38
const gchar *   g_property_canonicalize_name            (const char   *name);

GLIB_AVAILABLE_IN_2_38
GType           g_property_get_value_type               (GProperty    *property);

GLIB_AVAILABLE_IN_2_38
gboolean        g_property_is_writable                  (GProperty    *property);
GLIB_AVAILABLE_IN_2_38
gboolean        g_property_is_readable                  (GProperty    *property);
GLIB_AVAILABLE_IN_2_38
gboolean        g_property_is_deprecated                (GProperty    *property);
GLIB_AVAILABLE_IN_2_38
gboolean        g_property_is_copy_set                  (GProperty    *property);
GLIB_AVAILABLE_IN_2_38
gboolean        g_property_is_copy_get                  (GProperty    *property);
GLIB_AVAILABLE_IN_2_38
gboolean        g_property_is_construct_only            (GProperty    *property);

GLIB_AVAILABLE_IN_2_38
void            g_property_set_range_values             (GProperty    *property,
                                                         const GValue *min_value,
                                                         const GValue *max_value);
GLIB_AVAILABLE_IN_2_38
gboolean        g_property_get_range_values             (GProperty    *property,
                                                         GValue       *min_value,
                                                         GValue       *max_value);
GLIB_AVAILABLE_IN_2_38
void            g_property_set_range                    (GProperty    *property,
                                                         ...);
GLIB_AVAILABLE_IN_2_38
gboolean        g_property_get_range                    (GProperty    *property,
                                                         ...);
GLIB_AVAILABLE_IN_2_38
void            g_property_set_default_value            (GProperty    *property,
                                                         const GValue *value);
GLIB_AVAILABLE_IN_2_38
void            g_property_get_default_value            (GProperty    *property,
                                                         gpointer      gobject,
                                                         GValue       *value);
GLIB_AVAILABLE_IN_2_38
void            g_property_override_default_value       (GProperty    *property,
                                                         GType         gtype,
                                                         const GValue *value);
GLIB_AVAILABLE_IN_2_38
void            g_property_set_default                  (GProperty    *property,
                                                         ...);
GLIB_AVAILABLE_IN_2_38
void            g_property_get_default                  (GProperty    *property,
                                                         gpointer      gobject,
                                                         ...);
GLIB_AVAILABLE_IN_2_38
void            g_property_override_default             (GProperty    *property,
                                                         GType         gtype,
                                                         ...);
GLIB_AVAILABLE_IN_2_38
void            g_property_init_default                 (GProperty    *property,
                                                         gpointer      gobject);
GLIB_AVAILABLE_IN_2_38
void            g_property_set_prerequisite             (GProperty    *property,
                                                         ...);

GLIB_AVAILABLE_IN_2_38
gboolean        g_property_validate                     (GProperty    *property,
                                                         ...);
GLIB_AVAILABLE_IN_2_38
gboolean        g_property_validate_value               (GProperty    *property,
                                                         GValue       *value);

GLIB_AVAILABLE_IN_2_38
gboolean        g_property_set_value                    (GProperty    *property,
                                                         gpointer      gobject,
                                                         const GValue *value);
GLIB_AVAILABLE_IN_2_38
void            g_property_get_value                    (GProperty    *property,
                                                         gpointer      gobject,
                                                         GValue       *value);
GLIB_AVAILABLE_IN_2_38
gboolean        g_property_set                          (GProperty    *property,
                                                         gpointer      gobject,
                                                         ...);
GLIB_AVAILABLE_IN_2_38
gboolean        g_property_get                          (GProperty    *property,
                                                         gpointer      gobject,
                                                         ...);

/**
 * GPropertyCollectFlags:
 * @G_PROPERTY_COLLECT_NONE: No flags
 * @G_PROPERTY_COLLECT_COPY: Make a copy when collecting pointer
 *   locations for boxed and string values
 * @G_PROPERTY_COLLECT_REF: Take a reference when collecting
 *   pointer locations for object values
 *
 * Flags to pass to g_property_collect() and g_property_lcopy().
 *
 * Since: 2.38
 */
typedef enum { /*< prefix=G_PROPERTY_COLLECT >*/
  G_PROPERTY_COLLECT_NONE = 0,

  G_PROPERTY_COLLECT_COPY = 1 << 0,
  G_PROPERTY_COLLECT_REF  = 1 << 1
} GPropertyCollectFlags;

GLIB_AVAILABLE_IN_2_38
gboolean        g_property_set_va       (GProperty             *property,
                                         gpointer               gobject,
                                         GPropertyCollectFlags  flags,
                                         va_list               *app);
GLIB_AVAILABLE_IN_2_38
gboolean        g_property_get_va       (GProperty             *property,
                                         gpointer               gobject,
                                         GPropertyCollectFlags  flags,
                                         va_list               *app);

/* per-type specific accessors */
typedef gboolean      (* GPropertyBooleanSet)   (gpointer       gobject,
                                                 gboolean       value);
typedef gboolean      (* GPropertyBooleanGet)   (gpointer       gobject);

typedef gboolean      (* GPropertyIntSet)       (gpointer       gobject,
                                                 gint           value);
typedef gint          (* GPropertyIntGet)       (gpointer       gobject);

typedef gboolean      (* GPropertyInt8Set)      (gpointer       gobject,
                                                 gint8          value);
typedef gint8         (* GPropertyInt8Get)      (gpointer       gobject);

typedef gboolean      (* GPropertyInt16Set)     (gpointer       gobject,
                                                 gint16         value);
typedef gint16        (* GPropertyInt16Get)     (gpointer       gobject);

typedef gboolean      (* GPropertyInt32Set)     (gpointer       gobject,
                                                 gint32         value);
typedef gint32        (* GPropertyInt32Get)     (gpointer       gobject);

typedef gboolean      (* GPropertyInt64Set)     (gpointer       gobject,
                                                 gint64         value);
typedef gint64        (* GPropertyInt64Get)     (gpointer       gobject);

typedef gboolean      (* GPropertyLongSet)      (gpointer       gobject,
                                                 glong          value);
typedef glong         (* GPropertyLongGet)      (gpointer       gobject);

typedef gboolean      (* GPropertyUIntSet)      (gpointer       gobject,
                                                 guint          value);
typedef guint         (* GPropertyUIntGet)      (gpointer       gobject);

typedef gboolean      (* GPropertyUInt8Set)     (gpointer       gobject,
                                                 guint8         value);
typedef guint8        (* GPropertyUInt8Get)     (gpointer       gobject);

typedef gboolean      (* GPropertyUInt16Set)    (gpointer       gobject,
                                                 guint16        value);
typedef guint16       (* GPropertyUInt16Get)    (gpointer       gobject);

typedef gboolean      (* GPropertyUInt32Set)    (gpointer       gobject,
                                                 guint32        value);
typedef guint32       (* GPropertyUInt32Get)    (gpointer       gobject);

typedef gboolean      (* GPropertyUInt64Set)    (gpointer       gobject,
                                                 guint64        value);
typedef guint64       (* GPropertyUInt64Get)    (gpointer       gobject);

typedef gboolean      (* GPropertyULongSet)     (gpointer       gobject,
                                                 gulong         value);
typedef gulong        (* GPropertyULongGet)     (gpointer       gobject);

typedef gboolean      (* GPropertyEnumSet)      (gpointer       gobject,
                                                 gint           value);
typedef gint          (* GPropertyEnumGet)      (gpointer       gobject);

typedef gboolean      (* GPropertyFlagsSet)     (gpointer       gobject,
                                                 guint          value);
typedef guint         (* GPropertyFlagsGet)     (gpointer       gobject);

typedef gboolean      (* GPropertyFloatSet)     (gpointer       gobject,
                                                 gfloat         value);
typedef gfloat        (* GPropertyFloatGet)     (gpointer       gobject);

typedef gboolean      (* GPropertyDoubleSet)    (gpointer       gobject,
                                                 gdouble        value);
typedef gdouble       (* GPropertyDoubleGet)    (gpointer       gobject);

typedef gboolean      (* GPropertyStringSet)    (gpointer       gobject,
                                                 const char    *value);
typedef const char *  (* GPropertyStringGet)    (gpointer       gobject);

typedef gboolean      (* GPropertyBoxedSet)     (gpointer       gobject,
                                                 gpointer       value);
typedef gpointer      (* GPropertyBoxedGet)     (gpointer       gobject);

typedef gboolean      (* GPropertyObjectSet)    (gpointer       gobject,
                                                 gpointer       value);
typedef gpointer      (* GPropertyObjectGet)    (gpointer       gobject);

typedef gboolean      (* GPropertyPointerSet)   (gpointer       gobject,
                                                 gpointer       value);
typedef gpointer      (* GPropertyPointerGet)   (gpointer       gobject);

/* per-type specific constructors */
GLIB_AVAILABLE_IN_2_38
GParamSpec *    g_boolean_property_new  (const gchar         *name,
                                         GPropertyFlags       flags,
                                         gssize               field_offset,
                                         GPropertyBooleanSet  setter,
                                         GPropertyBooleanGet  getter);

GLIB_AVAILABLE_IN_2_38
GParamSpec *    g_int_property_new      (const gchar         *name,
                                         GPropertyFlags       flags,
                                         gssize               field_offset,
                                         GPropertyIntSet      setter,
                                         GPropertyIntGet      getter);
GLIB_AVAILABLE_IN_2_38
GParamSpec *    g_int8_property_new     (const gchar         *name,
                                         GPropertyFlags       flags,
                                         gssize               field_offset,
                                         GPropertyInt8Set     setter,
                                         GPropertyInt8Get     getter);
GLIB_AVAILABLE_IN_2_38
GParamSpec *    g_int16_property_new    (const gchar         *name,
                                         GPropertyFlags       flags,
                                         gssize               field_offset,
                                         GPropertyInt16Set    setter,
                                         GPropertyInt16Get    getter);
GLIB_AVAILABLE_IN_2_38
GParamSpec *    g_int32_property_new    (const gchar         *name,
                                         GPropertyFlags       flags,
                                         gssize               field_offset,
                                         GPropertyInt32Set    setter,
                                         GPropertyInt32Get    getter);
GLIB_AVAILABLE_IN_2_38
GParamSpec *    g_int64_property_new    (const gchar         *name,
                                         GPropertyFlags       flags,
                                         gssize               field_offset,
                                         GPropertyInt64Set    setter,
                                         GPropertyInt64Get    getter);
GLIB_AVAILABLE_IN_2_38
GParamSpec *    g_long_property_new     (const gchar         *name,
                                         GPropertyFlags       flags,
                                         gssize               field_offset,
                                         GPropertyLongSet     setter,
                                         GPropertyLongGet     getter);

GLIB_AVAILABLE_IN_2_38
GParamSpec *    g_uint_property_new     (const gchar         *name,
                                         GPropertyFlags       flags,
                                         gssize               field_offset,
                                         GPropertyUIntSet     setter,
                                         GPropertyUIntGet     getter);
GLIB_AVAILABLE_IN_2_38
GParamSpec *    g_uint8_property_new    (const gchar         *name,
                                         GPropertyFlags       flags,
                                         gssize               field_offset,
                                         GPropertyUInt8Set    setter,
                                         GPropertyUInt8Get    getter);
GLIB_AVAILABLE_IN_2_38
GParamSpec *    g_uint16_property_new   (const gchar         *name,
                                         GPropertyFlags       flags,
                                         gssize               field_offset,
                                         GPropertyUInt16Set   setter,
                                         GPropertyUInt16Get   getter);
GLIB_AVAILABLE_IN_2_38
GParamSpec *    g_uint32_property_new   (const gchar         *name,
                                         GPropertyFlags       flags,
                                         gssize               field_offset,
                                         GPropertyUInt32Set   setter,
                                         GPropertyUInt32Get   getter);
GLIB_AVAILABLE_IN_2_38
GParamSpec *    g_uint64_property_new   (const gchar         *name,
                                         GPropertyFlags       flags,
                                         gssize               field_offset,
                                         GPropertyUInt64Set   setter,
                                         GPropertyUInt64Get   getter);
GLIB_AVAILABLE_IN_2_38
GParamSpec *    g_ulong_property_new    (const gchar         *name,
                                         GPropertyFlags       flags,
                                         gssize               field_offset,
                                         GPropertyULongSet    setter,
                                         GPropertyULongGet    getter);

#define g_char_property_new     g_int8_property_new
#define g_uchar_property_new    g_uint8_property_new
#define g_unichar_property_new  g_uint32_property_new

GLIB_AVAILABLE_IN_2_38
GParamSpec *    g_enum_property_new     (const gchar         *name,
                                         GPropertyFlags       flags,
                                         gssize               field_offset,
                                         GPropertyEnumSet     setter,
                                         GPropertyEnumGet     getter);
GLIB_AVAILABLE_IN_2_38
GParamSpec *    g_flags_property_new    (const gchar         *name,
                                         GPropertyFlags       flags,
                                         gssize               field_offset,
                                         GPropertyFlagsSet    setter,
                                         GPropertyFlagsGet    getter);

GLIB_AVAILABLE_IN_2_38
GParamSpec *    g_float_property_new    (const gchar         *name,
                                         GPropertyFlags       flags,
                                         gssize               field_offset,
                                         GPropertyFloatSet    setter,
                                         GPropertyFloatGet    getter);
GLIB_AVAILABLE_IN_2_38
GParamSpec *    g_double_property_new   (const gchar         *name,
                                         GPropertyFlags       flags,
                                         gssize               field_offset,
                                         GPropertyDoubleSet   setter,
                                         GPropertyDoubleGet   getter);

GLIB_AVAILABLE_IN_2_38
GParamSpec *    g_string_property_new   (const gchar         *name,
                                         GPropertyFlags       flags,
                                         gssize               field_offset,
                                         GPropertyStringSet   setter,
                                         GPropertyStringGet   getter);

GLIB_AVAILABLE_IN_2_38
GParamSpec *    g_boxed_property_new    (const gchar         *name,
                                         GPropertyFlags       flags,
                                         gssize               field_offset,
                                         GPropertyBoxedSet    setter,
                                         GPropertyBoxedGet    getter);

GLIB_AVAILABLE_IN_2_38
GParamSpec *    g_object_property_new   (const gchar         *name,
                                         GPropertyFlags       flags,
                                         gssize               field_offset,
                                         GPropertyObjectSet   setter,
                                         GPropertyObjectGet   getter);

GLIB_AVAILABLE_IN_2_38
GParamSpec *    g_pointer_property_new  (const gchar         *name,
                                         GPropertyFlags       flags,
                                         gssize               field_offset,
                                         GPropertyPointerSet  setter,
                                         GPropertyPointerGet  getter);


/* private API */
void            _g_property_set_installed       (GProperty           *property,
                                                 gpointer             g_class,
                                                 GType                class_gtype);

G_END_DECLS

#endif /* __G_PROPERTY_H__ */
