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

#include "config.h"

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
                                 const GVariantType *expected_type,
                                 gboolean            default_value)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (backend);
  GVariant *result;

  if (!default_value &&
      (result = g_tree_lookup (delayed->priv->delayed, key)))
    return g_variant_ref (result);

  return g_settings_backend_read (delayed->priv->backend,
                                  key, expected_type, default_value);
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
  if (g_tree_nnodes (delayed->priv->delayed) > 0)
    {
      gboolean success;
      GTree *tmp;

      tmp = delayed->priv->delayed;
      delayed->priv->delayed = g_settings_backend_create_tree ();
      success = g_settings_backend_write_keys (delayed->priv->backend,
                                               tmp, delayed->priv);

      if (!success)
        g_settings_backend_changed_tree (G_SETTINGS_BACKEND (delayed),
                                         tmp, NULL);

      g_tree_unref (tmp);

      if (delayed->priv->owner)
        g_object_notify (delayed->priv->owner, "has-unapplied");
    }
}

void
g_delayed_settings_backend_revert (GDelayedSettingsBackend *delayed)
{
  if (g_tree_nnodes (delayed->priv->delayed) > 0)
    {
      GTree *tmp;

      tmp = delayed->priv->delayed;
      delayed->priv->delayed = g_settings_backend_create_tree ();
      g_settings_backend_changed_tree (G_SETTINGS_BACKEND (delayed), tmp, NULL);
      g_tree_unref (tmp);

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

  if (g_tree_lookup (delayed->priv->delayed, key) &&
      !g_settings_backend_get_writable (delayed->priv->backend, key))
    {
      /* drop the key from our changeset if it just became read-only.
       * no need to signal this since the writable change implies it.
       */
      g_tree_remove (delayed->priv->delayed, key);

      /* if that was the only key... */
      if (delayed->priv->owner &&
          g_tree_nnodes (delayed->priv->delayed) == 0)
        g_object_notify (delayed->priv->owner, "has-unapplied");
    }

  g_settings_backend_writable_changed (G_SETTINGS_BACKEND (delayed), key);
}

/* slow method until we get foreach-with-remove in GTree
 */
typedef struct
{
  const gchar *path;
  const gchar **keys;
  gsize index;
} CheckPrefixState;

static gboolean
check_prefix (gpointer key,
              gpointer value,
              gpointer data)
{
  CheckPrefixState *state = data;

  if (g_str_has_prefix (key, state->path))
    state->keys[state->index++] = key;

  return FALSE;
}

static void
delayed_backend_path_writable_changed (GSettingsBackend *backend,
                                       const gchar      *path,
                                       gpointer          user_data)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (user_data);
  gsize n_keys;

  n_keys = g_tree_nnodes (delayed->priv->delayed);

  if (n_keys > 0)
    {
      CheckPrefixState state = { path, g_new (const gchar *, n_keys) };
      gsize i;

      /* collect a list of possibly-affected keys (ie: matching the path) */
      g_tree_foreach (delayed->priv->delayed, check_prefix, &state);

      /* drop the keys that have been affected */
      for (i = 0; i < state.index; i++)
        if (!g_settings_backend_get_writable (delayed->priv->backend,
                                              state.keys[i]))
          g_tree_remove (delayed->priv->delayed, state.keys[i]);

      g_free (state.keys);

      if (delayed->priv->owner &&
          g_tree_nnodes (delayed->priv->delayed) == 0)
        g_object_notify (delayed->priv->owner, "has-unapplied");
    }

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
