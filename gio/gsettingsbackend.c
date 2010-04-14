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

#include "gsettingsbackend.h"
#include "gmemorysettingsbackend.h"
#include "giomodule-priv.h"
#include "gio-marshal.h"

#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <glibintl.h>

#include "gioalias.h"

struct _GSettingsBackendPrivate
{
  gchar *context;
};

G_DEFINE_ABSTRACT_TYPE (GSettingsBackend, g_settings_backend, G_TYPE_OBJECT)

static guint changed_signal;

enum {
  PROP_ZERO,
  PROP_CONTEXT
};

/**
 * SECTION:gsettingsbackend
 * @short_description: an interface for settings backend implementations
 *
 * The #GSettingsBackend interface defines a generic interface for
 * non-strictly-typed data that is stored in a hierarchy.
 *
 * The interface defines methods for reading and writing values, a
 * method for determining if writing of certain values will fail
 * (lockdown) and a change notification signal.
 *
 * The semantics of the interface are very precisely defined and
 * implementations must carefully adhere to the expectations of
 * callers that are documented on each of the interface methods.
 *
 * Some of the GSettingsBackend functions accept or return a #GTree.
 * These trees always have strings as keys and #GVariant as values.
 * g_settings_backend_create_tree() is a convenience function to create
 * suitable trees.
 **/

/**
 * g_settings_backend_changed:
 * @backend: a #GSettingsBackend implementation
 * @prefix: a common prefix of the changed keys
 * @items: the %NULL-terminated list of changed keys
 * @n_items: the number of items in @items. May be -1
 * @origin_tag: the origin tag
 *
 * Emits the changed signal on @backend.  This function should only be
 * called by the implementation itself, to indicate that a change has
 * occurred.
 *
 * The list of changed keys, relative to the root of the settings store,
 * is @prefix prepended to each of the @items.  It must either be the
 * case that @prefix is equal to "" or ends in "/" or that @items
 * contains exactly one item: "".  @prefix need not be the largest
 * possible prefix.
 *
 * The implementation must call this function during any call to
 * g_settings_backend_write(), before the call returns (except in the
 * case that no keys are actually changed).  It may not rely on the
 * existence of a mainloop for dispatching the signal later.
 *
 * The implementation may call this function at any other time it likes
 * in response to other events (such as changes occuring outside of the
 * program).  These calls may originate from a mainloop or may originate
 * in response to any other action (including from calls to
 * g_settings_backend_write()).
 *
 * In the case that this call is in response to a call to
 * g_settings_backend_write() then @origin_tag must be set to the same
 * value that was passed to that call.
 *
 * Since: 2.26
 */
void
g_settings_backend_changed (GSettingsBackend    *backend,
                            const gchar         *prefix,
                            gchar const * const *items,
                            gint                 n_items,
                            gpointer             origin_tag)
{
  if (n_items == -1)
    for (n_items = 0; items[n_items]; n_items++);

  g_assert (items[n_items] == NULL);

  g_signal_emit (backend, changed_signal, 0,
                 prefix, items, n_items, origin_tag);
}

static gboolean
g_settings_backend_append_to_list (gpointer key,
                                   gpointer value,
                                   gpointer user_data)
{
  return (*((*((gchar ***) user_data))++) = key, FALSE);
}

/**
 * g_settings_backend_changed_tree:
 * @backend: a #GSettingsBackend implementation
 * @prefix: a common prefix of the changed keys
 * @tree: a #GTree containing the changes
 * @origin_tag: the origin tag
 *
 * This call is a convenience wrapper around
 * g_settings_backend_changed().  It gets the list of changes from
 * @tree.
 *
 * Since: 2.26
 **/
void
g_settings_backend_changed_tree (GSettingsBackend *backend,
                                 const gchar      *prefix,
                                 GTree            *tree,
                                 gpointer          origin_tag)
{
  gchar **list;

  list = g_new (gchar *, g_tree_nnodes (tree) + 1);

  {
    gchar **ptr = list;
    g_tree_foreach (tree, g_settings_backend_append_to_list, &ptr);
    *ptr = NULL;

    g_assert (list + g_tree_nnodes (tree) == ptr);
  }

  g_signal_emit (backend, changed_signal, 0,
                 prefix, list, g_tree_nnodes (tree), origin_tag);
  g_free (list);
}

/**
 * g_settings_backend_read:
 * @backend: a #GSettingsBackend implementation
 * @key: the key to read
 * @expected_type: a #GVariantType hint
 * @returns: the value that was read, or %NULL
 *
 * Reads a key. This call will never block.
 *
 * If the key exists, the value associated with it will be returned.
 * If the key does not exist, %NULL will be returned.
 *
 * If @expected_type is given, it serves as a type hint to the backend.
 * If you expect a key of a certain type then you should give
 * @expected_type to increase your chances of getting it.  Some backends
 * may ignore this argument and return values of a different type; it is
 * mostly used by backends that don't store strong type information.
 *
 * Since: 2.26
 **/
GVariant *
g_settings_backend_read (GSettingsBackend   *backend,
                         const gchar        *key,
                         const GVariantType *expected_type)
{
  return G_SETTINGS_BACKEND_GET_CLASS (backend)
    ->read (backend, key, expected_type);
}

/**
 * g_settings_backend_write:
 * @backend: a #GSettingsBackend implementation
 * @prefix: the longest common prefix
 * @values: a #GTree containing key-value pairs to write
 * @origin_tag: the origin tag
 *
 * Writes one or more keys.  This call will never block.
 *
 * For each item in @values, a key is written.  The key to be written is
 * @prefix prepended to the key used in the tree.  The value stored in
 * the tree is expected to be a #GVariant instance.  It must either be
 * the case that @prefix is equal to "" or ends in "/" or that @values
 * contains exactly one item, with a key of "".  @prefix need not be the
 * largest possible prefix.
 *
 * This call does not fail.  During this call a #GSettingsBackend::changed
 * signal will be emitted if any keys have been changed.  The new values of
 * all updated keys will be visible to any signal callbacks.
 *
 * One possible method that an implementation might deal with failures is
 * to emit a second "changed" signal (either during this call, or later)
 * to indicate that the affected keys have suddenly "changed back" to their
 * old values.
 *
 * Since: 2.26
 **/
void
g_settings_backend_write (GSettingsBackend *backend,
                          const gchar      *prefix,
                          GTree            *values,
                          gpointer          origin_tag)
{
  G_SETTINGS_BACKEND_GET_CLASS (backend)
    ->write (backend, prefix, values, origin_tag);
}

/**
 * g_settings_backend_get_writable:
 * @backend: a #GSettingsBackend implementation
 * @name: the name of a key, relative to the root of the backend
 * @returns: %TRUE if the key is writable
 *
 * Finds out if a key is available for writing to.  This is the
 * interface through which 'lockdown' is implemented.  Locked down
 * keys will have %FALSE returned by this call.
 *
 * You should not write to locked-down keys, but if you do, the
 * implementation will deal with it.
 *
 * Since: 2.26
 **/
gboolean
g_settings_backend_get_writable (GSettingsBackend *backend,
                                 const gchar      *name)
{
  return G_SETTINGS_BACKEND_GET_CLASS (backend)
    ->get_writable (backend, name);
}

/**
 * g_settings_backend_unsubscribe:
 * @backend: a #GSettingsBackend
 * @name: a key or path to subscribe to
 *
 * Reverses the effect of a previous call to
 * g_settings_backend_subscribe().
 *
 * Since: 2.26
 **/
void
g_settings_backend_unsubscribe (GSettingsBackend *backend,
                                const char       *name)
{
  G_SETTINGS_BACKEND_GET_CLASS (backend)
    ->unsubscribe (backend, name);
}

/**
 * g_settings_backend_subscribe:
 * @backend: a #GSettingsBackend
 * @name: a key or path to subscribe to
 *
 * Requests that change signals be emitted for events on @name.
 *
 * Since: 2.26
 **/
void
g_settings_backend_subscribe (GSettingsBackend *backend,
                              const gchar      *name)
{
  G_SETTINGS_BACKEND_GET_CLASS (backend)
    ->subscribe (backend, name);
}

static void
g_settings_backend_set_property (GObject         *object,
                                 guint            prop_id,
                                 const GValue    *value,
                                 GParamSpec      *pspec)
{
  GSettingsBackend *backend = G_SETTINGS_BACKEND (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      backend->priv->context = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_settings_backend_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GSettingsBackend *backend = G_SETTINGS_BACKEND (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_string (value, backend->priv->context);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_settings_backend_finalize (GObject *object)
{
  GSettingsBackend *backend = G_SETTINGS_BACKEND (object);

  g_free (backend->priv->context);

  G_OBJECT_CLASS (g_settings_backend_parent_class)->finalize (object);
}

static void
ignore_subscription (GSettingsBackend *backend,
                     const gchar      *key)
{
}

static void
g_settings_backend_class_init (GSettingsBackendClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  class->subscribe = ignore_subscription;
  class->unsubscribe = ignore_subscription;

  gobject_class->get_property = g_settings_backend_get_property;
  gobject_class->set_property = g_settings_backend_set_property;
  gobject_class->finalize = g_settings_backend_finalize;

  g_type_class_add_private (class, sizeof (GSettingsBackendPrivate));

  /**
   * GSettingsBackend::changed:
   * @backend: the object on which the signal was emitted
   * @prefix: a common prefix of the changed keys
   * @items: the list of changed keys
   * @n_items: the number of items in @items. May be -1 if @items is
   *     %NULL-terminated
   * @origin_tag: the origin tag
   *
   * The "changed" signal gets emitted when one or more keys change
   * in the #GSettingsBackend store.  The new values of all updated
   * keys will be visible to any signal callbacks.
   *
   * The list of changed keys, relative to the root of the settings store,
   * is @prefix prepended to each of the @items.  It must either be the
   * case that @prefix is equal to "" or ends in "/" or that @items
   * contains exactly one item: "".  @prefix need not be the largest
   * possible prefix.
   *
   * Since: 2.26
   */
  changed_signal =
    g_signal_new ("changed", G_TYPE_SETTINGS_BACKEND,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GSettingsBackendClass, changed),
                  NULL, NULL,
                  _gio_marshal_VOID__STRING_BOXED_INT_POINTER, G_TYPE_NONE,
                  4, G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                  G_TYPE_STRV | G_SIGNAL_TYPE_STATIC_SCOPE,
                  G_TYPE_INT, G_TYPE_POINTER);

  /**
   * GSettingsBackend:context:
   *
   * The "context" property gives a hint to the backend as to
   * what storage to use. It is up to the implementation to make
   * use of this information.
   *
   * E.g. DConf supports "user", "system", "defaults" and "login"
   * contexts.
   *
   * If your backend supports different contexts, you should also
   * provide an implementation of the supports_context() class
   * function in #GSettingsBackendClass.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class, PROP_CONTEXT,
    g_param_spec_string ("context",
                         P_("Context"),
                         P_("A context to use when deciding which storage to use"),
                         NULL,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

}

/**
 * g_settings_backend_create_tree:
 * @returns: a new #GTree
 *
 * This is a convenience function for creating a tree that is compatible
 * with g_settings_backend_write().  It merely calls g_tree_new_full()
 * with strcmp(), g_free() and g_variant_unref().
 *
 * Since: 2.26
 **/
GTree *
g_settings_backend_create_tree (void)
{
  return g_tree_new_full ((GCompareDataFunc) strcmp, NULL,
                          g_free, (GDestroyNotify) g_variant_unref);
}

static gpointer
get_default_backend (const gchar *context)
{
  GIOExtension *extension = NULL;
  GIOExtensionPoint *point;
  GList *extensions;
  const gchar *env;
  GType type;

  _g_io_modules_ensure_loaded ();

  point = g_io_extension_point_lookup (G_SETTINGS_BACKEND_EXTENSION_POINT_NAME);

  if ((env = getenv ("GSETTINGS_BACKEND")))
    {
      extension = g_io_extension_point_get_extension_by_name (point, env);

      if (extension == NULL)
        g_warning ("Can't find GSettings backend '%s' given in "
                   "GSETTINGS_BACKEND environment variable", env);
    }

  if (extension == NULL)
    {
      extensions = g_io_extension_point_get_extensions (point);

      if (extensions == NULL)
        g_error ("No GSettingsBackend implementations exist.");

      extension = extensions->data;
    }

  if (context)
    {
      GTypeClass *class;

      class = g_io_extension_ref_class (extension);

      if (!g_settings_backend_class_supports_context (G_SETTINGS_BACKEND_CLASS (class), context))
        {
          g_type_class_unref (class);
          return NULL;
        }

      g_type_class_unref (class);
    }

  type = g_io_extension_get_type (extension);

  return g_object_new (type, "context", context, NULL);
}

/**
 * g_settings_backend_get_with_context:
 * @context: a context that might be used by the backend to determine
 *     which storage to use, or %NULL to use the default storage
 * @returns: the default #GSettingsBackend
 *
 * Returns the default #GSettingsBackend. It is possible to override
 * the default by setting the <envvar>GSETTINGS_BACKEND</envvar>
 * environment variable to the name of a settings backend.
 *
 * The @context parameter can be used to indicate that a different
 * than the default storage is desired. E.g. the DConf backend lets
 * you use "user", "system", "defaults" and "login" as contexts.
 *
 * If @context is not supported by the implementation, this function
 * returns an instance of the #GSettingsMemoryBackend.
 * See g_settings_backend_supports_context(),
 *
 * The user does not own the return value and it must not be freed.
 *
 * Since: 2.26
 **/
GSettingsBackend *
g_settings_backend_get_with_context (const gchar *context)
{
  static GHashTable *backends;
  GSettingsBackend *backend;

  _g_io_modules_ensure_extension_points_registered ();
  G_TYPE_MEMORY_SETTINGS_BACKEND;

  /* FIXME: hash null properly? */
  if (!context)
    context = "";

  if (!backends)
    backends = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  backend = g_hash_table_lookup (backends, context);

  if (backend)
    return backend;

  backend = get_default_backend (context);

  if (!backend)
    {
      /* FIXME: create an instance of the memory backend */
    }

  g_hash_table_insert (backends, g_strdup (context), backend);

  return backend;
}

/**
 * g_settings_backend_supports_context:
 * @context: a context string that might be passed to
 *     g_settings_backend_new_with_context()
 * @returns: #TRUE if @context is supported
 *
 * Determines if the given context is supported by the default
 * GSettingsBackend implementation.
 *
 * Since: 2.26
 */
gboolean
g_settings_backend_supports_context (const gchar *context)
{
  GSettingsBackend *backend;

  backend = get_default_backend (context);

  if (backend)
    {
      g_object_unref (backend);
      return TRUE;
    }

  return FALSE;
}

static void
g_settings_backend_init (GSettingsBackend *backend)
{
  backend->priv = g_type_instance_get_private (backend, G_TYPE_SETTINGS_BACKEND);
}

gboolean
g_settings_backend_class_supports_context (GSettingsBackendClass *klass,
                                           const gchar           *context)
{
  if (klass->supports_context)
    return (klass->supports_context) (klass, context);

  return TRUE;
}

