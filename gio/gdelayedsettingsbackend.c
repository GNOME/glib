/*
 * Copyright © 2009 Codethink Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of version 3 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * See the included COPYING file for more information.
 */

#include "config.h"
#include <glib.h>
#include <glibintl.h>

#include "gdelayedsettingsbackend.h"
#include "gsettingsbackendinternal.h"

#include <string.h>

#include "gioalias.h"

struct _GDelayedSettingsBackendPrivate
{
  GSettingsBackend *backend;
  GTree *delayed;
  gpointer owner;
};

G_DEFINE_TYPE (GDelayedSettingsBackend,
               g_delayed_settings_backend,
               G_TYPE_SETTINGS_BACKEND)

static GVariant *
g_delayed_settings_backend_read (GSettingsBackend   *backend,
                                 const gchar        *key,
                                 const GVariantType *expected_type)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (backend);
  GVariant *result;

  if ((result = g_tree_lookup (delayed->priv->delayed, key))) 
    return g_variant_ref (result);

  return g_settings_backend_read (delayed->priv->backend,
                                  key, expected_type);
}
static gboolean
g_delayed_settings_backend_write (GSettingsBackend *backend,
                                  const gchar      *key,
                                  GVariant         *value,
                                  gpointer          origin_tag)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (backend);
  gboolean was_empty;

  was_empty = g_tree_nnodes (delayed->priv->delayed) == 0;
  g_tree_insert (delayed->priv->delayed, g_strdup (key),
                 g_variant_ref_sink (value));
  g_settings_backend_changed (backend, key, origin_tag);

  if (was_empty && delayed->priv->owner)
    g_object_notify (delayed->priv->owner, "has-unapplied");

  return TRUE;
}

static gboolean
add_to_tree (gpointer key,
             gpointer value,
             gpointer user_data)
{
  g_tree_insert (user_data, g_strdup (key), g_variant_ref (value));
  return FALSE;
}

static gboolean
g_delayed_settings_backend_write_keys (GSettingsBackend *backend,
                                       GTree            *tree,
                                       gpointer          origin_tag)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (backend);
  gboolean was_empty;

  was_empty = g_tree_nnodes (delayed->priv->delayed) == 0;

  g_tree_foreach (tree, add_to_tree, delayed->priv->delayed);

  g_settings_backend_changed_tree (backend, tree, origin_tag);

  if (was_empty && delayed->priv->owner)
    g_object_notify (delayed->priv->owner, "has-unapplied");

  return TRUE;
}

static gboolean
g_delayed_settings_backend_get_writable (GSettingsBackend *backend,
                                         const gchar      *name)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (backend);

  return g_settings_backend_get_writable (delayed->priv->backend, name);
}

static void
g_delayed_settings_backend_reset (GSettingsBackend *backend,
                                  const gchar      *key,
                                  gpointer          origin_tag)
{
  /* deal with this... */
}

static void
g_delayed_settings_backend_reset_path (GSettingsBackend *backend,
                                       const gchar      *path,
                                       gpointer          origin_tag)
{
  /* deal with this... */
}

static void
g_delayed_settings_backend_subscribe (GSettingsBackend *backend,
                                      const char       *name)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (backend);

  g_settings_backend_subscribe (delayed->priv->backend, name);
}

static void
g_delayed_settings_backend_unsubscribe (GSettingsBackend *backend,
                                        const char       *name)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (backend);

  g_settings_backend_unsubscribe (delayed->priv->backend, name);
}


/* method calls */
gboolean
g_delayed_settings_backend_get_has_unapplied (GDelayedSettingsBackend *delayed)
{
  return g_tree_nnodes (delayed->priv->delayed) > 0;
}

void
g_delayed_settings_backend_apply (GDelayedSettingsBackend *delayed)
{
  if (g_tree_nnodes (delayed->priv->delayed))
    {
      GTree *tmp;

      tmp = delayed->priv->delayed;
      delayed->priv->delayed = g_settings_backend_create_tree ();
      g_settings_backend_write_keys (delayed->priv->backend,
                                     tmp, delayed->priv);
      g_tree_unref (tmp);

      if (delayed->priv->owner)
        g_object_notify (delayed->priv->owner, "has-unapplied");
    }
}

void
g_delayed_settings_backend_revert (GDelayedSettingsBackend *delayed)
{
  if (g_tree_nnodes (delayed->priv->delayed))
    {
      GTree *tmp;

      tmp = delayed->priv->delayed;
      delayed->priv->delayed = g_settings_backend_create_tree ();
      g_settings_backend_changed_tree (G_SETTINGS_BACKEND (delayed), tmp, NULL);
      g_tree_destroy (tmp);

      if (delayed->priv->owner)
        g_object_notify (delayed->priv->owner, "has-unapplied");
    }
}

/* change notification */
static void
delayed_backend_changed (GSettingsBackend *backend,
                         const gchar      *key,
                         gpointer          origin_tag,
                         gpointer          user_data)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (user_data);

  if (origin_tag != delayed->priv)
    g_settings_backend_changed (G_SETTINGS_BACKEND (delayed),
                                key, origin_tag);
}

static void
delayed_backend_keys_changed (GSettingsBackend    *backend,
                              const gchar         *path,
                              const gchar * const *items,
                              gpointer             origin_tag,
                              gpointer             user_data)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (user_data);

  if (origin_tag != delayed->priv)
    g_settings_backend_keys_changed (G_SETTINGS_BACKEND (delayed),
                                     path, items, origin_tag);
}

static void
delayed_backend_path_changed (GSettingsBackend    *backend,
                              const gchar         *path,
                              gpointer             origin_tag,
                              gpointer             user_data)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (user_data);

  if (origin_tag != delayed->priv)
    g_settings_backend_path_changed (G_SETTINGS_BACKEND (delayed),
                                     path, origin_tag);
}

static void
delayed_backend_writable_changed (GSettingsBackend *backend,
                                  const gchar      *key,
                                  gpointer          user_data)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (user_data);

  /* XXX: maybe drop keys from the delayed-apply settings
   *      if they became non-writable?
   */
  g_settings_backend_writable_changed (G_SETTINGS_BACKEND (delayed), key);
}

static void
delayed_backend_path_writable_changed (GSettingsBackend *backend,
                                       const gchar      *path,
                                       gpointer          user_data)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (user_data);

  /* XXX: maybe drop keys from the delayed-apply settings
   *      if they became non-writable?
   */
  g_settings_backend_path_writable_changed (G_SETTINGS_BACKEND (delayed),
                                            path);
}

static void
g_delayed_settings_backend_finalize (GObject *object)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (object);

  g_settings_backend_unwatch (delayed->priv->backend, delayed);
  g_object_unref (delayed->priv->backend);
}

static void
g_delayed_settings_backend_class_init (GDelayedSettingsBackendClass *class)
{
  GSettingsBackendClass *backend_class = G_SETTINGS_BACKEND_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  g_type_class_add_private (class, sizeof (GDelayedSettingsBackendPrivate));

  backend_class->read = g_delayed_settings_backend_read;
  backend_class->write = g_delayed_settings_backend_write;
  backend_class->write_keys = g_delayed_settings_backend_write_keys;
  backend_class->reset = g_delayed_settings_backend_reset;
  backend_class->reset_path = g_delayed_settings_backend_reset_path;
  backend_class->get_writable = g_delayed_settings_backend_get_writable;
  backend_class->subscribe = g_delayed_settings_backend_subscribe;
  backend_class->unsubscribe = g_delayed_settings_backend_unsubscribe;

  object_class->finalize = g_delayed_settings_backend_finalize;
}

static void
g_delayed_settings_backend_init (GDelayedSettingsBackend *delayed)
{
  delayed->priv =
    G_TYPE_INSTANCE_GET_PRIVATE (delayed, G_TYPE_DELAYED_SETTINGS_BACKEND,
                                 GDelayedSettingsBackendPrivate);

  delayed->priv->delayed = g_settings_backend_create_tree ();
}

GDelayedSettingsBackend *
g_delayed_settings_backend_new (GSettingsBackend *backend,
                                gpointer          owner)
{
  GDelayedSettingsBackend *delayed;

  delayed = g_object_new (G_TYPE_DELAYED_SETTINGS_BACKEND, NULL);
  delayed->priv->backend = g_object_ref (backend);
  delayed->priv->owner = owner;

  g_settings_backend_watch (delayed->priv->backend,
                            delayed_backend_changed,
                            delayed_backend_path_changed,
                            delayed_backend_keys_changed,
                            delayed_backend_writable_changed,
                            delayed_backend_path_writable_changed,
                            delayed);

  return delayed;
}
