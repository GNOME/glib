/*
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

#ifndef __G_SETTINGS_H__
#define __G_SETTINGS_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define G_TYPE_SETTINGS                                     (g_settings_get_type ())
#define G_SETTINGS(inst)                                    (G_TYPE_CHECK_INSTANCE_CAST ((inst),                     \
                                                             G_TYPE_SETTINGS, GSettings))
#define G_SETTINGS_CLASS(class)                             (G_TYPE_CHECK_CLASS_CAST ((class),                       \
                                                             G_TYPE_SETTINGS, GSettingsClass))
#define G_IS_SETTINGS(inst)                                 (G_TYPE_CHECK_INSTANCE_TYPE ((inst), G_TYPE_SETTINGS))
#define G_IS_SETTINGS_CLASS(class)                          (G_TYPE_CHECK_CLASS_TYPE ((class), G_TYPE_SETTINGS))
#define G_SETTINGS_GET_CLASS(inst)                          (G_TYPE_INSTANCE_GET_CLASS ((inst),                      \
                                                             G_TYPE_SETTINGS, GSettingsClass))

typedef struct _GSettingsPrivate                            GSettingsPrivate;
typedef struct _GSettingsClass                              GSettingsClass;
typedef struct _GSettings                                   GSettings;

struct _GSettingsClass
{
  GObjectClass parent_class;

  void        (*all_writable_changed) (GSettings    *settings);
  void        (*all_changed)          (GSettings    *settings);
  void        (*keys_changed)         (GSettings    *settings,
                                       const GQuark *keys,
                                       gint          n_keys);

  void        (*writable_changed)     (GSettings    *settings,
                                       const gchar  *key);
  void        (*changed)              (GSettings    *settings,
                                       const gchar  *key);
};

struct _GSettings
{
  GObject parent_instance;
  GSettingsPrivate *priv;
};


GType                   g_settings_get_type                             (void);

GSettings *             g_settings_new                                  (const gchar        *schema);
GSettings *             g_settings_new_with_path                        (const gchar        *schema,
                                                                         const gchar        *path);
gboolean                g_settings_supports_context                     (const gchar        *context);
GSettings *             g_settings_new_with_context                     (const gchar        *schema,
                                                                         const gchar        *context);
GSettings *             g_settings_new_with_context_and_path            (const gchar        *schema,
                                                                         const gchar        *context,
                                                                         const gchar        *path);

void                    g_settings_set_value                            (GSettings          *settings,
                                                                         const gchar        *key,
                                                                         GVariant           *value);
GVariant *              g_settings_get_value                            (GSettings          *settings,
                                                                         const gchar        *key);

void                    g_settings_set                                  (GSettings          *settings,
                                                                         const gchar        *key,
                                                                         const gchar        *format_string,
                                                                         ...);
void                    g_settings_get                                  (GSettings          *settings,
                                                                         const gchar        *key,
                                                                         const gchar        *format_string,
                                                                         ...);

GSettings *             g_settings_get_child                            (GSettings          *settings,
                                                                         const gchar        *name);

void                    g_settings_reset                                (GSettings          *settings,
                                                                         const gchar        *key);
void                    g_settings_reset_all                            (GSettings          *settings);

gboolean                g_settings_is_writable                          (GSettings          *settings,
                                                                         const gchar        *name);

void                    g_settings_delay                                (GSettings          *settings);
void                    g_settings_apply                                (GSettings          *settings);
void                    g_settings_revert                               (GSettings          *settings);
gboolean                g_settings_get_has_unapplied                    (GSettings          *settings);

typedef GVariant *    (*GSettingsBindSetMapping)                        (const GValue       *value,
                                                                         const GVariantType *expected_type,
                                                                         gpointer            user_data);

typedef gboolean      (*GSettingsBindGetMapping)                        (GValue             *value,
                                                                         GVariant           *variant,
                                                                         gpointer            user_data);

typedef enum
{
  G_SETTINGS_BIND_DEFAULT,
  G_SETTINGS_BIND_GET            = (1<<0),
  G_SETTINGS_BIND_SET            = (1<<1),
  G_SETTINGS_BIND_NO_SENSITIVITY = (1<<2)
} GSettingsBindFlags;

void                    g_settings_bind                                 (GSettings               *settings,
                                                                         const gchar             *key,
                                                                         gpointer                 object,
                                                                         const gchar             *property,
                                                                         GSettingsBindFlags       flags);
void                    g_settings_bind_with_mapping                    (GSettings               *settings,
                                                                         const gchar             *key,
                                                                         gpointer                 object,
                                                                         const gchar             *property,
                                                                         GSettingsBindFlags       flags,
                                                                         GSettingsBindGetMapping  get_mapping,
                                                                         GSettingsBindSetMapping  set_mapping,
                                                                         gpointer                 user_data);
void                    g_settings_unbind                               (gpointer                 object,
                                                                         const gchar             *key);

G_END_DECLS

#endif  /* __G_SETTINGS_H__ */
