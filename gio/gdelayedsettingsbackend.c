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


struct _GDelayedSettingsBackendPrivate
{
  GSettingsBackend *backend;
  GMutex lock;
  GTree *delayed;
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
  gpointer result = NULL;

  if (!default_value)
    {
      g_mutex_lock (&delayed->priv->lock);
      if (g_tree_lookup_extended (delayed->priv->delayed, key, NULL, &result))
        {
          /* NULL in the tree means we should consult the default value */
          if (result != NULL)
            g_variant_ref (result);
          else
            default_value = TRUE;
        }
      g_mutex_unlock (&delayed->priv->lock);
    }

  if (result == NULL)
    result = g_settings_backend_read (delayed->priv->backend, key,
                                      expected_type, default_value);

  return result;
}

static gboolean
g_delayed_settings_backend_write (GSettingsBackend *backend,
                                  const gchar      *key,
                                  GVariant         *value,
                                  gpointer          origin_tag)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (backend);

  g_mutex_lock (&delayed->priv->lock);
  g_tree_insert (delayed->priv->delayed, g_strdup (key), g_variant_ref_sink (value));
  g_mutex_unlock (&delayed->priv->lock);

  g_settings_backend_changed (backend, key, origin_tag);
  g_settings_backend_set_has_unapplied (backend, TRUE);

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
g_delayed_settings_backend_write_tree (GSettingsBackend *backend,
                                       GTree            *tree,
                                       gpointer          origin_tag)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (backend);

  g_mutex_lock (&delayed->priv->lock);
  g_tree_foreach (tree, add_to_tree, delayed->priv->delayed);
  g_mutex_unlock (&delayed->priv->lock);

  g_settings_backend_changed_tree (backend, tree, origin_tag);
  g_settings_backend_set_has_unapplied (backend, TRUE);

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
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (backend);

  g_mutex_lock (&delayed->priv->lock);
  g_tree_insert (delayed->priv->delayed, g_strdup (key), NULL);
  g_mutex_unlock (&delayed->priv->lock);

  g_settings_backend_changed (backend, key, origin_tag);
  g_settings_backend_set_has_unapplied (backend, TRUE);
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

static GPermission *
g_delayed_settings_backend_get_permission (GSettingsBackend *backend,
                                           const gchar      *path)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (backend);

  return g_settings_backend_get_permission (delayed->priv->backend, path);
}


/* method calls */
void
g_delayed_settings_backend_apply (GDelayedSettingsBackend *delayed)
{
  if (g_tree_nnodes (delayed->priv->delayed) > 0)
    {
      gboolean success;
      GTree *tmp;

      g_mutex_lock (&delayed->priv->lock);
      tmp = delayed->priv->delayed;
      delayed->priv->delayed = g_settings_backend_create_tree ();
      success = g_settings_backend_write_tree (delayed->priv->backend,
                                               tmp, delayed->priv);
      g_mutex_unlock (&delayed->priv->lock);

      if (!success)
        g_settings_backend_changed_tree (G_SETTINGS_BACKEND (delayed),
                                         tmp, NULL);

      g_tree_unref (tmp);

      g_settings_backend_set_has_unapplied (G_SETTINGS_BACKEND (delayed), FALSE);
    }
}

void
g_delayed_settings_backend_revert (GDelayedSettingsBackend *delayed)
{
  if (g_tree_nnodes (delayed->priv->delayed) > 0)
    {
      GTree *tmp;

      g_mutex_lock (&delayed->priv->lock);
      tmp = delayed->priv->delayed;
      delayed->priv->delayed = g_settings_backend_create_tree ();
      g_mutex_unlock (&delayed->priv->lock);
      g_settings_backend_changed_tree (G_SETTINGS_BACKEND (delayed), tmp, NULL);
      g_tree_unref (tmp);

      g_settings_backend_set_has_unapplied (G_SETTINGS_BACKEND (delayed), FALSE);
    }
}

static void
g_delayed_settings_got_event (GObject              *target,
                              const GSettingsEvent *event)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (target);

  if (event->origin_tag != delayed->priv)
    g_settings_backend_report_event (G_SETTINGS_BACKEND (delayed), event);
}

static void
g_delayed_settings_backend_finalize (GObject *object)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (object);

  g_mutex_clear (&delayed->priv->lock);
  g_object_unref (delayed->priv->backend);
  g_tree_unref (delayed->priv->delayed);

  G_OBJECT_CLASS (g_delayed_settings_backend_parent_class)
    ->finalize (object);
}

static void
g_delayed_settings_backend_class_init (GDelayedSettingsBackendClass *class)
{
  GSettingsBackendClass *backend_class = G_SETTINGS_BACKEND_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  g_type_class_add_private (class, sizeof (GDelayedSettingsBackendPrivate));

  backend_class->read = g_delayed_settings_backend_read;
  backend_class->write = g_delayed_settings_backend_write;
  backend_class->write_tree = g_delayed_settings_backend_write_tree;
  backend_class->reset = g_delayed_settings_backend_reset;
  backend_class->get_writable = g_delayed_settings_backend_get_writable;
  backend_class->subscribe = g_delayed_settings_backend_subscribe;
  backend_class->unsubscribe = g_delayed_settings_backend_unsubscribe;
  backend_class->get_permission = g_delayed_settings_backend_get_permission;

  object_class->finalize = g_delayed_settings_backend_finalize;
}

static void
g_delayed_settings_backend_init (GDelayedSettingsBackend *delayed)
{
  delayed->priv =
    G_TYPE_INSTANCE_GET_PRIVATE (delayed, G_TYPE_DELAYED_SETTINGS_BACKEND,
                                 GDelayedSettingsBackendPrivate);

  delayed->priv->delayed = g_settings_backend_create_tree ();
  g_mutex_init (&delayed->priv->lock);
}

GDelayedSettingsBackend *
g_delayed_settings_backend_new (GSettingsBackend *backend)
{
  GDelayedSettingsBackend *delayed;

  delayed = g_object_new (G_TYPE_DELAYED_SETTINGS_BACKEND, NULL);
  delayed->priv->backend = g_object_ref (backend);

  g_settings_backend_watch (delayed->priv->backend, g_delayed_settings_got_event, G_OBJECT (delayed));

  return delayed;
}
