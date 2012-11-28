/*
 * Copyright © 2007, 2008 Ryan Lortie
 * Copyright © 2009, 2010 Codethink Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#if !defined (__GLIB_H_INSIDE__) && !defined (GLIB_COMPILATION)
#error "Only <glib.h> can be included directly."
#endif

#ifndef __G_VARIANT_H__
#define __G_VARIANT_H__

#include <glib/gvarianttype.h>
#include <glib/gstring.h>
#include <glib/gbytes.h>

G_BEGIN_DECLS

typedef struct _GVariant        GVariant;

typedef enum
{
  G_VARIANT_CLASS_BOOLEAN       = 'b',
  G_VARIANT_CLASS_BYTE          = 'y',
  G_VARIANT_CLASS_INT16         = 'n',
  G_VARIANT_CLASS_UINT16        = 'q',
  G_VARIANT_CLASS_INT32         = 'i',
  G_VARIANT_CLASS_UINT32        = 'u',
  G_VARIANT_CLASS_INT64         = 'x',
  G_VARIANT_CLASS_UINT64        = 't',
  G_VARIANT_CLASS_HANDLE        = 'h',
  G_VARIANT_CLASS_DOUBLE        = 'd',
  G_VARIANT_CLASS_STRING        = 's',
  G_VARIANT_CLASS_OBJECT_PATH   = 'o',
  G_VARIANT_CLASS_SIGNATURE     = 'g',
  G_VARIANT_CLASS_VARIANT       = 'v',
  G_VARIANT_CLASS_MAYBE         = 'm',
  G_VARIANT_CLASS_ARRAY         = 'a',
  G_VARIANT_CLASS_TUPLE         = '(',
  G_VARIANT_CLASS_DICT_ENTRY    = '{'
} GVariantClass;

_GLIB_API
void                            g_variant_unref                         (GVariant             *value);
_GLIB_API
GVariant *                      g_variant_ref                           (GVariant             *value);
_GLIB_API
GVariant *                      g_variant_ref_sink                      (GVariant             *value);
_GLIB_API
gboolean                        g_variant_is_floating                   (GVariant             *value);
_GLIB_API
GVariant *                      g_variant_take_ref                      (GVariant             *value);

_GLIB_API
const GVariantType *            g_variant_get_type                      (GVariant             *value);
_GLIB_API
const gchar *                   g_variant_get_type_string               (GVariant             *value);
_GLIB_API
gboolean                        g_variant_is_of_type                    (GVariant             *value,
                                                                         const GVariantType   *type);
_GLIB_API
gboolean                        g_variant_is_container                  (GVariant             *value);
_GLIB_API
GVariantClass                   g_variant_classify                      (GVariant             *value);
_GLIB_API
GVariant *                      g_variant_new_boolean                   (gboolean              value);
_GLIB_API
GVariant *                      g_variant_new_byte                      (guchar                value);
_GLIB_API
GVariant *                      g_variant_new_int16                     (gint16                value);
_GLIB_API
GVariant *                      g_variant_new_uint16                    (guint16               value);
_GLIB_API
GVariant *                      g_variant_new_int32                     (gint32                value);
_GLIB_API
GVariant *                      g_variant_new_uint32                    (guint32               value);
_GLIB_API
GVariant *                      g_variant_new_int64                     (gint64                value);
_GLIB_API
GVariant *                      g_variant_new_uint64                    (guint64               value);
_GLIB_API
GVariant *                      g_variant_new_handle                    (gint32                value);
_GLIB_API
GVariant *                      g_variant_new_double                    (gdouble               value);
_GLIB_API
GVariant *                      g_variant_new_string                    (const gchar          *string);
_GLIB_API
GVariant *                      g_variant_new_object_path               (const gchar          *object_path);
_GLIB_API
gboolean                        g_variant_is_object_path                (const gchar          *string);
_GLIB_API
GVariant *                      g_variant_new_signature                 (const gchar          *signature);
_GLIB_API
gboolean                        g_variant_is_signature                  (const gchar          *string);
_GLIB_API
GVariant *                      g_variant_new_variant                   (GVariant             *value);
_GLIB_API
GVariant *                      g_variant_new_strv                      (const gchar * const  *strv,
                                                                         gssize                length);
GLIB_AVAILABLE_IN_2_30
GVariant *                      g_variant_new_objv                      (const gchar * const  *strv,
                                                                         gssize                length);
_GLIB_API
GVariant *                      g_variant_new_bytestring                (const gchar          *string);
_GLIB_API
GVariant *                      g_variant_new_bytestring_array          (const gchar * const  *strv,
                                                                         gssize                length);
_GLIB_API
GVariant *                      g_variant_new_fixed_array               (const GVariantType   *element_type,
                                                                         gconstpointer         elements,
                                                                         gsize                 n_elements,
                                                                         gsize                 element_size);
_GLIB_API
gboolean                        g_variant_get_boolean                   (GVariant             *value);
_GLIB_API
guchar                          g_variant_get_byte                      (GVariant             *value);
_GLIB_API
gint16                          g_variant_get_int16                     (GVariant             *value);
_GLIB_API
guint16                         g_variant_get_uint16                    (GVariant             *value);
_GLIB_API
gint32                          g_variant_get_int32                     (GVariant             *value);
_GLIB_API
guint32                         g_variant_get_uint32                    (GVariant             *value);
_GLIB_API
gint64                          g_variant_get_int64                     (GVariant             *value);
_GLIB_API
guint64                         g_variant_get_uint64                    (GVariant             *value);
_GLIB_API
gint32                          g_variant_get_handle                    (GVariant             *value);
_GLIB_API
gdouble                         g_variant_get_double                    (GVariant             *value);
_GLIB_API
GVariant *                      g_variant_get_variant                   (GVariant             *value);
_GLIB_API
const gchar *                   g_variant_get_string                    (GVariant             *value,
                                                                         gsize                *length);
_GLIB_API
gchar *                         g_variant_dup_string                    (GVariant             *value,
                                                                         gsize                *length);
_GLIB_API
const gchar **                  g_variant_get_strv                      (GVariant             *value,
                                                                         gsize                *length);
_GLIB_API
gchar **                        g_variant_dup_strv                      (GVariant             *value,
                                                                         gsize                *length);
GLIB_AVAILABLE_IN_2_30
const gchar **                  g_variant_get_objv                      (GVariant             *value,
                                                                         gsize                *length);
_GLIB_API
gchar **                        g_variant_dup_objv                      (GVariant             *value,
                                                                         gsize                *length);
_GLIB_API
const gchar *                   g_variant_get_bytestring                (GVariant             *value);
_GLIB_API
gchar *                         g_variant_dup_bytestring                (GVariant             *value,
                                                                         gsize                *length);
_GLIB_API
const gchar **                  g_variant_get_bytestring_array          (GVariant             *value,
                                                                         gsize                *length);
_GLIB_API
gchar **                        g_variant_dup_bytestring_array          (GVariant             *value,
                                                                         gsize                *length);

_GLIB_API
GVariant *                      g_variant_new_maybe                     (const GVariantType   *child_type,
                                                                         GVariant             *child);
_GLIB_API
GVariant *                      g_variant_new_array                     (const GVariantType   *child_type,
                                                                         GVariant * const     *children,
                                                                         gsize                 n_children);
_GLIB_API
GVariant *                      g_variant_new_tuple                     (GVariant * const     *children,
                                                                         gsize                 n_children);
_GLIB_API
GVariant *                      g_variant_new_dict_entry                (GVariant             *key,
                                                                         GVariant             *value);

_GLIB_API
GVariant *                      g_variant_get_maybe                     (GVariant             *value);
_GLIB_API
gsize                           g_variant_n_children                    (GVariant             *value);
_GLIB_API
void                            g_variant_get_child                     (GVariant             *value,
                                                                         gsize                 index_,
                                                                         const gchar          *format_string,
                                                                         ...);
_GLIB_API
GVariant *                      g_variant_get_child_value               (GVariant             *value,
                                                                         gsize                 index_);
_GLIB_API
gboolean                        g_variant_lookup                        (GVariant             *dictionary,
                                                                         const gchar          *key,
                                                                         const gchar          *format_string,
                                                                         ...);
_GLIB_API
GVariant *                      g_variant_lookup_value                  (GVariant             *dictionary,
                                                                         const gchar          *key,
                                                                         const GVariantType   *expected_type);
_GLIB_API
gconstpointer                   g_variant_get_fixed_array               (GVariant             *value,
                                                                         gsize                *n_elements,
                                                                         gsize                 element_size);

_GLIB_API
gsize                           g_variant_get_size                      (GVariant             *value);
_GLIB_API
gconstpointer                   g_variant_get_data                      (GVariant             *value);
GLIB_AVAILABLE_IN_2_36
GBytes *                        g_variant_get_data_as_bytes             (GVariant             *value);
_GLIB_API
void                            g_variant_store                         (GVariant             *value,
                                                                         gpointer              data);

_GLIB_API
gchar *                         g_variant_print                         (GVariant             *value,
                                                                         gboolean              type_annotate);
_GLIB_API
GString *                       g_variant_print_string                  (GVariant             *value,
                                                                         GString              *string,
                                                                         gboolean              type_annotate);

_GLIB_API
guint                           g_variant_hash                          (gconstpointer         value);
_GLIB_API
gboolean                        g_variant_equal                         (gconstpointer         one,
                                                                         gconstpointer         two);

_GLIB_API
GVariant *                      g_variant_get_normal_form               (GVariant             *value);
_GLIB_API
gboolean                        g_variant_is_normal_form                (GVariant             *value);
_GLIB_API
GVariant *                      g_variant_byteswap                      (GVariant             *value);

GLIB_AVAILABLE_IN_2_36
GVariant *                      g_variant_new_from_bytes                (const GVariantType   *type,
                                                                         GBytes               *bytes,
                                                                         gboolean              trusted);
_GLIB_API
GVariant *                      g_variant_new_from_data                 (const GVariantType   *type,
                                                                         gconstpointer         data,
                                                                         gsize                 size,
                                                                         gboolean              trusted,
                                                                         GDestroyNotify        notify,
                                                                         gpointer              user_data);

typedef struct _GVariantIter GVariantIter;
struct _GVariantIter {
  /*< private >*/
  gsize x[16];
};

_GLIB_API
GVariantIter *                  g_variant_iter_new                      (GVariant             *value);
_GLIB_API
gsize                           g_variant_iter_init                     (GVariantIter         *iter,
                                                                         GVariant             *value);
_GLIB_API
GVariantIter *                  g_variant_iter_copy                     (GVariantIter         *iter);
_GLIB_API
gsize                           g_variant_iter_n_children               (GVariantIter         *iter);
_GLIB_API
void                            g_variant_iter_free                     (GVariantIter         *iter);
_GLIB_API
GVariant *                      g_variant_iter_next_value               (GVariantIter         *iter);
_GLIB_API
gboolean                        g_variant_iter_next                     (GVariantIter         *iter,
                                                                         const gchar          *format_string,
                                                                         ...);
_GLIB_API
gboolean                        g_variant_iter_loop                     (GVariantIter         *iter,
                                                                         const gchar          *format_string,
                                                                         ...);


typedef struct _GVariantBuilder GVariantBuilder;
struct _GVariantBuilder {
  /*< private >*/
  gsize x[16];
};

typedef enum
{
  G_VARIANT_PARSE_ERROR_FAILED,
  G_VARIANT_PARSE_ERROR_BASIC_TYPE_EXPECTED,
  G_VARIANT_PARSE_ERROR_CANNOT_INFER_TYPE,
  G_VARIANT_PARSE_ERROR_DEFINITE_TYPE_EXPECTED,
  G_VARIANT_PARSE_ERROR_INPUT_NOT_AT_END,
  G_VARIANT_PARSE_ERROR_INVALID_CHARACTER,
  G_VARIANT_PARSE_ERROR_INVALID_FORMAT_STRING,
  G_VARIANT_PARSE_ERROR_INVALID_OBJECT_PATH,
  G_VARIANT_PARSE_ERROR_INVALID_SIGNATURE,
  G_VARIANT_PARSE_ERROR_INVALID_TYPE_STRING,
  G_VARIANT_PARSE_ERROR_NO_COMMON_TYPE,
  G_VARIANT_PARSE_ERROR_NUMBER_OUT_OF_RANGE,
  G_VARIANT_PARSE_ERROR_NUMBER_TOO_BIG,
  G_VARIANT_PARSE_ERROR_TYPE_ERROR,
  G_VARIANT_PARSE_ERROR_UNEXPECTED_TOKEN,
  G_VARIANT_PARSE_ERROR_UNKNOWN_KEYWORD,
  G_VARIANT_PARSE_ERROR_UNTERMINATED_STRING_CONSTANT,
  G_VARIANT_PARSE_ERROR_VALUE_EXPECTED
} GVariantParseError;
#define G_VARIANT_PARSE_ERROR (g_variant_parser_get_error_quark ())

_GLIB_API
GQuark                          g_variant_parser_get_error_quark        (void);

_GLIB_API
GVariantBuilder *               g_variant_builder_new                   (const GVariantType   *type);
_GLIB_API
void                            g_variant_builder_unref                 (GVariantBuilder      *builder);
_GLIB_API
GVariantBuilder *               g_variant_builder_ref                   (GVariantBuilder      *builder);
_GLIB_API
void                            g_variant_builder_init                  (GVariantBuilder      *builder,
                                                                         const GVariantType   *type);
_GLIB_API
GVariant *                      g_variant_builder_end                   (GVariantBuilder      *builder);
_GLIB_API
void                            g_variant_builder_clear                 (GVariantBuilder      *builder);
_GLIB_API
void                            g_variant_builder_open                  (GVariantBuilder      *builder,
                                                                         const GVariantType   *type);
_GLIB_API
void                            g_variant_builder_close                 (GVariantBuilder      *builder);
_GLIB_API
void                            g_variant_builder_add_value             (GVariantBuilder      *builder,
                                                                         GVariant             *value);
_GLIB_API
void                            g_variant_builder_add                   (GVariantBuilder      *builder,
                                                                         const gchar          *format_string,
                                                                         ...);
_GLIB_API
void                            g_variant_builder_add_parsed            (GVariantBuilder      *builder,
                                                                         const gchar          *format,
                                                                         ...);

_GLIB_API
GVariant *                      g_variant_new                           (const gchar          *format_string,
                                                                         ...);
_GLIB_API
void                            g_variant_get                           (GVariant             *value,
                                                                         const gchar          *format_string,
                                                                         ...);
_GLIB_API
GVariant *                      g_variant_new_va                        (const gchar          *format_string,
                                                                         const gchar         **endptr,
                                                                         va_list              *app);
_GLIB_API
void                            g_variant_get_va                        (GVariant             *value,
                                                                         const gchar          *format_string,
                                                                         const gchar         **endptr,
                                                                         va_list              *app);
_GLIB_API
gboolean                        g_variant_check_format_string           (GVariant             *value,
                                                                         const gchar          *format_string,
                                                                         gboolean              copy_only);

_GLIB_API
GVariant *                      g_variant_parse                         (const GVariantType   *type,
                                                                         const gchar          *text,
                                                                         const gchar          *limit,
                                                                         const gchar         **endptr,
                                                                         GError              **error);
_GLIB_API
GVariant *                      g_variant_new_parsed                    (const gchar          *format,
                                                                         ...);
_GLIB_API
GVariant *                      g_variant_new_parsed_va                 (const gchar          *format,
                                                                         va_list              *app);

_GLIB_API
gint                            g_variant_compare                       (gconstpointer one,
                                                                         gconstpointer two);
G_END_DECLS

#endif /* __G_VARIANT_H__ */
