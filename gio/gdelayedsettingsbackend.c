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
  PROP_HAS_UNAPPLIED
};

struct _GDelayedSettingsBackendPrivate {
  GSettingsBackend *backend;
  guint writable_changed_handler_id;
  guint keys_changed_handler_id;
  guint changed_handler_id;
  GTree *delayed;
};

G_DEFINE_TYPE (GDelayedSettingsBackend,
               g_delayed_settings_backend,
               G_TYPE_SETTINGS_BACKEND)

static void
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

  if (was_empty)
    g_object_notify (G_OBJECT (delayed), "has-unapplied");
}

static gboolean
add_to_tree (gpointer key,
             gpointer value,
             gpointer user_data)
{
  g_tree_insert (user_data, g_strdup (key), g_variant_ref (value));
  return FALSE;
}

static void
g_delayed_settings_backend_write_keys (GSettingsBackend *backend,
                                       GTree            *tree,
                                       gpointer          origin_tag)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (backend);
  gboolean was_empty;

  was_empty = g_tree_nnodes (delayed->priv->delayed) == 0;

  g_tree_foreach (tree, add_to_tree, delayed->priv->delayed);

  g_settings_backend_changed_tree (backend, tree, origin_tag);

  if (was_empty)
    g_object_notify (G_OBJECT (delayed), "has-unapplied");
}

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
      g_settings_backend_changed_tree (G_SETTINGS_BACKEND (delayed), tmp, NULL);
      g_tree_destroy (tmp);

      g_object_notify (G_OBJECT (delayed), "has-unapplied");
    }
}

static gboolean
g_delayed_settings_backend_get_writable (GSettingsBackend *backend,
                                         const gchar      *name)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (backend);

  return g_settings_backend_get_writable (delayed->priv->backend, name);
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

    default:
      g_assert_not_reached ();
    }
}

static void
delayed_backend_changed (GSettingsBackend *backend,
                         const gchar      *name,
                         gpointer          origin_tag,
                         gpointer          user_data)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (user_data);

  if (origin_tag != delayed->priv)
    g_settings_backend_changed (backend, name, origin_tag);
}

static void
delayed_backend_keys_changed (GSettingsBackend    *backend,
                              const gchar         *prefix,
                              const gchar * const *items,
                              gpointer             origin_tag,
                              gpointer             user_data)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (user_data);

  if (origin_tag != delayed->priv)
    g_settings_backend_keys_changed (backend, prefix, items, origin_tag);
}

static void
delayed_backend_writable_changed (GSettingsBackend *backend,
                                  const gchar      *name,
                                  gpointer          user_data)
{
  /* XXX: maybe drop keys from the delayed-apply settings
   *      if they became non-writable?
   */
  g_settings_backend_writable_changed (backend, name);
}

static void
g_delayed_settings_backend_constructed (GObject *object)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (object);

  g_assert (delayed->priv->backend != NULL);

  delayed->priv->changed_handler_id = 
    g_signal_connect (delayed->priv->backend, "changed",
                      G_CALLBACK (delayed_backend_changed),
                      delayed);

 delayed->priv->keys_changed_handler_id = 
    g_signal_connect (delayed->priv->backend, "keys-changed",
                      G_CALLBACK (delayed_backend_keys_changed),
                      delayed);

 delayed->priv->writable_changed_handler_id = 
    g_signal_connect (delayed->priv->backend, "writable-changed",
                      G_CALLBACK (delayed_backend_writable_changed),
                      delayed);
}

static void
g_delayed_settings_backend_finalize (GObject *object)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (object);

  g_signal_handler_disconnect (delayed->priv->backend,
                               delayed->priv->changed_handler_id);
  g_signal_handler_disconnect (delayed->priv->backend,
                               delayed->priv->keys_changed_handler_id);
  g_signal_handler_disconnect (delayed->priv->backend,
                               delayed->priv->writable_changed_handler_id);
  g_object_unref (delayed->priv->backend);
}

static void
g_delayed_settings_backend_class_init (GDelayedSettingsBackendClass *class)
{
  GSettingsBackendClass *backend_class = G_SETTINGS_BACKEND_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  g_type_class_add_private (class, sizeof (GDelayedSettingsBackendPrivate));

  backend_class->write = g_delayed_settings_backend_write;
  backend_class->write_keys = g_delayed_settings_backend_write_keys;
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
g_delayed_settings_backend_new (GSettingsBackend *backend)
{
  return g_object_new (G_TYPE_DELAYED_SETTINGS_BACKEND,
                       "backend", backend, NULL);
}

#define _gsettingsdelayedbackend_c_
#include "gioaliasdef.c"
