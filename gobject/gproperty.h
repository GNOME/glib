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
void            g_property_set_installed       (GProperty *property,
                                                gpointer   g_class,
                                                GType      class_gtype);
void            g_property_init_default        (GProperty *property,
                                                gpointer   object);

/* property generation */

/**
 * G_PROPERTY_DESCRIBE:
 * @p_nick: a human readable, translatable name for the property
 * @p_blurb: a human readable, translatable description for the property
 *
 * Sets the property nick and blurb using two static strings.
 *
 * This macro can only be called inside %G_DEFINE_PROPERTY_EXTENDED.
 *
 * Since: 2.38
 */
#define G_PROPERTY_DESCRIBE(p_nick, p_blurb) \
  { \
    GParamSpec *__g_param_spec = (GParamSpec *) g_property; \
    g_param_spec_set_static_nick (__g_param_spec, p_nick); \
    g_param_spec_set_static_blurb (__g_param_spec, p_blurb); \
  }

/**
 * G_PROPERTY_DEFAULT:
 * @p_val: the default value of the property
 *
 * Sets the default value for the property.
 *
 * This macro can only be called inside %G_DEFINE_PROPERTY_EXTENDED.
 *
 * Since: 2.38
 */
#define G_PROPERTY_DEFAULT(p_val) \
  g_property_set_default (g_property, p_val);

/**
 * G_PROPERTY_RANGE:
 * @p_min: the minimum value of the valid range for the property
 * @p_max: the maximum value of the valid range for the property
 *
 * Sets the range of valid values for the property.
 *
 * This macro can only be called inside %G_DEFINE_PROPERTY_EXTENDED.
 *
 * Since: 2.38
 */
#define G_PROPERTY_RANGE(p_min, p_max) \
  g_property_set_range (g_property, p_min, p_max);

/**
 * G_PROPERTY_PREREQUISITE:
 * @type: the prerequisite type for the property
 *
 * Sets the prerequisite type for enumeration, flags, boxed,
 * and object properties.
 *
 * This macro can only be called inside %G_DEFINE_PROPERTY_EXTENDED.
 *
 * Since: 2.38
 */
#define G_PROPERTY_PREREQUISITE(type) \
  g_property_set_prerequisite (g_property, type);

#define _G_DEFINE_PROPERTIES_BEGIN() \
  { \
    GPtrArray *g_define_properties = g_ptr_array_new (); \
    g_ptr_array_add (g_define_properties, NULL);

#define _G_DEFINE_PROPERTIES_END(T_N, t_n, g_class) \
    t_n##_properties_len = g_define_properties->len; \
    t_n##_properties = (GParamSpec **) g_ptr_array_free (g_define_properties, FALSE); \
    g_object_class_install_properties (G_OBJECT_CLASS (g_class), \
                                       t_n##_properties_len, \
                                       t_n##_properties); \
  }

/**
 * G_DEFINE_PROPERTIES:
 * @T_N: the name of the type, in CamelCase
 * @t_n: the name of the type, in lower case, with spaces replaced by underscores
 * @g_class: a pointer to the class structure of the type
 * @_C_: a list of G_DEFINE_PROPERTY_EXTENDED calls
 *
 * Defines a list of properties and installs them on the class.
 *
 * Note that this macro can only be used with types defined using
 * G_DEFINE_TYPE_* macros, as it depends on variables defined by
 * those macros.
 *
 * Since: 2.38
 */
#define G_DEFINE_PROPERTIES(T_N, t_n, g_class, _C_) \
  _G_DEFINE_PROPERTIES_BEGIN() \
  { _C_; } \
  _G_DEFINE_PROPERTIES_END(T_N, t_n, g_class)

#define _G_DEFINE_DIRECT_PROPERTY_EXTENDED_BEGIN(T_N, c_type, name, flags) \
{ \
  GProperty *g_property = (GProperty *) \
    g_##c_type##_property_new (#name, flags, \
                               G_PRIVATE_OFFSET (T_N, name), \
                               NULL, \
                               NULL);
#define _G_DEFINE_PROPERTY_EXTENDED_BEGIN(T_N, c_type, name, offset, setterFunc, getterFunc, flags) \
{ \
  GProperty *g_property = (GProperty *) \
    g_##c_type##_property_new (#name, flags, \
                               offset, \
                               setterFunc, \
                               getterFunc);
#define _G_DEFINE_PROPERTY_EXTENDED_END \
  g_ptr_array_add (g_define_properties, g_property); \
}

/**
 * G_DEFINE_PROPERTY_EXTENDED:
 * @T_N: the name of the type, in CamelCase
 * @c_type: the C type of the property, in lower case, minus the "g" if the type
 *   is defined by GLib; for instance "int" for "gint", "uint8" for "guint8",
 *   "boolean" for "gboolean"; strings stored in a gchar* are defined as "string"
 * @name: the name of the property, in lower case, with '-' replaced by '_'
 * @offset: the offset of the field in the structure that stores the property, or 0
 * @setterFunc: the explicit setter function, or %NULL for direct access
 * @getterFunc: the explicit getter function, or %NULL for direct access
 * @flags: #GPropertyFlags for the property
 * @_C_: custom code to be called after the property has been defined; the
 *   GProperty instance is available under the "g_property" variable
 *
 * The most generic property definition macro.
 *
 * |[
 *   G_DEFINE_PROPERTY_EXTENDED (GtkGadget,
 *                               int,
 *                               width,
 *                               G_PRIVATE_OFFSET (GtkGadget, width),
 *                               NULL, NULL,
 *                               G_PROPERTY_READWRITE,
 *                               G_PROPERTY_RANGE (0, G_MAXINT))
 * ]|
 * expands to
 * |[
 *   {
 *     GProperty *g_property =
 *       g_int_property_new ("width", G_PROPERTY_READWRITE,
 *                           G_PRIVATE_OFFSET (GtkGadget, width),
 *                           NULL, NULL);
 *     g_property_set_range (g_property, 0, G_MAXINT);
 *     gtk_gadget_properties[PROP_GtkGadget_width] = g_property;
 *   }
 * ]|
 *
 * This macro should only be used with G_DEFINE_PROPERTIES() as it depends
 * on variables and functions defined by that macro.
 *
 * Since: 2.38
 */
#define G_DEFINE_PROPERTY_EXTENDED(T_N, c_type, name, offset, setterFunc, getterFunc, flags, _C_) \
  _G_DEFINE_PROPERTY_EXTENDED_BEGIN (T_N, c_type, name, offset, setterFunc, getterFunc, flags) \
  { _C_; } \
  _G_DEFINE_PROPERTY_EXTENDED_END

/**
 * G_DEFINE_PROPERTY_WITH_CODE:
 * @T_N: the name of the type, in CamelCase
 * @c_type: the C type of the property, in lower case, minus the "g" if the type
 *   is defined by GLib; for instance "int" for "gint", "uint8" for "guint8",
 *   "boolean" for "gboolean"; strings stored in a gchar* are defined as "string"
 * @name: the name of the property, in lower case, with '-' replaced by '_'; the
 *   name of the property must exist as a member of the per-instance private data
 *   structure of the type name
 * @flags: #GPropertyFlags for the property
 * @_C_: custom code to be called after the property has been defined; the
 *   GProperty instance is available under the "g_property" variable
 *
 * A variant of G_DEFINE_PROPERTY_EXTENDED() that only allows properties
 * with direct access, stored on the per-instance private data structure
 * for the given type.
 *
 * Since: 2.38
 */
#define G_DEFINE_PROPERTY_WITH_CODE(T_N, c_type, name, flags, _C_) \
  _G_DEFINE_DIRECT_PROPERTY_EXTENDED_BEGIN (T_N, c_type, name, flags) \
  { _C_; } \
  _G_DEFINE_PROPERTY_EXTENDED_END

/**
 * G_DEFINE_PROPERTY_WITH_DEFAULT:
 * @T_N: the name of the type, in CamelCase
 * @c_type: the C type of the property, in lower case, minus the "g" if the type
 *   is defined by GLib; for instance "int" for "gint", "uint8" for "guint8",
 *   "boolean" for "gboolean"; strings stored in a gchar* are defined as "string"
 * @name: the name of the property, in lower case, with '-' replaced by '_'; the
 *   name of the property must exist as a member of the per-instance private data
 *   structure of the type name
 * @flags: #GPropertyFlags for the property
 * @defVal: the default value of the property
 *
 * A convenience macro for defining a direct access property with a default
 * value.
 *
 * See G_DEFINE_PROPERTY_WITH_CODE() and G_PROPERTY_DEFAULT().
 *
 * Since: 2.38
 */
#define G_DEFINE_PROPERTY_WITH_DEFAULT(T_N, c_type, name, flags, defVal) \
  _G_DEFINE_DIRECT_PROPERTY_EXTENDED_BEGIN (T_N, c_type, name, flags) \
  G_PROPERTY_DEFAULT (defVal) \
  _G_DEFINE_PROPERTY_EXTENDED_END

/**
 * G_DEFINE_PROPERTY_WITH_RANGE:
 * @T_N: the name of the type, in CamelCase
 * @c_type: the C type of the property, in lower case, minus the "g" if the type
 *   is defined by GLib; for instance "int" for "gint", "uint8" for "guint8",
 *   "boolean" for "gboolean"; strings stored in a gchar* are defined as "string"
 * @name: the name of the property, in lower case, with '-' replaced by '_'; the
 *   name of the property must exist as a member of the per-instance private data
 *   structure of the type name
 * @flags: #GPropertyFlags for the property
 * @minVal: the minimum value of the property
 * @maxVal: the maximum value of the property
 *
 * A convenience macro for defining a direct access property with a range
 * of valid values.
 *
 * See G_DEFINE_PROPERTY_WITH_CODE() and G_PROPERTY_RANGE().
 *
 * Since: 2.38
 */
#define G_DEFINE_PROPERTY_WITH_RANGE(T_N, c_type, name, flags, minVal, maxVal) \
  _G_DEFINE_DIRECT_PROPERTY_EXTENDED_BEGIN (T_N, c_type, name, flags) \
  G_PROPERTY_RANGE (minVal, maxVal) \
  _G_DEFINE_PROPERTY_EXTENDED_END

/**
 * G_DEFINE_PROPERTY:
 * @T_N: the name of the type, in CamelCase
 * @c_type: the C type of the property, in lower case, minus the "g" if the type
 *   is defined by GLib; for instance "int" for "gint", "uint8" for "guint8",
 *   "boolean" for "gboolean"; strings stored in a gchar* are defined as "string"
 * @name: the name of the property, in lower case, with '-' replaced by '_'; the
 *   name of the property must exist as a member of the per-instance private data
 *   structure of the type name
 * @flags: #GPropertyFlags for the property
 *
 * A convenience macro for defining a direct access property.
 *
 * See G_DEFINE_PROPERTY_WITH_CODE() and G_DEFINE_PROPERTY_EXTENDED() if
 * you need more flexibility.
 *
 * Since: 2.38
 */
#define G_DEFINE_PROPERTY(T_N, c_type, name, flags) \
  _G_DEFINE_DIRECT_PROPERTY_EXTENDED_BEGIN (T_N, c_type, name, flags) \
  _G_DEFINE_PROPERTY_EXTENDED_END

/* accessors generation */
#define _G_DECLARE_PROPERTY_GETTER(T_n, t_n, f_t, f_n)  f_t t_n##_get_##f_n (T_n *self)

#define _G_DEFINE_PROPERTY_DIRECT_GETTER_BEGIN(T_n, t_n, f_t, f_n) \
{ \
  T_n##Private *priv; \
  f_t retval; \
\
  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE_TYPE (self, t_n##_get_type ()), (f_t) 0); \
\
  priv = t_n##_get_instance_private (self); \
  retval = priv->f_n; \
\
  { /* custom code follows */

#define _G_DEFINE_PROPERTY_INDIRECT_GETTER_BEGIN(T_n, t_n, f_t, f_n) \
{ \
  GProperty *g_property = NULL; \
  f_t retval; \
\
  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE_TYPE (self, t_n##_get_type ()), (f_t) 0); \
\
  { \
    const char *pname = g_property_canonicalize_name (#f_n); \
    int i; \
\
    for (i = 1; i < t_n##_properties_len; i++) \
      { \
        GParamSpec *pspec = t_n##_properties[i]; \
        if (pspec != NULL && pspec->name == pname) \
          { \
            g_property = (GProperty *) pspec; \
            break; \
          } \
      } \
\
    if (G_UNLIKELY (g_property == NULL)) \
      { \
        g_critical (G_STRLOC ": No property " #f_n " found for class %s", \
                    G_OBJECT_TYPE_NAME (self)); \
        return (f_t) 0; \
      } \
  } \
\
  if (!g_property_get (g_property, self, &retval)) \
    { \
      g_property_get_default (g_property, self, &retval); \
      return retval; \
    } \
\
  { /* custom code follows */
#define _G_DEFINE_PROPERTY_GETTER_END   \
  } /* following custom code */         \
\
  return retval;                        \
}

#define _G_DECLARE_PROPERTY_SETTER(T_n, t_n, f_t, f_n)  void t_n##_set_##f_n (T_n *self, f_t value)

#define _G_DEFINE_PROPERTY_SETTER_BEGIN(T_n, t_n, f_t, f_n) \
{ \
  GProperty *g_property = NULL; \
\
  g_return_if_fail (G_TYPE_CHECK_INSTANCE_TYPE (self, t_n##_get_type ())); \
\
  { \
    const char *pname = g_property_canonicalize_name (#f_n); \
    int i; \
\
    for (i = 1; i < t_n##_properties_len; i++) \
      { \
        GParamSpec *pspec = t_n##_properties[i]; \
        if (pspec != NULL && pspec->name == pname) \
          { \
            g_property = (GProperty *) pspec; \
            break; \
          } \
      } \
\
    if (G_UNLIKELY (g_property == NULL)) \
      { \
        g_critical (G_STRLOC ": No property " #f_n " found for class %s", G_OBJECT_TYPE_NAME (self)); \
        return; \
      } \
  } \
\
  if (!g_property_set (g_property, self, value)) \
    return; \
\
  { /* custom code follows */
#define _G_DEFINE_PROPERTY_SETTER_END           \
  }/* following custom code */                  \
}

/**
 * G_DECLARE_PROPERTY_GET_SET:
 * @TypeName: the name of the type, in Camel case
 * @type_name: the name of the type, in lowercase, with words separated by '_'
 * @field_type: the type of the property, which must match the type of the
 *   field in the @TypeName<!-- -->Private structure
 * @field_name: the name of the property, which must match the name of the
 *   field in the @TypeName<!-- -->Private structure
 *
 * Declares the accessor functions for a @field_name property in the
 * class @TypeName. This macro should only be used in header files.
 *
 * Since: 2.38
 */
#define G_DECLARE_PROPERTY_GET_SET(T_n, t_n, f_t, f_n)  \
_G_DECLARE_PROPERTY_SETTER (T_n, t_n, f_t, f_n);        \
_G_DECLARE_PROPERTY_GETTER (T_n, t_n, f_t, f_n);

/**
 * G_DECLARE_PROPERTY_GET:
 * @T_n: the name of the type, in Camel case
 * @t_n: the name of the type, in lowercase, with words separated by '_'
 * @f_t: the type of the property, which must match the type of the field
 * @f_n: the name of the property, which must match the name of the field
 *
 * Declares the getter function for a @f_n property in the @T_n class.
 *
 * This macro should only be used in header files.
 *
 * Since: 2.38
 */
#define G_DECLARE_PROPERTY_GET(T_n, t_n, f_t, f_n)      _G_DECLARE_PROPERTY_GETTER (T_n, t_n, f_t, f_n);

/**
 * G_DECLARE_PROPERTY_SET:
 * @T_n: the name of the type, in Camel case
 * @t_n: the name of the type, in lowercase, with words separated by '_'
 * @f_t: the type of the property, which must match the type of the field
 * @f_n: the name of the property, which must match the name of the field
 *
 * Declares the setter function for a @f_n property in the @T_n class.
 *
 * This macro should only be used in header files.
 *
 * Since: 2.38
 */
#define G_DECLARE_PROPERTY_SET(T_n, t_n, f_t, f_n)      _G_DECLARE_PROPERTY_SETTER (T_n, t_n, f_t, f_n);

/**
 * G_DEFINE_PROPERTY_SET_WITH_CODE:
 * @TypeName: the name of the type, in Camel case
 * @type_name: the name of the type, in lowercase, with words separated by '_'
 * @field_type: the type of the property, which must match the type of the
 *   field in the @TypeName<!-- -->Private structure
 * @field_name: the name of the property, which must match the name of the
 *   field in the @TypeName<!-- -->Private structure
 * @_C_: C code that should be called after the property has been set
 *
 * Defines the setter function for a @field_name property in the
 * class @TypeName, with the possibility of calling custom code, for
 * instance:
 *
 * |[
 * G_DEFINE_PROPERTY_SET_WITH_CODE (ClutterActor, clutter_actor,
 *                                  int, margin_top,
 *                                  clutter_actor_queue_redraw (self))
 * ]|
 *
 * This macro should only be used for properties defined using #GProperty.
 *
 * This macro should only be used in C source files.
 *
 * The code in @_C_ will only be called if the property was successfully
 * updated to a new value.
 *
 * Note that this macro should be used with types defined using G_DEFINE_TYPE_*
 * macros, as it depends on variables and functions defined by those macros.
 *
 * Since: 2.38
 */

#define G_DEFINE_PROPERTY_SET_WITH_CODE(T_n, t_n, f_t, f_n, _C_)   \
_G_DECLARE_PROPERTY_SETTER (T_n, t_n, f_t, f_n)                    \
_G_DEFINE_PROPERTY_SETTER_BEGIN (T_n, t_n, f_t, f_n)               \
{ _C_; } \
_G_DEFINE_PROPERTY_SETTER_END

/**
 * G_DEFINE_PROPERTY_GET_WITH_CODE:
 * @T_n: the name of the type, in Camel case
 * @t_n: the name of the type, in lowercase, with words separated by '_'
 * @f_t: the type of the property, which must match the type of the
 *   field in the @T_n<!-- -->Private structure
 * @f_n: the name of the property, which must match the name of the
 *   field in the @T_n<!-- -->Private structure
 * @_C_: C code to be called after the property has been retrieved
 *
 * Defines the getter function for a @f_n property in the
 * class @T_n, with the possibility of calling custom code.
 *
 * This macro will directly access the field on the private
 * data structure, and should only be used if the property
 * has been defined to use an offset instead of an explicit
 * getter. Use G_DEFINE_PROPERTY_COMPUTED_GET_WITH_CODE() if
 * you have an internal getter function.
 *
 * This macro should only be used in C source files.
 *
 * Note that this macro should be used with types defined using G_DEFINE_TYPE_*
 * macros, as it depends on variables and functions defined by those macros.
 *
 * Since: 2.38
 */
#define G_DEFINE_PROPERTY_GET_WITH_CODE(T_n, t_n, f_t, f_n, _C_) \
_G_DECLARE_PROPERTY_GETTER (T_n, t_n, f_t, f_n)                  \
_G_DEFINE_PROPERTY_DIRECT_GETTER_BEGIN (T_n, t_n, f_t, f_n)      \
{ _C_; } \
_G_DEFINE_PROPERTY_GETTER_END

/**
 * G_DEFINE_PROPERTY_INDIRECT_GET_WITH_CODE:
 * @T_n: the name of the type, in Camel case
 * @t_n: the name of the type, in lowercase, with words separated by '_'
 * @f_t: the type of the property, which must match the type of the
 *   field in the @T_n<!-- -->Private structure
 * @f_n: the name of the property, which must match the name of the
 *   field in the @T_n<!-- -->Private structure
 * @_C_: C code to be called after the property has been retrieved
 *
 * Defines the getter function for a @f_n property in the
 * class @T_n, with the possibility of calling custom code.
 *
 * This macro will call g_property_get().
 *
 * This macro should only be used in C source files.
 *
 * Note that this macro should be used with types defined using G_DEFINE_TYPE_*
 * macros, as it depends on variables and functions defined by those macros.
 *
 * Since: 2.38
 */
#define G_DEFINE_PROPERTY_INDIRECT_GET_WITH_CODE(T_n, t_n, f_t, f_n, _C_) \
_G_DECLARE_PROPERTY_GETTER (T_n, t_n, f_t, f_n)                           \
_G_DEFINE_PROPERTY_INDIRECT_GETTER_BEGIN (T_n, t_n, f_t, f_n)             \
{ _C_; } \
_G_DEFINE_PROPERTY_GETTER_END

/**
 * G_DEFINE_PROPERTY_SET:
 * @TypeName: the name of the type, in Camel case
 * @type_name: the name of the type, in lowercase, with words separated by '_'
 * @field_type: the type of the property, which must match the type of the
 *   field in the @TypeName<!-- -->Private structure
 * @field_name: the name of the property, which must match the name of the
 *   field in the @TypeName<!-- -->Private structure
 *
 * Defines the setter function for a @field_name property in the
 * class @TypeName. This macro should only be used in C source files.
 *
 * Note that this macro should be used with types defined using G_DEFINE_TYPE_*
 * macros, as it depends on variables and functions defined by those macros.
 *
 * See also: %G_DEFINE_PROPERTY_SET_WITH_CODE
 *
 * Since: 2.38
 */
#define G_DEFINE_PROPERTY_SET(T_n, t_n, f_t, f_n)  G_DEFINE_PROPERTY_SET_WITH_CODE (T_n, t_n, f_t, f_n, ;)

/**
 * G_DEFINE_PROPERTY_GET:
 * @TypeName: the name of the type, in Camel case
 * @type_name: the name of the type, in lowercase, with words separated by '_'
 * @field_type: the type of the property, which must match the type of the
 *   field in the @TypeName<!-- -->Private structure
 * @field_name: the name of the property, which must match the name of the
 *   field in the @TypeName<!-- -->Private structure
 *
 * Defines the getter function for a @field_name property in the
 * class @TypeName. This macro should only be used in C source files.
 *
 * Note that this macro should be used with types defined using G_DEFINE_TYPE_*
 * macros, as it depends on variables and functions defined by those macros.
 *
 * See also %G_DEFINE_PROPERTY_GET_WITH_CODE.
 *
 * Since: 2.38
 */
#define G_DEFINE_PROPERTY_GET(T_n, t_n, f_t, f_n)  G_DEFINE_PROPERTY_GET_WITH_CODE (T_n, t_n, f_t, f_n, ;)

/**
 * G_DEFINE_PROPERTY_INDIRECT_GET:
 * @TypeName: the name of the type, in Camel case
 * @type_name: the name of the type, in lowercase, with words separated by '_'
 * @field_type: the type of the property, which must match the type of the
 *   field in the @TypeName<!-- -->Private structure
 * @field_name: the name of the property, which must match the name of the
 *   field in the @TypeName<!-- -->Private structure
 *
 * Defines the getter function for a @field_name property in the
 * class @TypeName. This macro should only be used in C source files.
 *
 * Note that this macro should be used with types defined using G_DEFINE_TYPE_*
 * macros, as it depends on variables and functions defined by those macros.
 *
 * See also %G_DEFINE_PROPERTY_COMPUTED_GET_WITH_CODE.
 *
 * Since: 2.38
 */
#define G_DEFINE_PROPERTY_INDIRECT_GET(T_n, t_n, f_t, f_n)  G_DEFINE_PROPERTY_INDIRECT_GET_WITH_CODE (T_n, t_n, f_t, f_n, ;)

/**
 * G_DEFINE_PROPERTY_GET_SET:
 * @T_n: the name of the type, in Camel case
 * @t_n: the name of the type, in lowercase, with words separated by '_'
 * @f_t: the type of the property, which must match the type of the
 *   field in the @TypeName<!-- -->Private structure
 * @f_n: the name of the property, which must match the name of the
 *   field in the @TypeName<!-- -->Private structure
 *
 * Defines the accessor functions for a @f_n property in the class @T_n.
 *
 * This macro should only be used in C source files, for instance:
 *
 * |[
 *   G_DEFINE_PROPERTY_GET_SET (ClutterActor, clutter_actor, int, margin_top)
 * ]|
 *
 * will synthesize the equivalent of the following code:
 *
 * |[
 * void
 * clutter_actor_set_margin_top (ClutterActor *self,
 *                               int           value)
 * {
 *   ClutterActorPrivate *priv;
 *
 *   g_return_if_fail (G_TYPE_CHECK_INSTANCE_TYPE (self, clutter_actor_get_type ()));
 *
 *   priv = clutter_actor_get_instance_private (self);
 *
 *   if (priv->margin_top != value)
 *     {
 *       priv->value = value;
 *
 *       g_object_notify (G_OBJECT (self), "margin-top");
 *     }
 * }
 *
 * int
 * clutter_actor_get_margin_top (ClutterActor *self)
 * {
 *   ClutterActorPrivate *priv;
 *
 *   g_return_val_if_fail (G_TYPE_CHECK_INSTANCE_TYPE (self, clutter_actor_get_type ()), 0);
 *
 *   priv = clutter_actor_get_instance_private (self);
 *
 *   return priv->margin_top;
 * }
 * ]|
 *
 * This macro will generate both the setter and getter functions; if the
 * property is not readable and writable, the generated functions will
 * warn at run-time.
 *
 * For greater control on the setter and getter implementation, see also the
 * %G_DEFINE_PROPERTY_GET and %G_DEFINE_PROPERTY_SET macros, along with their
 * %G_DEFINE_PROPERTY_GET_WITH_CODE and %G_DEFINE_PROPERTY_SET_WITH_CODE
 * variants.
 *
 * Note that this macro should only be used with types defined using
 * G_DEFINE_TYPE_* macros, as it depends on variable and functions defined
 * by those macros.
 *
 * Since: 2.38
 */
#define G_DEFINE_PROPERTY_GET_SET(T_n, t_n, f_t, f_n)      \
G_DEFINE_PROPERTY_GET (T_n, t_n, f_t, f_n)                 \
G_DEFINE_PROPERTY_SET (T_n, t_n, f_t, f_n)

G_END_DECLS

#endif /* __G_PROPERTY_H__ */
