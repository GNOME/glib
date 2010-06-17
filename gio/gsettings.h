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

#if !defined (__GIO_GIO_H_INSIDE__) && !defined (GIO_COMPILATION)
#error "Only <gio/gio.h> can be included directly."
#endif

#ifndef __G_SETTINGS_H__
#define __G_SETTINGS_H__

#include <gio/giotypes.h>

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

struct _GSettingsClass
{
  GObjectClass parent_class;

  /* Signals */
  void        (*writable_changed)      (GSettings    *settings,
                                        const gchar  *key);
  void        (*changed)               (GSettings    *settings,
                                        const gchar  *key);
  gboolean    (*writable_change_event) (GSettings    *settings,
                                        GQuark        key);
  gboolean    (*change_event)          (GSettings    *settings,
                                        const GQuark *keys,
                                        gint          n_keys);

  gpointer padding[20];
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
GSettings *             g_settings_new_with_backend                     (const gchar        *schema,
                                                                         GSettingsBackend   *backend);
GSettings *             g_settings_new_with_backend_and_path            (const gchar        *schema,
                                                                         GSettingsBackend   *backend,
                                                                         const gchar        *path);

gboolean                g_settings_set_value                            (GSettings          *settings,
                                                                         const gchar        *key,
                                                                         GVariant           *value);
GVariant *              g_settings_get_value                            (GSettings          *settings,
                                                                         const gchar        *key);

gboolean                g_settings_set                                  (GSettings          *settings,
                                                                         const gchar        *key,
                                                                         const gchar        *format,
                                                                         ...);
void                    g_settings_get                                  (GSettings          *settings,
                                                                         const gchar        *key,
                                                                         const gchar        *format,
                                                                         ...);

gint                    g_settings_get_int                              (GSettings          *settings,
                                                                         const gchar        *key);
gboolean                g_settings_set_int                              (GSettings          *settings,
                                                                         const gchar        *key,
                                                                         gint                value);
gchar *                 g_settings_get_string                           (GSettings          *settings,
                                                                         const gchar        *key);
gboolean                g_settings_set_string                           (GSettings          *settings,
                                                                         const gchar        *key,
                                                                         const gchar        *value);
gboolean                g_settings_get_boolean                          (GSettings          *settings,
                                                                         const gchar        *key);
gboolean                g_settings_set_boolean                          (GSettings          *settings,
                                                                         const gchar        *key,
                                                                         gboolean            value);
gdouble                 g_settings_get_double                           (GSettings          *settings,
                                                                         const gchar        *key);
gboolean                g_settings_set_double                           (GSettings          *settings,
                                                                         const gchar        *key,
                                                                         gdouble             value);
gchar **                g_settings_get_strv                             (GSettings          *settings,
                                                                         const gchar        *key);
gboolean                g_settings_set_strv                             (GSettings          *settings,
                                                                         const gchar        *key,
                                                                         const gchar *const *value);
gint                    g_settings_get_enum                             (GSettings          *settings,
                                                                         const gchar        *key);
gboolean                g_settings_set_enum                             (GSettings          *settings,
                                                                         const gchar        *key,
                                                                         gint                value);
GSettings *             g_settings_get_child                            (GSettings          *settings,
                                                                         const gchar        *name);

gboolean                g_settings_is_writable                          (GSettings          *settings,
                                                                         const gchar        *name);

void                    g_settings_delay                                (GSettings          *settings);
void                    g_settings_apply                                (GSettings          *settings);
void                    g_settings_revert                               (GSettings          *settings);
gboolean                g_settings_get_has_unapplied                    (GSettings          *settings);
void                    g_settings_sync                                 (void);

/**
 * GSettingsBindSetMapping:
 * @value: a #GValue containing the property value to map
 * @expected_type: the #GVariantType to create
 * @user_data: user data that was specified when the binding was created
 * @returns: a new #GVariant holding the data from @value,
 *     or %NULL in case of an error
 *
 * The type for the function that is used to convert an object property
 * value to a #GVariant for storing it in #GSettings.
 */
typedef GVariant *    (*GSettingsBindSetMapping)                        (const GValue       *value,
                                                                         const GVariantType *expected_type,
                                                                         gpointer            user_data);

/**
 * GSettingsBindGetMapping:
 * @value: return location for the property value
 * @variant: the #GVariant
 * @user_data: user data that was specified when the binding was created
 * @returns: %TRUE if the conversion succeeded, %FALSE in case of an error
 *
 * The type for the function that is used to convert from #GSettings to
 * an object property. The @value is already initialized to hold values
 * of the appropriate type.
 */
typedef gboolean      (*GSettingsBindGetMapping)                        (GValue             *value,
                                                                         GVariant           *variant,
                                                                         gpointer            user_data);

/**
 * GSettingsBindFlags:
 * @G_SETTINGS_BIND_DEFAULT: Equivalent to <literal>G_SETTINGS_BIND_GET|G_SETTINGS_BIND_SET</literal>
 * @G_SETTINGS_BIND_GET: Update the #GObject property when the setting changes.
 *     It is an error to use this flag if the property is not writable.
 * @G_SETTINGS_BIND_SET: Update the setting when the #GObject property changes.
 *     It is an error to use this flag if the property is not readable.
 * @G_SETTINGS_BIND_NO_SENSITIVITY: Do not try to bind a "sensitivity" property to the writability of the setting
 * @G_SETTINGS_BIND_GET_NO_CHANGES: When set in addition to #G_SETTINGS_BIND_GET, set the #GObject property
 *     value initially from the setting, but do not listen for changes of the setting
 *
 * Flags used when creating a binding. These flags determine in which
 * direction the binding works. The default is to synchronize in both
 * directions.
 */
typedef enum
{
  G_SETTINGS_BIND_DEFAULT,
  G_SETTINGS_BIND_GET            = (1<<0),
  G_SETTINGS_BIND_SET            = (1<<1),
  G_SETTINGS_BIND_NO_SENSITIVITY = (1<<2),
  G_SETTINGS_BIND_GET_NO_CHANGES = (1<<3)
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
                                                                         gpointer                 user_data,
                                                                         GDestroyNotify           destroy);
void                    g_settings_bind_writable                        (GSettings               *settings,
                                                                         const gchar             *key,
                                                                         gpointer                 object,
                                                                         const gchar             *property,
                                                                         gboolean                 inverted);
void                    g_settings_unbind                               (gpointer                 object,
                                                                         const gchar             *property);

G_END_DECLS

#endif  /* __G_SETTINGS_H__ */
