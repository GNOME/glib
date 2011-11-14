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

#include <glib.h>

typedef struct _GSettingsSchema                             GSettingsSchema;

G_GNUC_INTERNAL
GSettingsSchema *       g_settings_schema_new                           (const gchar      *name);
G_GNUC_INTERNAL
GSettingsSchema *       g_settings_schema_ref                           (GSettingsSchema  *schema);
G_GNUC_INTERNAL
void                    g_settings_schema_unref                         (GSettingsSchema  *schema);
G_GNUC_INTERNAL
const gchar *           g_settings_schema_get_name                      (GSettingsSchema  *schema);
G_GNUC_INTERNAL
const gchar *           g_settings_schema_get_path                      (GSettingsSchema  *schema);
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

#endif /* __G_SETTINGS_SCHEMA_INTERNAL_H__ */
