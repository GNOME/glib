/*
 * Copyright © 2010 Codethink Limited
 * Copyright © 2011 Canonical Limited
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

#ifndef __G_SETTINGS_SCHEMA_H__
#define __G_SETTINGS_SCHEMA_H__

#include <glib.h>

typedef struct _GSettingsSchemaSource                       GSettingsSchemaSource;
typedef struct _GSettingsSchema                             GSettingsSchema;

GSettingsSchemaSource * g_settings_schema_source_get_default            (void);
GSettingsSchemaSource * g_settings_schema_source_ref                    (GSettingsSchemaSource *source);
void                    g_settings_schema_source_unref                  (GSettingsSchemaSource *source);

GSettingsSchema *       g_settings_schema_source_lookup                 (GSettingsSchemaSource *source,
                                                                         const gchar           *schema_name,
                                                                         gboolean               recursive);

GSettingsSchema *       g_settings_schema_ref                           (GSettingsSchema       *schema);
void                    g_settings_schema_unref                         (GSettingsSchema       *schema);

const gchar *           g_settings_schema_get_name                      (GSettingsSchema       *schema);
const gchar *           g_settings_schema_get_path                      (GSettingsSchema       *schema);

#endif /* __G_SETTINGS_SCHEMA_H__ */
