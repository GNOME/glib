/*
 * Copyright © 2010 Codethink Limited
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
 */

#ifndef __G_SETTINGS_SCHEMA_INTERNAL_H__
#define __G_SETTINGS_SCHEMA_INTERNAL_H__

#include "gsettingsschema.h"

typedef struct
{
  GSettingsSchema *schema;
  const gchar *name;

  guint is_flags : 1;
  guint is_enum  : 1;

  const guint32 *strinfo;
  gsize strinfo_length;

  const gchar *unparsed;
  gchar lc_char;

  const GVariantType *type;
  GVariant *minimum, *maximum;
  GVariant *default_value;
} GSettingsSchemaKey;

G_GNUC_INTERNAL
const gchar *           g_settings_schema_get_gettext_domain            (GSettingsSchema  *schema);
G_GNUC_INTERNAL
GVariantIter *          g_settings_schema_get_value                     (GSettingsSchema  *schema,
                                                                         const gchar      *key);
G_GNUC_INTERNAL
gboolean                g_settings_schema_has_key                       (GSettingsSchema  *schema,
                                                                         const gchar      *key);
G_GNUC_INTERNAL
const GQuark *          g_settings_schema_list                          (GSettingsSchema  *schema,
                                                                         gint             *n_items);
G_GNUC_INTERNAL
const gchar *           g_settings_schema_get_string                    (GSettingsSchema  *schema,
                                                                         const gchar      *key);

G_GNUC_INTERNAL
void                    g_settings_schema_key_init                      (GSettingsSchemaKey *key,
                                                                         GSettingsSchema    *schema,
                                                                         const gchar        *name);
G_GNUC_INTERNAL
void                    g_settings_schema_key_clear                     (GSettingsSchemaKey *key);
G_GNUC_INTERNAL
gboolean                g_settings_schema_key_type_check                (GSettingsSchemaKey *key,
                                                                         GVariant           *value);
G_GNUC_INTERNAL
gboolean                g_settings_schema_key_range_check               (GSettingsSchemaKey *key,
                                                                         GVariant           *value);
G_GNUC_INTERNAL
GVariant *              g_settings_schema_key_range_fixup               (GSettingsSchemaKey *key,
                                                                         GVariant           *value);
G_GNUC_INTERNAL
GVariant *              g_settings_schema_key_get_translated_default    (GSettingsSchemaKey *key);

G_GNUC_INTERNAL
gint                    g_settings_schema_key_to_enum                   (GSettingsSchemaKey *key,
                                                                         GVariant           *value);
G_GNUC_INTERNAL
GVariant *              g_settings_schema_key_from_enum                 (GSettingsSchemaKey *key,
                                                                         gint                value);
G_GNUC_INTERNAL
guint                   g_settings_schema_key_to_flags                  (GSettingsSchemaKey *key,
                                                                         GVariant           *value);
G_GNUC_INTERNAL
GVariant *              g_settings_schema_key_from_flags                (GSettingsSchemaKey *key,
                                                                         guint               value);

#endif /* __G_SETTINGS_SCHEMA_INTERNAL_H__ */
