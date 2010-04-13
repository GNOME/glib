/*
 * Copyright © 2009 Codethink Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of version 3 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * See the included COPYING file for more information.
 */

#ifndef _gsettings_h_
#define _gsettings_h_

#include "gsettingsbackend.h"

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


  GSettings * (*get_settings) (GSettings    *settings,
                               const gchar  *name);


  void        (*changes)      (GSettings    *settings,
                               const GQuark *keys,
                               gint          n_keys);
  void        (*changed)      (GSettings    *settings,
                               const gchar  *key);
  void        (*destroyed)    (GSettings    *settings);
};

struct _GSettings
{
  GObject parent_instance;
  GSettingsPrivate *priv;
};

typedef enum
{
  G_SETTINGS_BIND_DEFAULT,
  G_SETTINGS_BIND_GET            = (1<<0),
  G_SETTINGS_BIND_SET            = (1<<1),
  G_SETTINGS_BIND_NO_SENSITIVITY = (1<<2)
} GSettingsBindFlags;

G_BEGIN_DECLS

GType                   g_settings_get_type                             (void);
void                    g_settings_revert                               (GSettings          *settings);
void                    g_settings_apply                                (GSettings          *settings);

gboolean                g_settings_get_delay_apply                      (GSettings          *settings);
gboolean                g_settings_get_has_unapplied                    (GSettings          *settings);
void                    g_settings_set_delay_apply                      (GSettings          *settings,
                                                                         gboolean            delay_apply);
gboolean                g_settings_get_locked                           (GSettings          *settings);
void                    g_settings_lock                                 (GSettings          *settings);

GSettings *             g_settings_new                                  (const gchar        *schema);
GSettings *             g_settings_new_from_path                        (const gchar        *path);

void                    g_settings_set_value                            (GSettings          *settings,
                                                                         const gchar        *key,
                                                                         GVariant           *value);

GVariant *              g_settings_get_value                            (GSettings          *settings,
                                                                         const gchar        *key);

void                    g_settings_set                                  (GSettings          *settings,
                                                                         const gchar        *key,
                                                                         const gchar        *format,
                                                                         ...);

void                    g_settings_get                                  (GSettings          *settings,
                                                                         const gchar        *key,
                                                                         const gchar        *format_string,
                                                                         ...);

GSettings *             g_settings_get_settings                         (GSettings          *settings,
                                                                         const gchar        *name);

gboolean                g_settings_is_writable                          (GSettings          *settings,
                                                                         const gchar        *name);
void                    g_settings_changes                              (GSettings          *settings,
                                                                         const GQuark       *keys,
                                                                         gint                n_keys);
void                    g_settings_destroy                              (GSettings          *settings);

void                    g_settings_bind                                 (GSettings          *settings,
                                                                         const gchar        *key,
                                                                         gpointer            object,
                                                                         const gchar        *property,
                                                                         GSettingsBindFlags  flags);
void                    g_settings_unbind                               (gpointer            object,
                                                                         const gchar        *key);

G_END_DECLS

#endif /* _gsettings_h_ */
