/*
 * Copyright © 2009 Codethink Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of version 3 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * See the included COPYING file for more information.
 */

#include "gdelayedsettingsbackend.h"

#include <string.h>

#include "gioalias.h"

enum
{
  PROP_NONE,
  PROP_BACKEND,
  PROP_BASE_PATH,
  PROP_HAS_UNAPPLIED
};

struct _GDelayedSettingsBackendPrivate {
  GSettingsBackend *backend;
  guint handler_id;
  gchar *base_path;
  GTree *delayed;
};

G_DEFINE_TYPE (GDelayedSettingsBackend,
               g_delayed_settings_backend,
               G_TYPE_SETTINGS_BACKEND)

static gboolean
g_delayed_settings_backend_add_to_tree (gpointer key,
                                        gpointer value,
                                        gpointer user_data)
{
  gpointer *args = user_data;

  g_tree_insert (args[0],
                 g_strjoin (NULL, args[1], key, NULL),
                 g_variant_ref (value));

  return FALSE;
}

static void
g_delayed_settings_backend_write (GSettingsBackend *backend,
                                  const gchar      *prefix,
                                  GTree            *tree,
                                  gpointer          origin_tag)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (backend);
  gconstpointer args[2] = { delayed->priv->delayed, prefix };
  gboolean was_empty;

  was_empty = g_tree_nnodes (delayed->priv->delayed) == 0;

  g_tree_foreach (tree, g_delayed_settings_backend_add_to_tree, args);

  g_settings_backend_changed_tree (backend, prefix, tree, origin_tag);

  if (was_empty)
    g_object_notify (G_OBJECT (delayed), "has-unapplied");
}

static GVariant *
g_delayed_settings_backend_read (GSettingsBackend   *backend,
                                 const gchar        *key,
                                 const GVariantType *expected_type)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (backend);
  GVariant *result = NULL;
  gchar *path;

  if ((result = g_tree_lookup (delayed->priv->delayed, key))) 
    return g_variant_ref (result);

  path = g_strconcat (delayed->priv->base_path, key, NULL);
  result = g_settings_backend_read (delayed->priv->backend,
                                    path, expected_type);
  g_free (path);

  return result;
}

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

      g_settings_backend_write (delayed->priv->backend,
                                delayed->priv->base_path,
                                tmp, delayed->priv);
      g_tree_unref (tmp);

      g_object_notify (G_OBJECT (delayed), "has-unapplied");
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
      g_settings_backend_changed_tree (G_SETTINGS_BACKEND (delayed),
                                       "", tmp, NULL);
      g_tree_destroy (tmp);

      g_object_notify (G_OBJECT (delayed), "has-unapplied");
    }
}

static gboolean
g_delayed_settings_backend_get_writable (GSettingsBackend *backend,
                                         const gchar      *name)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (backend);
  gboolean sensitive;
  gchar *path;

  path = g_strconcat (delayed->priv->base_path, name, NULL);
  sensitive = g_settings_backend_get_writable (delayed->priv->backend, path);
  g_free (path);

  return sensitive;
}

static void
g_delayed_settings_backend_subscribe (GSettingsBackend *base,
                                      const char       *name)
{
}

static void
g_delayed_settings_backend_get_property (GObject *object, guint prop_id,
                                         GValue *value, GParamSpec *pspec)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (object);

  switch (prop_id)
    {
      gboolean has;

    case PROP_HAS_UNAPPLIED:
      has = g_delayed_settings_backend_get_has_unapplied (delayed);
      g_value_set_boolean (value, has);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
g_delayed_settings_backend_set_property (GObject *object, guint prop_id,
                                         const GValue *value, GParamSpec *pspec)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (object);

  switch (prop_id)
    {
    case PROP_BACKEND:
      g_assert (delayed->priv->backend == NULL);
      delayed->priv->backend = g_value_dup_object (value);
      break;

    case PROP_BASE_PATH:
      g_assert (delayed->priv->base_path == NULL);
      delayed->priv->base_path = g_value_dup_string (value);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
g_delayed_settings_backend_backend_changed (GSettingsBackend    *backend,
                                            const gchar         *prefix,
                                            const gchar * const *items,
                                            gint                 n_items,
                                            gpointer             origin_tag,
                                            gpointer             user_data)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (user_data);

  if (origin_tag == delayed->priv)
    return;

  if (g_str_has_prefix (prefix, delayed->priv->base_path))
    {
      g_settings_backend_changed (G_SETTINGS_BACKEND (delayed),
                                  prefix + strlen (delayed->priv->base_path),
                                  items, n_items, origin_tag);
    }

  else if (g_str_has_prefix (delayed->priv->base_path, prefix))
    {
      const gchar **my_items;
      const gchar *relative;
      gint relative_length;
      gint i, j;

      relative = delayed->priv->base_path + strlen (prefix);
      relative_length = strlen (relative);

      my_items = g_new (const gchar *, n_items + 1);

      for (i = j = 0; i < n_items; i++)
        if (g_str_has_prefix (items[i], relative))
          my_items[j++] = items[i] + relative_length;
      my_items[j] = NULL;

      if (j > 0)
        g_settings_backend_changed (G_SETTINGS_BACKEND (delayed),
                                    "", my_items, j, origin_tag);
      g_free (my_items);
    }

  else
    /* do nothing */;
}

static void
g_delayed_settings_backend_constructed (GObject *object)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (object);

  g_assert (delayed->priv->backend != NULL);
  g_assert (delayed->priv->base_path != NULL);

  delayed->priv->handler_id = 
    g_signal_connect (delayed->priv->backend, "changed",
                      G_CALLBACK (g_delayed_settings_backend_backend_changed),
                      delayed);

  g_settings_backend_subscribe (delayed->priv->backend,
                                delayed->priv->base_path);
}

static void
g_delayed_settings_backend_finalize (GObject *object)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (object);

  g_signal_handler_disconnect (delayed->priv->backend,
                               delayed->priv->handler_id);
  g_settings_backend_unsubscribe (delayed->priv->backend,
                                  delayed->priv->base_path);
  g_object_unref (delayed->priv->backend);
  g_free (delayed->priv->base_path);
}

static void
g_delayed_settings_backend_class_init (GDelayedSettingsBackendClass *class)
{
  GSettingsBackendClass *backend_class = G_SETTINGS_BACKEND_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  g_type_class_add_private (class, sizeof (GDelayedSettingsBackendPrivate));

  backend_class->write = g_delayed_settings_backend_write;
  backend_class->read = g_delayed_settings_backend_read;
  backend_class->get_writable = g_delayed_settings_backend_get_writable;
  backend_class->subscribe = g_delayed_settings_backend_subscribe;
  backend_class->unsubscribe = g_delayed_settings_backend_subscribe;

  object_class->get_property = g_delayed_settings_backend_get_property;
  object_class->set_property = g_delayed_settings_backend_set_property;
  object_class->constructed = g_delayed_settings_backend_constructed;
  object_class->finalize = g_delayed_settings_backend_finalize;

  g_object_class_install_property (object_class, PROP_BACKEND,
    g_param_spec_object ("backend", "backend backend", "backend",
                         G_TYPE_SETTINGS_BACKEND, G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_BASE_PATH,
    g_param_spec_string ("base-path", "base path", "base",
                         "", G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_HAS_UNAPPLIED,
    g_param_spec_boolean ("has-unapplied", "has unapplied", "unapplied",
                          FALSE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

}

static void
g_delayed_settings_backend_init (GDelayedSettingsBackend *delayed)
{
  delayed->priv =
    G_TYPE_INSTANCE_GET_PRIVATE (delayed, G_TYPE_DELAYED_SETTINGS_BACKEND,
                                 GDelayedSettingsBackendPrivate);

  delayed->priv->delayed = g_settings_backend_create_tree ();
}

GSettingsBackend *
g_delayed_settings_backend_new (GSettingsBackend *backend,
                       const gchar      *base_path)
{
  return g_object_new (G_TYPE_DELAYED_SETTINGS_BACKEND,
                       "backend", backend,
                       "base-path", base_path,
                       NULL);
}

#define _gsettingsdelayedbackend_c_
#include "gioaliasdef.c"
