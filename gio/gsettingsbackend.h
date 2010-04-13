/*
 * Copyright © 2009 Codethink Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of version 3 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * See the included COPYING file for more information.
 */

#ifndef _gsettingsbackend_h_
#define _gsettingsbackend_h_

#include <glib-object.h>

G_BEGIN_DECLS

#define G_TYPE_SETTINGS_BACKEND                             (g_settings_backend_get_type ())
#define G_SETTINGS_BACKEND(inst)                            (G_TYPE_CHECK_INSTANCE_CAST ((inst),                     \
                                                             G_TYPE_SETTINGS_BACKEND, GSettingsBackend))
#define G_SETTINGS_BACKEND_CLASS(class)                     (G_TYPE_CHECK_CLASS_CAST ((class),                       \
                                                             G_TYPE_SETTINGS_BACKEND, GSettingsBackendClass))
#define G_IS_SETTINGS_BACKEND(inst)                         (G_TYPE_CHECK_INSTANCE_TYPE ((inst),                     \
                                                             G_TYPE_SETTINGS_BACKEND))
#define G_IS_SETTINGS_BACKEND_CLASS(class)                  (G_TYPE_CHECK_CLASS_TYPE ((class),                       \
                                                             G_TYPE_SETTINGS_BACKEND))
#define G_SETTINGS_BACKEND_GET_CLASS(inst)                  (G_TYPE_INSTANCE_GET_CLASS ((inst),                      \
                                                             G_TYPE_SETTINGS_BACKEND, GSettingsBackendClass))

#define G_SETTINGS_BACKEND_EXTENSION_POINT_NAME "gsettings-backend"

/**
 * GSettingsBackend:
 *
 * An implementation of a settings storage repository.
 **/
typedef struct _GSettingsBackendPrivate                     GSettingsBackendPrivate;
typedef struct _GSettingsBackendClass                       GSettingsBackendClass;
typedef struct _GSettingsBackend                            GSettingsBackend;

struct _GSettingsBackendClass
{
  GObjectClass parent_class;

  void        (*changed)      (GSettingsBackend    *backend,
                               const gchar         *prefix,
                               gchar const * const *names,
                               gint                 names_len,
                               gpointer             origin_tag);

  GVariant *  (*read)         (GSettingsBackend    *backend,
                               const gchar         *key,
                               const GVariantType  *expected_type);
  void        (*write)        (GSettingsBackend    *backend,
                               const gchar         *prefix,
                               GTree               *tree,
                               gpointer             origin_tag);
  gboolean    (*get_writable) (GSettingsBackend    *backend,
                               const gchar         *name);
  void        (*subscribe)    (GSettingsBackend    *backend,
                               const gchar         *name);
  void        (*unsubscribe)  (GSettingsBackend    *backend,
                               const gchar         *name);
};

struct _GSettingsBackend
{
  GObject parent_instance;

  /*< private >*/
  GSettingsBackendPrivate *priv;
};

GType                           g_settings_backend_get_type             (void);
GSettingsBackend *              g_settings_backend_get_default          (void);
GTree *                         g_settings_backend_create_tree          (void);

GVariant *                      g_settings_backend_read                 (GSettingsBackend    *backend,
                                                                         const gchar         *key,
                                                                         const GVariantType  *expected_type);

void                            g_settings_backend_write                (GSettingsBackend    *backend,
                                                                         const gchar         *prefix,
                                                                         GTree               *values,
                                                                         gpointer             origin_tag);

gboolean                        g_settings_backend_get_writable         (GSettingsBackend    *backend,
                                                                         const char          *name);

void                            g_settings_backend_unsubscribe          (GSettingsBackend    *backend,
                                                                         const char          *name);
void                            g_settings_backend_subscribe            (GSettingsBackend    *backend,
                                                                         const char          *name);

void                            g_settings_backend_changed              (GSettingsBackend    *backend,
                                                                         const gchar         *prefix,
                                                                         gchar const * const *items,
                                                                         gint                 n_items,
                                                                         gpointer             origin_tag);
void                            g_settings_backend_changed_tree         (GSettingsBackend    *backend,
                                                                         const gchar         *prefix,
                                                                         GTree               *tree,
                                                                         gpointer             origin_tag);

G_END_DECLS

#endif /* _gsettingsbackend_h_ */
