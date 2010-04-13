/*
 * Copyright © 2009 Codethink Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of version 3 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * See the included COPYING file for more information.
 */

#include "gsettingsbackend.h"

#include "giomodule-priv.h"
#include "gio-marshal.h"

#include <string.h>
#include <stdlib.h>
#include <glib.h>

#include "gioalias.h"

G_DEFINE_ABSTRACT_TYPE (GSettingsBackend, g_settings_backend, G_TYPE_OBJECT)

static guint changed_signal;

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
 * @items: the list of changed keys
 * @n_items: the number of items in @items. May be -1 if @items is
 *     %NULL-terminated
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
g_settings_backend_class_init (GSettingsBackendClass *class)
{
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
get_default_backend (gpointer user_data)
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

  type = g_io_extension_get_type (extension);

  return g_object_new (type, NULL);
}

/**
 * g_settings_backend_get_default:
 * @returns: the default #GSettingsBackend
 *
 * Returns the default #GSettingsBackend. It is possible to override
 * the default by setting the <envvar>GSETTINGS_BACKEND</envvar>
 * environment variable to the name of a settings backend.
 *
 * The user does not own the return value and it must not be freed.
 *
 * Since: 2.26
 **/
GSettingsBackend *
g_settings_backend_get_default (void)
{
  static GOnce once = G_ONCE_INIT;

  return g_once (&once, get_default_backend, NULL);
}

static void
g_settings_backend_init (GSettingsBackend *backend)
{
}
