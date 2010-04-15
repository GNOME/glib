/*
 * Copyright © 2009, 2010 Codethink Limited
 * Copyright © 2010 Red Hat, Inc.
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
 * Authors: Ryan Lortie <desrt@desrt.ca>
 *          Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#define G_SETTINGS_ENABLE_BACKEND
#include "gsettingsbackendinternal.h"
#include "gmemorysettingsbackend.h"
#include "giomodule-priv.h"
#include "gio-marshal.h"

#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <glibintl.h>

#include "gioalias.h"

G_DEFINE_ABSTRACT_TYPE (GSettingsBackend, g_settings_backend, G_TYPE_OBJECT)

typedef struct _GSettingsBackendWatch GSettingsBackendWatch;

struct _GSettingsBackendPrivate
{
  GSettingsBackendWatch *watches;
  gchar *context;
};

enum
{
  PROP_0,
  PROP_CONTEXT
};

/**
 * SECTION:gsettingsbackend
 * @title: GSettingsBackend
 * @short_description: an interface for settings backend implementations
 * @include: gio/gsettingsbackend.h
 * @see_also: #GSettings, #GIOExtensionPoint
 *
 * The #GSettingsBackend interface defines a generic interface for
 * non-strictly-typed data that is stored in a hierarchy. To implement
 * an alternative storage backend for #GSettings, you need to implement
 * the #GSettingsBackend interface and then make it implement the
 * extension point #G_SETTINGS_BACKEND_EXTENSION_POINT_NAME.
 *
 * The interface defines methods for reading and writing values, a
 * method for determining if writing of certain values will fail
 * (lockdown) and a change notification mechanism.
 *
 * The semantics of the interface are very precisely defined and
 * implementations must carefully adhere to the expectations of
 * callers that are documented on each of the interface methods.
 *
 * Some of the GSettingsBackend functions accept or return a #GTree.
 * These trees always have strings as keys and #GVariant as values.
 * g_settings_backend_create_tree() is a convenience function to create
 * suitable trees.
 *
 * <note><para>
 * The #GSettingsBackend API is exported to allow third-party
 * implementations, but does not carry the same stability guarantees
 * as the public GIO API. For this reason, you have to define the
 * C preprocessor symbol #G_SETTINGS_ENABLE_BACKEND before including
 * <filename>gio/gsettingsbackend.h</filename>
 * </para></note>
 **/

struct _GSettingsBackendWatch
{
  GSettingsBackendChangedFunc              changed;
  GSettingsBackendPathChangedFunc          path_changed;
  GSettingsBackendKeysChangedFunc          keys_changed;
  GSettingsBackendWritableChangedFunc      writable_changed;
  GSettingsBackendPathWritableChangedFunc  path_writable_changed;
  gpointer                                 user_data;

  GSettingsBackendWatch                   *next;
};

void
g_settings_backend_watch (GSettingsBackend *backend,
  GSettingsBackendChangedFunc               changed,
  GSettingsBackendPathChangedFunc           path_changed,
  GSettingsBackendKeysChangedFunc           keys_changed,
  GSettingsBackendWritableChangedFunc       writable_changed,
  GSettingsBackendPathWritableChangedFunc   path_writable_changed,
                          gpointer          user_data)
{
  GSettingsBackendWatch *watch;

  watch = g_slice_new (GSettingsBackendWatch);
  watch->changed = changed;
  watch->path_changed = path_changed;
  watch->keys_changed = keys_changed;
  watch->writable_changed = writable_changed;
  watch->path_writable_changed = path_writable_changed;
  watch->user_data = user_data;

  watch->next = backend->priv->watches;
  backend->priv->watches = watch;
}

void
g_settings_backend_unwatch (GSettingsBackend *backend,
                            gpointer          user_data)
{
  GSettingsBackendWatch **ptr;

  for (ptr = &backend->priv->watches; *ptr; ptr = &(*ptr)->next)
    if ((*ptr)->user_data == user_data)
      {
        GSettingsBackendWatch *tmp = *ptr;

        *ptr = tmp->next;
        g_slice_free (GSettingsBackendWatch, tmp);

        return;
      }

  g_assert_not_reached ();
}

static gboolean
is_key (const gchar *key)
{
  gint length;
  gint i;

  g_return_val_if_fail (key != NULL, FALSE);
  g_return_val_if_fail (key[0] == '/', FALSE);

  for (i = 1; key[i]; i++)
    g_return_val_if_fail (key[i] != '/' || key[i + 1] != '/', FALSE);

  length = i;

  g_return_val_if_fail (key[length - 1] != '/', FALSE);

  return TRUE;
}

static gboolean
is_path (const gchar *path)
{
  gint length;
  gint i;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (path[0] == '/', FALSE);

  for (i = 1; path[i]; i++)
    g_return_val_if_fail (path[i] != '/' || path[i + 1] != '/', FALSE);

  length = i;

  g_return_val_if_fail (path[length - 1] == '/', FALSE);

  return TRUE;
}

/**
 * g_settings_backend_changed:
 * @backend: a #GSettingsBackend implementation
 * @key: the name of the key
 * @origin_tag: the origin tag
 *
 * Signals that a single key has possibly changed.  Backend
 * implementations should call this if a key has possibly changed its
 * value.
 *
 * @key must be a valid key (ie: starting with a slash, not containing
 * '//', and not ending with a slash).
 *
 * The implementation must call this function during any call to
 * g_settings_backend_write(), before the call returns (except in the
 * case that no keys are actually changed and it cares to detect this
 * fact).  It may not rely on the existence of a mainloop for
 * dispatching the signal later.
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
 **/
void
g_settings_backend_changed (GSettingsBackend *backend,
                            const gchar      *key,
                            gpointer          origin_tag)
{
  GSettingsBackendWatch *watch;

  g_return_if_fail (G_IS_SETTINGS_BACKEND (backend));
  g_return_if_fail (is_key (key));

  for (watch = backend->priv->watches; watch; watch = watch->next)
    watch->changed (backend, key, origin_tag, watch->user_data);
}

/**
 * g_settings_backend_keys_changed:
 * @backend: a #GSettingsBackend implementation
 * @path: the path containing the changes
 * @items: the %NULL-terminated list of changed keys
 * @origin_tag: the origin tag
 *
 * Signals that a list of keys have possibly changed.  Backend
 * implementations should call this if keys have possibly changed their
 * values.
 *
 * @path must be a valid path (ie: starting and ending with a slash and
 * not containing '//').  Each string in @items must form a valid key
 * name when @path is prefixed to it (ie: each item must not start or
 * end with '/' and must not contain '//').
 *
 * The meaning of this signal is that any of the key names resulting
 * from the contatenation of @path with each item in @items may have
 * changed.
 *
 * The same rules for when notifications must occur apply as per
 * g_settings_backend_changed().  These two calls can be used
 * interchangeably if exactly one item has changed (although in that
 * case g_settings_backend_changed() is definitely preferred).
 *
 * For efficiency reasons, the implementation should strive for @path to
 * be as long as possible (ie: the longest common prefix of all of the
 * keys that were changed) but this is not strictly required.
 *
 * Since: 2.26
 */
void
g_settings_backend_keys_changed (GSettingsBackend    *backend,
                                 const gchar         *path,
                                 gchar const * const *items,
                                 gpointer             origin_tag)
{
  GSettingsBackendWatch *watch;

  g_return_if_fail (G_IS_SETTINGS_BACKEND (backend));
  g_return_if_fail (path[0] == '\0' || is_path (path));
  g_return_if_fail (items != NULL);

  for (watch = backend->priv->watches; watch; watch = watch->next)
    watch->keys_changed (backend, path, items, origin_tag, watch->user_data);
}

/**
 * g_settings_backend_keys_changed:
 * @backend: a #GSettingsBackend implementation
 * @path: the path containing the changes
 * @origin_tag: the origin tag
 *
 * Signals that all keys below a given path may have possibly changed.
 * Backend implementations should call this if an entire path of keys
 * have possibly changed their values.
 *
 * @path must be a valid path (ie: starting and ending with a slash and
 * not containing '//').
 *
 * The meaning of this signal is that any of the key which has a name
 * starting with @path may have changed.
 *
 * The same rules for when notifications must occur apply as per
 * g_settings_backend_changed().  This call might be an appropriate
 * reasponse to a 'reset' call but implementations are also free to
 * explicitly list the keys that were affected by that call if they can
 * easily do so.
 *
 * For efficiency reasons, the implementation should strive for @path to
 * be as long as possible (ie: the longest common prefix of all of the
 * keys that were changed) but this is not strictly required.  As an
 * example, if this function is called with the path of "/" then every
 * single key in the application will be notified of a possible change.
 *
 * Since: 2.26
 */
void
g_settings_backend_path_changed (GSettingsBackend *backend,
                                 const gchar      *path,
                                 gpointer          origin_tag)
{
  GSettingsBackendWatch *watch;

  g_return_if_fail (G_IS_SETTINGS_BACKEND (backend));
  g_return_if_fail (is_path (path));

  for (watch = backend->priv->watches; watch; watch = watch->next)
    watch->path_changed (backend, path, origin_tag, watch->user_data);
}

/**
 * g_settings_backend_writable_changed:
 * @backend: a #GSettingsBackend implementation
 * @key: the name of the key
 *
 * Signals that the writability of a single key has possibly changed.
 *
 * Since GSettings performs no locking operations for itself, this call
 * will always be made in response to external events.
 *
 * Since: 2.26
 **/
void
g_settings_backend_writable_changed (GSettingsBackend *backend,
                                     const gchar      *key)
{
  GSettingsBackendWatch *watch;

  g_return_if_fail (G_IS_SETTINGS_BACKEND (backend));
  g_return_if_fail (is_key (key));

  for (watch = backend->priv->watches; watch; watch = watch->next)
    watch->writable_changed (backend, key, watch->user_data);
}

/**
 * g_settings_backend_writable_changed:
 * @backend: a #GSettingsBackend implementation
 * @path: the name of the path
 *
 * Signals that the writability of all keys below a given path may have
 * changed.
 *
 * Since GSettings performs no locking operations for itself, this call
 * will always be made in response to external events.
 *
 * Since: 2.26
 **/
void
g_settings_backend_path_writable_changed (GSettingsBackend *backend,
                                          const gchar      *path)
{
  GSettingsBackendWatch *watch;

  g_return_if_fail (G_IS_SETTINGS_BACKEND (backend));
  g_return_if_fail (is_path (path));

  for (watch = backend->priv->watches; watch; watch = watch->next)
    watch->path_writable_changed (backend, path, watch->user_data);
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
 * @tree: a #GTree containing the changes
 * @origin_tag: the origin tag
 *
 * This call is a convenience wrapper.  It gets the list of changes from
 * @tree, computes the longest common prefix and calls
 * g_settings_backend_changed().
 *
 * Since: 2.26
 **/
void
g_settings_backend_changed_tree (GSettingsBackend *backend,
                                 GTree            *tree,
                                 gpointer          origin_tag)
{
  GSettingsBackendWatch *watch;
  gchar **list;

  list = g_new (gchar *, g_tree_nnodes (tree) + 1);

  {
    gchar **ptr = list;
    g_tree_foreach (tree, g_settings_backend_append_to_list, &ptr);
    *ptr = NULL;

    g_assert (list + g_tree_nnodes (tree) == ptr);
  }

  for (watch = backend->priv->watches; watch; watch = watch->next)
    watch->keys_changed (backend, "/",
                         (const gchar * const *) list,
                         origin_tag, watch->user_data);
  g_free (list);
}

/*< private >
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
 */
GVariant *
g_settings_backend_read (GSettingsBackend   *backend,
                         const gchar        *key,
                         const GVariantType *expected_type)
{
  return G_SETTINGS_BACKEND_GET_CLASS (backend)
    ->read (backend, key, expected_type);
}

/*< private >
 * g_settings_backend_write:
 * @backend: a #GSettingsBackend implementation
 * @key: the name of the key
 * @value: a #GVariant value to write to this key
 * @origin_tag: the origin tag
 *
 * Writes exactly one key.
 *
 * This call does not fail.  During this call a
 * #GSettingsBackend::changed signal will be emitted if the value of the
 * key has changed.  The updated key value will be visible to any signal
 * callbacks.
 *
 * One possible method that an implementation might deal with failures is
 * to emit a second "changed" signal (either during this call, or later)
 * to indicate that the affected keys have suddenly "changed back" to their
 * old values.
 *
 * Since: 2.26
 */
void
g_settings_backend_write (GSettingsBackend *backend,
                          const gchar      *key,
                          GVariant         *value,
                          gpointer          origin_tag)
{
  G_SETTINGS_BACKEND_GET_CLASS (backend)
    ->write (backend, key, value, origin_tag);
}

/*< private >
 * g_settings_backend_write_keys:
 * @backend: a #GSettingsBackend implementation
 * @values: a #GTree containing key-value pairs to write
 * @origin_tag: the origin tag
 *
 * Writes one or more keys.  This call will never block.
 *
 * The key of each item in the tree is the key name to write to and the
 * value is a #GVariant to write.  The proper type of #GTree for this
 * call can be created with g_settings_backend_create_tree().  This call
 * might take a reference to the tree; you must not modified the #GTree
 * after passing it to this call.
 *
 * This call does not fail.  During this call a #GSettingsBackend::changed
 * signal will be emitted if any keys have been changed.  The new values of
 * all updated keys will be visible to any signal callbacks.
 *
 * One possible method that an implementation might deal with failures is
 * to emit a second "changed" signal (either during this call, or later)
 * to indicate that the affected keys have suddenly "changed back" to their
 * old values.
 */
void
g_settings_backend_write_keys (GSettingsBackend *backend,
                               GTree            *tree,
                               gpointer          origin_tag)
{
  G_SETTINGS_BACKEND_GET_CLASS (backend)
    ->write_keys (backend, tree, origin_tag);
}

/*< private >
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
 */
gboolean
g_settings_backend_get_writable (GSettingsBackend *backend,
                                 const gchar      *name)
{
  return G_SETTINGS_BACKEND_GET_CLASS (backend)
    ->get_writable (backend, name);
}

/*< private >
 * g_settings_backend_unsubscribe:
 * @backend: a #GSettingsBackend
 * @name: a key or path to subscribe to
 *
 * Reverses the effect of a previous call to
 * g_settings_backend_subscribe().
 */
void
g_settings_backend_unsubscribe (GSettingsBackend *backend,
                                const char       *name)
{
  G_SETTINGS_BACKEND_GET_CLASS (backend)
    ->unsubscribe (backend, name);
}

/*< private >
 * g_settings_backend_subscribe:
 * @backend: a #GSettingsBackend
 * @name: a key or path to subscribe to
 *
 * Requests that change signals be emitted for events on @name.
 */
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
g_settings_backend_init (GSettingsBackend *backend)
{
  backend->priv = G_TYPE_INSTANCE_GET_PRIVATE (backend,
                                               G_TYPE_SETTINGS_BACKEND,
                                               GSettingsBackendPrivate);
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
    g_param_spec_string ("context", P_("Context"),
                         P_("An identifier to decide which storage to use"),
                         NULL, G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

}

/*< private >
 * g_settings_backend_create_tree:
 * @returns: a new #GTree
 *
 * This is a convenience function for creating a tree that is compatible
 * with g_settings_backend_write().  It merely calls g_tree_new_full()
 * with strcmp(), g_free() and g_variant_unref().
 */
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
      GSettingsBackendClass *backend_class;
      GTypeClass *class;

      class = g_io_extension_ref_class (extension);
      backend_class = G_SETTINGS_BACKEND_CLASS (class);

      if (backend_class->supports_context != NULL &&
          !backend_class->supports_context (context))
        {
          g_type_class_unref (class);
          return NULL;
        }

      g_type_class_unref (class);
    }

  type = g_io_extension_get_type (extension);

  return g_object_new (type, "context", context, NULL);
}

/*< private >
 * g_settings_backend_get_with_context:
 * @context: a context that might be used by the backend to determine
 *     which storage to use, or %NULL to use the default storage
 * @returns: the default #GSettingsBackend
 *
 * Returns the default #GSettingsBackend. It is possible to override
 * the default by setting the <envar>GSETTINGS_BACKEND</envar>
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
 */
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

  if (!backend)
    {
      backend = get_default_backend (context);

      if (!backend)
        {
          /* FIXME: create an instance of the memory backend */
        }

      g_hash_table_insert (backends, g_strdup (context), backend);
    }

  return g_object_ref (backend);
}

/*< private >
 * g_settings_backend_supports_context:
 * @context: a context string that might be passed to
 *     g_settings_backend_new_with_context()
 * @returns: #TRUE if @context is supported
 *
 * Determines if the given context is supported by the default
 * GSettingsBackend implementation.
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
