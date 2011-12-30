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

#include "gsettingsbackendinternal.h"
#include "gsimplepermission.h"
#include "giomodule-priv.h"

#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <glibintl.h>


G_DEFINE_ABSTRACT_TYPE (GSettingsBackend, g_settings_backend, G_TYPE_OBJECT)

struct _GSettingsBackendPrivate
{
  gboolean has_unapplied;
};

enum
{
  PROP_0,
  PROP_HAS_UNAPPLIED,
  N_PROPS
};

static GParamSpec *g_settings_backend_pspecs[N_PROPS];

enum
{
  SIGNAL_EVENT,
  N_SIGNALS
};

static guint g_settings_backend_signals[N_SIGNALS];


/* For g_settings_backend_sync_default(), we only want to actually do
 * the sync if the backend already exists.  This avoids us creating an
 * entire GSettingsBackend in order to call a do-nothing sync()
 * operation on it.  This variable lets us avoid that.
 */
static gboolean g_settings_has_backend;

/**
 * SECTION:gsettingsbackend
 * @title: GSettingsBackend
 * @short_description: Interface for settings backend implementations
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

void
g_settings_backend_event (GSettingsBackend     *backend,
                          const GSettingsEvent *event)
{
  g_signal_emit (backend, g_settings_backend_signals[SIGNAL_EVENT], 0, event);
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
 * @key must be a valid key (ie starting with a slash, not containing
 * '//', and not ending with a slash).
 *
 * The implementation must call this function during any call to
 * g_settings_backend_write(), before the call returns (except in the
 * case that no keys are actually changed and it cares to detect this
 * fact).  It may not rely on the existence of a mainloop for
 * dispatching the signal later.
 *
 * The implementation may call this function at any other time it likes
 * in response to other events (such as changes occurring outside of the
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
  GSettingsEvent event;
  gchar *null = NULL;

  g_return_if_fail (G_IS_SETTINGS_BACKEND (backend));
  g_return_if_fail (is_key (key));

  event.type = G_SETTINGS_EVENT_CHANGE;
  event.prefix = (gchar *) key;
  event.keys = &null;
  event.origin_tag = origin_tag;

  g_settings_backend_event (backend, &event);
}

/**
 * g_settings_backend_keys_changed:
 * @backend: a #GSettingsBackend implementation
 * @path: the path containing the changes
 * @items: (array zero-terminated=1): the %NULL-terminated list of changed keys
 * @origin_tag: the origin tag
 *
 * Signals that a list of keys have possibly changed.  Backend
 * implementations should call this if keys have possibly changed their
 * values.
 *
 * @path must be a valid path (ie starting and ending with a slash and
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
  GSettingsEvent event;

  g_return_if_fail (G_IS_SETTINGS_BACKEND (backend));
  g_return_if_fail (is_path (path));

  /* XXX: should do stricter checking (ie: inspect each item) */
  g_return_if_fail (items != NULL);

  event.type = G_SETTINGS_EVENT_CHANGE;
  event.prefix = (gchar *) path;
  event.keys = (gchar **) items;
  event.origin_tag = origin_tag;

  g_settings_backend_event (backend, &event);
}

/**
 * g_settings_backend_path_changed:
 * @backend: a #GSettingsBackend implementation
 * @path: the path containing the changes
 * @origin_tag: the origin tag
 *
 * Signals that all keys below a given path may have possibly changed.
 * Backend implementations should call this if an entire path of keys
 * have possibly changed their values.
 *
 * @path must be a valid path (ie starting and ending with a slash and
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
  GSettingsEvent event;
  gchar *null = NULL;

  g_return_if_fail (G_IS_SETTINGS_BACKEND (backend));
  g_return_if_fail (is_path (path));

  event.type = G_SETTINGS_EVENT_CHANGE;
  event.prefix = (gchar *) path;
  event.keys = &null;
  event.origin_tag = origin_tag;

  g_settings_backend_event (backend, &event);
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
  GSettingsEvent event;
  gchar *null = NULL;

  g_return_if_fail (G_IS_SETTINGS_BACKEND (backend));
  g_return_if_fail (is_key (key));

  event.type = G_SETTINGS_EVENT_WRITABLE_CHANGE;
  event.prefix = (gchar *) key;
  event.keys = &null;
  event.origin_tag = NULL;

  g_settings_backend_event (backend, &event);
}

/**
 * g_settings_backend_path_writable_changed:
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
  GSettingsEvent event;
  gchar *null = NULL;

  g_return_if_fail (G_IS_SETTINGS_BACKEND (backend));
  g_return_if_fail (is_path (path));

  event.type = G_SETTINGS_EVENT_WRITABLE_CHANGE;
  event.prefix = (gchar *) path;
  event.keys = &null;
  event.origin_tag = NULL;

  g_settings_backend_event (backend, &event);
}

typedef struct
{
  const gchar **keys;
  GVariant **values;
  gint prefix_len;
  gchar *prefix;
} FlattenState;

static gboolean
g_settings_backend_flatten_one (gpointer key,
                                gpointer value,
                                gpointer user_data)
{
  FlattenState *state = user_data;
  const gchar *skey = key;
  gint i;

  g_return_val_if_fail (is_key (key), TRUE);

  /* calculate longest common prefix */
  if (state->prefix == NULL)
    {
      gchar *last_byte;

      /* first key?  just take the prefix up to the last '/' */
      state->prefix = g_strdup (skey);
      last_byte = strrchr (state->prefix, '/') + 1;
      state->prefix_len = last_byte - state->prefix;
      *last_byte = '\0';
    }
  else
    {
      /* find the first character that does not match.  we will
       * definitely find one because the prefix ends in '/' and the key
       * does not.  also: no two keys in the tree are the same.
       */
      for (i = 0; state->prefix[i] == skey[i]; i++);

      /* check if we need to shorten the prefix */
      if (state->prefix[i] != '\0')
        {
          /* find the nearest '/', terminate after it */
          while (state->prefix[i - 1] != '/')
            i--;

          state->prefix[i] = '\0';
          state->prefix_len = i;
        }
    }


  /* save the entire item into the array.
   * the prefixes will be removed later.
   */
  *state->keys++ = key;

  if (state->values)
    *state->values++ = value;

  return FALSE;
}

/**
 * g_settings_backend_flatten_tree:
 * @tree: a #GTree containing the changes
 * @path: (out): the location to save the path
 * @keys: (out) (transfer container) (array zero-terminated=1): the
 *        location to save the relative keys
 * @values: (out) (allow-none) (transfer container) (array zero-terminated=1):
 *          the location to save the values, or %NULL
 *
 * Calculate the longest common prefix of all keys in a tree and write
 * out an array of the key names relative to that prefix and,
 * optionally, the value to store at each of those keys.
 *
 * You must free the value returned in @path, @keys and @values using
 * g_free().  You should not attempt to free or unref the contents of
 * @keys or @values.
 *
 * Since: 2.26
 **/
void
g_settings_backend_flatten_tree (GTree         *tree,
                                 gchar        **path,
                                 const gchar ***keys,
                                 GVariant    ***values)
{
  FlattenState state = { 0, };
  gsize nnodes;

  nnodes = g_tree_nnodes (tree);

  *keys = state.keys = g_new (const gchar *, nnodes + 1);
  state.keys[nnodes] = NULL;

  if (values != NULL)
    {
      *values = state.values = g_new (GVariant *, nnodes + 1);
      state.values[nnodes] = NULL;
    }

  g_tree_foreach (tree, g_settings_backend_flatten_one, &state);
  g_return_if_fail (*keys + nnodes == state.keys);

  *path = state.prefix;
  while (nnodes--)
    *--state.keys += state.prefix_len;
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
  const gchar **keys;
  gchar *path;

  g_return_if_fail (G_IS_SETTINGS_BACKEND (backend));

  g_settings_backend_flatten_tree (tree, &path, &keys, NULL);

#ifdef DEBUG_CHANGES
  {
    gint i;

    g_print ("----\n");
    g_print ("changed_tree(): prefix %s\n", path);
    for (i = 0; keys[i]; i++)
      g_print ("  %s\n", keys[i]);
    g_print ("----\n");
  }
#endif

  g_settings_backend_keys_changed (backend, path, keys, origin_tag);
  g_free (path);
  g_free (keys);
}

/*< private >
 * g_settings_backend_read:
 * @backend: a #GSettingsBackend implementation
 * @key: the key to read
 * @expected_type: a #GVariantType
 * @default_value: if the default value should be returned
 *
 * Reads a key. This call will never block.
 *
 * If the key exists, the value associated with it will be returned.
 * If the key does not exist, %NULL will be returned.
 *
 * The returned value will be of the type given in @expected_type.  If
 * the backend stored a value of a different type then %NULL will be
 * returned.
 *
 * If @default_value is %TRUE then this gets the default value from the
 * backend (ie: the one that the backend would contain if
 * g_settings_reset() were called).
 *
 * Returns: the value that was read, or %NULL
 */
GVariant *
g_settings_backend_read (GSettingsBackend   *backend,
                         const gchar        *key,
                         const GVariantType *expected_type,
                         gboolean            default_value)
{
  GVariant *value;

  value = G_SETTINGS_BACKEND_GET_CLASS (backend)
    ->read (backend, key, expected_type, default_value);

  if (value != NULL)
    value = g_variant_take_ref (value);

  if G_UNLIKELY (value && !g_variant_is_of_type (value, expected_type))
    {
      g_variant_unref (value);
      value = NULL;
    }

  return value;
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
 * Returns: %TRUE if the write succeeded, %FALSE if the key was not writable
 */
gboolean
g_settings_backend_write (GSettingsBackend *backend,
                          const gchar      *key,
                          GVariant         *value,
                          gpointer          origin_tag)
{
  gboolean success;

  g_variant_ref_sink (value);
  success = G_SETTINGS_BACKEND_GET_CLASS (backend)
    ->write (backend, key, value, origin_tag);
  g_variant_unref (value);

  return success;
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
gboolean
g_settings_backend_write_tree (GSettingsBackend *backend,
                               GTree            *tree,
                               gpointer          origin_tag)
{
  return G_SETTINGS_BACKEND_GET_CLASS (backend)
    ->write_tree (backend, tree, origin_tag);
}

/*< private >
 * g_settings_backend_reset:
 * @backend: a #GSettingsBackend implementation
 * @key: the name of a key
 * @origin_tag: the origin tag
 *
 * "Resets" the named key to its "default" value (ie: after system-wide
 * defaults, mandatory keys, etc. have been taken into account) or possibly
 * unsets it.
 */
void
g_settings_backend_reset (GSettingsBackend *backend,
                          const gchar      *key,
                          gpointer          origin_tag)
{
  G_SETTINGS_BACKEND_GET_CLASS (backend)
    ->reset (backend, key, origin_tag);
}

/*< private >
 * g_settings_backend_get_writable:
 * @backend: a #GSettingsBackend implementation
 * @key: the name of a key
 *
 * Finds out if a key is available for writing to.  This is the
 * interface through which 'lockdown' is implemented.  Locked down
 * keys will have %FALSE returned by this call.
 *
 * You should not write to locked-down keys, but if you do, the
 * implementation will deal with it.
 *
 * Returns: %TRUE if the key is writable
 */
gboolean
g_settings_backend_get_writable (GSettingsBackend *backend,
                                 const gchar      *key)
{
  return G_SETTINGS_BACKEND_GET_CLASS (backend)
    ->get_writable (backend, key);
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
g_settings_backend_get_property (GObject *object, guint prop_id,
                                 GValue *value, GParamSpec *pspec)
{
  g_assert (prop_id == PROP_HAS_UNAPPLIED);
  g_value_set_boolean (value, g_settings_backend_get_has_unapplied (G_SETTINGS_BACKEND (object)));
}

static void
ignore_subscription (GSettingsBackend *backend,
                     const gchar      *key)
{
}

static GSettingsBackend *
g_settings_backend_real_delay (GSettingsBackend *backend)
{
  return g_null_settings_backend_new ();
}

static void
ignore_apply (GSettingsBackend *backend)
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
  class->delay = g_settings_backend_real_delay;
  class->apply = ignore_apply;
  class->revert = ignore_apply;

  gobject_class->get_property = g_settings_backend_get_property;

  g_settings_backend_signals[SIGNAL_EVENT] =
    g_signal_new ("event", G_TYPE_SETTINGS_BACKEND, G_SIGNAL_RUN_FIRST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);

  g_settings_backend_pspecs[PROP_HAS_UNAPPLIED] =
    g_param_spec_boolean ("has-unapplied", "has unapplied", "TRUE if apply() is meaningful",
                          FALSE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (gobject_class, N_PROPS, g_settings_backend_pspecs);

  g_type_class_add_private (class, sizeof (GSettingsBackendPrivate));
}

static void
g_settings_backend_variant_unref0 (gpointer data)
{
  if (data != NULL)
    g_variant_unref (data);
}

/*< private >
 * g_settings_backend_create_tree:
 *
 * This is a convenience function for creating a tree that is compatible
 * with g_settings_backend_write().  It merely calls g_tree_new_full()
 * with strcmp(), g_free() and g_variant_unref().
 *
 * Returns: a new #GTree
 */
GTree *
g_settings_backend_create_tree (void)
{
  return g_tree_new_full ((GCompareDataFunc) strcmp, NULL,
                          g_free, g_settings_backend_variant_unref0);
}

static gboolean
g_settings_backend_verify (gpointer impl)
{
  GSettingsBackend *backend = impl;

  if (strcmp (G_OBJECT_TYPE_NAME (backend), "GMemorySettingsBackend") == 0 &&
      g_strcmp0 (g_getenv ("GSETTINGS_BACKEND"), "memory") != 0)
    {
      g_message ("Using the 'memory' GSettings backend.  Your settings "
		 "will not be saved or shared with other applications.");
    }

  g_settings_has_backend = TRUE;
  return TRUE;
}

/**
 * g_settings_backend_get_default:
 *
 * Returns the default #GSettingsBackend. It is possible to override
 * the default by setting the <envar>GSETTINGS_BACKEND</envar>
 * environment variable to the name of a settings backend.
 *
 * The user gets a reference to the backend.
 *
 * Returns: (transfer full): the default #GSettingsBackend
 *
 * Since: 2.28
 */
GSettingsBackend *
g_settings_backend_get_default (void)
{
  GSettingsBackend *backend;

  backend = _g_io_module_get_default (G_SETTINGS_BACKEND_EXTENSION_POINT_NAME,
				      "GSETTINGS_BACKEND",
				      g_settings_backend_verify);
  return g_object_ref (backend);
}

/*< private >
 * g_settings_backend_get_permission:
 * @backend: a #GSettingsBackend
 * @path: a path
 *
 * Gets the permission object associated with writing to keys below
 * @path on @backend.
 *
 * If this is not implemented in the backend, then a %TRUE
 * #GSimplePermission is returned.
 *
 * Returns: a non-%NULL #GPermission. Free with g_object_unref()
 */
GPermission *
g_settings_backend_get_permission (GSettingsBackend *backend,
                                   const gchar      *path)
{
  GSettingsBackendClass *class = G_SETTINGS_BACKEND_GET_CLASS (backend);

  if (class->get_permission)
    return class->get_permission (backend, path);

  return g_simple_permission_new (TRUE);
}

/*< private >
 * g_settings_backend_sync_default:
 *
 * Syncs the default backend.
 */
void
g_settings_backend_sync_default (void)
{
  if (g_settings_has_backend)
    {
      GSettingsBackendClass *class;
      GSettingsBackend *backend;

      backend = g_settings_backend_get_default ();
      class = G_SETTINGS_BACKEND_GET_CLASS (backend);

      if (class->sync)
        class->sync (backend);
    }
}

GSettingsBackend *
g_settings_backend_delay (GSettingsBackend *backend)
{
  return G_SETTINGS_BACKEND_GET_CLASS (backend)
    ->delay (backend);
}

gboolean
g_settings_backend_get_has_unapplied (GSettingsBackend *backend)
{
  return backend->priv->has_unapplied;
}

void
g_settings_backend_set_has_unapplied (GSettingsBackend *backend,
                                      gboolean          has_unapplied)
{
  if (has_unapplied != backend->priv->has_unapplied)
    {
      backend->priv->has_unapplied = has_unapplied;

      g_object_notify_by_pspec (G_OBJECT (backend), g_settings_backend_pspecs[PROP_HAS_UNAPPLIED]);
    }
}

void
g_settings_backend_apply (GSettingsBackend *backend)
{
  G_SETTINGS_BACKEND_GET_CLASS (backend)
    ->apply (backend);
}

void
g_settings_backend_revert (GSettingsBackend *backend)
{
  G_SETTINGS_BACKEND_GET_CLASS (backend)
    ->revert (backend);
}
