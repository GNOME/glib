/*
 * Copyright © 2010 Codethink Limited
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Allison Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gsettingsbackend.h"

#include <string.h>
#include <stdlib.h>

/**
 * SECTION:changeset
 * @title: GSettingsBackendChangeset
 * @Short_description: A set of changes to a g_settings_backend database
 *
 * #GSettingsBackendChangeset represents a set of changes that can be made to a
 * g_settings_backend database.  Currently supported operations are writing new
 * values to keys and resetting keys and dirs.
 *
 * Create the changeset with g_settings_backend_changeset_new() and populate it with
 * g_settings_backend_changeset_set().  Submit it to g_settings_backend with
 * g_settings_backend_client_change_fast() or g_settings_backend_client_change_sync().
 * g_settings_backend_changeset_new_write() is a convenience constructor for the
 * common case of writing or resetting a single value.
 **/

/**
 * GSettingsBackendChangeset:
 *
 * This is a reference counted opaque structure type.  It is not a
 * #GObject.
 *
 * Use g_settings_backend_changeset_ref() and g_settings_backend_changeset_unref() to manipulate
 * references.
 **/

struct _GSettingsBackendChangeset
{
  GHashTable *table;
  GHashTable *dir_resets;
  guint is_database : 1;
  guint is_sealed : 1;
  gint ref_count;

  gchar *prefix;
  const gchar **paths;
  GVariant **values;
};

static void
unref_gvariant0 (gpointer data)
{
  if (data)
    g_variant_unref (data);
}

/**
 * g_settings_backend_changeset_new:
 *
 * Creates a new, empty, #GSettingsBackendChangeset.
 *
 * Returns: the new #GSettingsBackendChangeset.
 **/
GSettingsBackendChangeset *
g_settings_backend_changeset_new (void)
{
  GSettingsBackendChangeset *changeset;

  changeset = g_slice_new0 (GSettingsBackendChangeset);
  changeset->table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, unref_gvariant0);
  changeset->ref_count = 1;

  return changeset;
}

/**
 * g_settings_backend_changeset_new_database:
 * @copy_of: (allow-none): a #GSettingsBackendChangeset to copy
 *
 * Creates a new #GSettingsBackendChangeset in "database" mode, possibly
 * initialising it with the values of another changeset.
 *
 * In a certain sense it's possible to imagine that a #GSettingsBackendChangeset
 * could express the contents of an entire g_settings_backend database -- the
 * contents are the database are what you would have if you applied the
 * changeset to an empty database.  One thing that fails to map in this
 * analogy are reset operations -- if we start with an empty database
 * then reset operations are meaningless.
 *
 * A "database" mode changeset is therefore a changeset which is
 * incapable of containing reset operations.
 *
 * It is not permitted to use a database-mode changeset for most
 * operations (such as the @change argument to g_settings_backend_changeset_change()
 * or the @changeset argument to #GSettingsBackendClient APIs).
 *
 * If @copy_of is non-%NULL then its contents will be copied into the
 * created changeset.  @copy_of must be a database-mode changeset.
 *
 * Returns: a new #GSettingsBackendChangeset in "database" mode
 *
 * Since: 0.16
 */
GSettingsBackendChangeset *
g_settings_backend_changeset_new_database (GSettingsBackendChangeset *copy_of)
{
  GSettingsBackendChangeset *changeset;

  g_return_val_if_fail (copy_of == NULL || copy_of->is_database, NULL);

  changeset = g_settings_backend_changeset_new ();
  changeset->is_database = TRUE;

  if (copy_of)
    {
      GHashTableIter iter;
      gpointer key, value;

      g_hash_table_iter_init (&iter, copy_of->table);
      while (g_hash_table_iter_next (&iter, &key, &value))
        g_hash_table_insert (changeset->table, g_strdup (key), g_variant_ref (value));
    }

  return changeset;
}

/**
 * g_settings_backend_changeset_unref:
 * @changeset: a #GSettingsBackendChangeset
 *
 * Releases a #GSettingsBackendChangeset reference.
 **/
void
g_settings_backend_changeset_unref (GSettingsBackendChangeset *changeset)
{
  if (g_atomic_int_dec_and_test (&changeset->ref_count))
    {
      g_free (changeset->prefix);
      g_free (changeset->paths);
      g_free (changeset->values);

      g_hash_table_unref (changeset->table);

      if (changeset->dir_resets)
        g_hash_table_unref (changeset->dir_resets);

      g_slice_free (GSettingsBackendChangeset, changeset);
    }
}

/**
 * g_settings_backend_changeset_ref:
 * @changeset: a #GSettingsBackendChangeset
 *
 * Increases the reference count on @changeset
 *
 * Returns: @changeset
 **/
GSettingsBackendChangeset *
g_settings_backend_changeset_ref (GSettingsBackendChangeset *changeset)
{
  g_atomic_int_inc (&changeset->ref_count);

  return changeset;
}

static void
g_settings_backend_changeset_record_dir_reset (GSettingsBackendChangeset *changeset,
                                               const gchar               *dir)
{
  g_return_if_fail (g_settings_backend_is_dir (dir));
  g_return_if_fail (!changeset->is_database);
  g_return_if_fail (!changeset->is_sealed);

  if (!changeset->dir_resets)
    changeset->dir_resets = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  g_hash_table_insert (changeset->table, g_strdup (dir), NULL);
  g_hash_table_add (changeset->dir_resets, g_strdup (dir));
}

/**
 * g_settings_backend_changeset_set:
 * @changeset: a #GSettingsBackendChangeset
 * @path: a path to modify
 * @value: the value for the key, or %NULL to reset
 *
 * Adds an operation to modify @path to a #GSettingsBackendChangeset.
 *
 * @path may either be a key or a dir.  If it is a key then @value may
 * be a #GVariant, or %NULL (to set or reset the key).
 *
 * If @path is a dir then this must be a reset operation: @value must be
 * %NULL.  It is not permitted to assign a #GVariant value to a dir.
 **/
void
g_settings_backend_changeset_set (GSettingsBackendChangeset *changeset,
                                  const gchar               *path,
                                  GVariant                  *value)
{
  g_return_if_fail (!changeset->is_sealed);
  g_return_if_fail (g_settings_backend_is_path (path));

  /* Check if we are performing a path reset */
  if (g_str_has_suffix (path, "/"))
    {
      GHashTableIter iter;
      gpointer key;

      g_return_if_fail (value == NULL);

      /* When we reset a path we must also reset all keys within that
       * path.
       */
      g_hash_table_iter_init (&iter, changeset->table);
      while (g_hash_table_iter_next (&iter, &key, NULL))
        if (g_str_has_prefix (key, path))
          g_hash_table_iter_remove (&iter);

      /* If this is a non-database then record the reset itself. */
      if (!changeset->is_database)
        g_settings_backend_changeset_record_dir_reset (changeset, path);
    }

  /* ...or a value reset */
  else if (value == NULL)
    {
      /* If we're a non-database, record the reset explicitly.
       * Otherwise, just reset whatever may be there already.
       */
      if (!changeset->is_database)
        g_hash_table_insert (changeset->table, g_strdup (path), NULL);
      else
        g_hash_table_remove (changeset->table, path);
    }

  /* ...or a normal write. */
  else
    g_hash_table_insert (changeset->table, g_strdup (path), g_variant_ref_sink (value));
}

/**
 * g_settings_backend_changeset_get:
 * @changeset: a #GSettingsBackendChangeset
 * @key: the key to check
 * @value: a return location for the value, or %NULL
 *
 * Checks if a #GSettingsBackendChangeset has an outstanding request to change
 * the value of the given @key.
 *
 * If the change doesn't involve @key then %FALSE is returned and the
 * @value is unmodified.
 *
 * If the change modifies @key then @value is set either to the value
 * for that key, or %NULL in the case that the key is being reset by the
 * request.
 *
 * Returns: %TRUE if the key is being modified by the change
 */
gboolean
g_settings_backend_changeset_get (GSettingsBackendChangeset  *changeset,
                                  const gchar                *key,
                                  GVariant                  **value)
{
  gpointer tmp;

  if (!g_hash_table_lookup_extended (changeset->table, key, NULL, &tmp))
    {
      /* Did not find an exact match, so check for dir resets */
      if (changeset->dir_resets)
        {
          GHashTableIter iter;
          gpointer dir;

          g_hash_table_iter_init (&iter, changeset->dir_resets);
          while (g_hash_table_iter_next (&iter, &dir, NULL))
            if (g_str_has_prefix (key, dir))
              {
                if (value)
                  *value = NULL;

                return TRUE;
              }
        }

      return FALSE;
    }

  if (value)
    *value = tmp ? g_variant_ref (tmp) : NULL;

  return TRUE;
}

/**
 * g_settings_backend_changeset_is_similar_to:
 * @changeset: a #GSettingsBackendChangeset
 * @other: another #GSettingsBackendChangeset
 *
 * Checks if @changeset is similar to @other.
 *
 * Two changes are considered similar if they write to the exact same
 * set of keys.  The values written are not considered.
 *
 * This check is used to prevent building up a queue of repeated writes
 * of the same keys.  This is often seen when an application writes to a
 * key on every move of a slider or an application window.
 *
 * Strictly speaking, a write resettings all of "/a/" after a write
 * containing "/a/b" could cause the later to be removed from the queue,
 * but this situation is difficult to detect and is expected to be
 * extremely rare.
 *
 * Returns: %TRUE if the changes are similar
 **/
gboolean
g_settings_backend_changeset_is_similar_to (GSettingsBackendChangeset *changeset,
                                            GSettingsBackendChangeset *other)
{
  GHashTableIter iter;
  gpointer key;

  if (g_hash_table_size (changeset->table) != g_hash_table_size (other->table))
    return FALSE;

  g_hash_table_iter_init (&iter, changeset->table);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    if (!g_hash_table_contains (other->table, key))
      return FALSE;

  return TRUE;
}

/**
 * GSettingsBackendChangesetPredicate:
 * @path: a path, as per g_settings_backend_is_path()
 * @value: a #GVariant, or %NULL
 * @user_data: user data pointer
 *
 * Callback function type for predicates over items in a
 * #GSettingsBackendChangeset.
 *
 * Use with g_settings_backend_changeset_all().
 *
 * Returns: %TRUE if the predicate is met for the given @path and @value
 **/

/**
 * g_settings_backend_changeset_all:
 * @changeset: a #GSettingsBackendChangeset
 * @predicate: a #GSettingsBackendChangesetPredicate
 * @user_data: user data to pass to @predicate
 *
 * Checks if all changes in the changeset satisfy @predicate.
 *
 * @predicate is called on each item in the changeset, in turn, until it
 * returns %FALSE.
 *
 * If @predicate returns %FALSE for any item, this function returns
 * %FALSE.  If not (including the case of no items) then this function
 * returns %TRUE.
 *
 * Returns: %TRUE if all items in @changeset satisfy @predicate
 */
gboolean
g_settings_backend_changeset_all (GSettingsBackendChangeset          *changeset,
                                  GSettingsBackendChangesetPredicate  predicate,
                                  gpointer                            user_data)
{
  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init (&iter, changeset->table);
  while (g_hash_table_iter_next (&iter, &key, &value))
    if (!(* predicate) (key, value, user_data))
      return FALSE;

  return TRUE;
}

static gint
g_settings_backend_changeset_string_ptr_compare (gconstpointer a_p,
                                                 gconstpointer b_p)
{
  const gchar * const *a = a_p;
  const gchar * const *b = b_p;

  return strcmp (*a, *b);
}

/**
 * g_settings_backend_changeset_seal:
 * @changeset: a #GSettingsBackendChangeset
 *
 * Seals @changeset.
 *
 * When a #GSettingsBackendChangeset is first created, it is mutable and
 * non-threadsafe.  Once the changeset is populated with the required
 * changes, it can be shared between multiple threads, but only by
 * making it immutable by "sealing" it.
 *
 * After the changeset is sealed, you cannot call g_settings_backend_changeset_set()
 * or any other functions that would modify it.  It is safe, however, to
 * share it between multiple threads.
 *
 * All changesets are unsealed on creation, including those that are
 * made by copying changesets that are sealed.
 * g_settings_backend_changeset_describe() will implicitly seal a changeset.
 *
 * This function is idempotent.
 *
 * Since: 0.18
 **/
void
g_settings_backend_changeset_seal (GSettingsBackendChangeset *changeset)
{
  gsize prefix_length;
  gint n_items;

  if (changeset->is_sealed)
    return;

  changeset->is_sealed = TRUE;

  /* This function used to be called g_settings_backend_changeset_build_description()
   * because that's basically what sealing is...
   */

  n_items = g_hash_table_size (changeset->table);

  /* If there are no items then what is there to describe? */
  if (n_items == 0)
    return;

  /* We do three separate passes.  This might take a bit longer than
   * doing it all at once but it keeps the complexity down.
   *
   * First, we iterate the table in order to determine the common
   * prefix.
   *
   * Next, we iterate the table again to pull the strings out excluding
   * the leading prefix.
   *
   * We sort the list of paths at this point because the writer
   * requires a sorted list in order to ensure that dir resets come
   * before writes to keys in that dir.
   *
   * Finally, we iterate over the sorted list and use the normal
   * hashtable lookup in order to populate the values array in the same
   * order.
   *
   * Doing it this way avoids the complication of trying to sort two
   * arrays (keys and values) at the same time.
   */

  /* Pass 1: determine the common prefix. */
  {
    GHashTableIter iter;
    const gchar *first;
    gboolean have_one;
    gpointer key;

    g_hash_table_iter_init (&iter, changeset->table);

    /* We checked above that we have at least one item. */
    have_one = g_hash_table_iter_next (&iter, &key, NULL);
    g_assert (have_one);

    prefix_length = strlen (key);
    first = key;

    /* Consider the remaining items to find the common prefix */
    while (g_hash_table_iter_next (&iter, &key, NULL))
      {
        const gchar *this = key;
        gint i;

        for (i = 0; i < prefix_length; i++)
          if (first[i] != this[i])
            {
              prefix_length = i;
              break;
            }
      }

    /* We must surely always have a common prefix of '/' */
    g_assert (prefix_length > 0);
    g_assert (first[0] == '/');

    /* We may find that "/a/ab" and "/a/ac" have a common prefix of
     * "/a/a" but really we want to trim that back to "/a/".
     *
     * If there is only one item, leave it alone.
     */
    if (n_items > 1)
      {
        while (first[prefix_length - 1] != '/')
          prefix_length--;
      }

    changeset->prefix = g_strndup (first, prefix_length);
  }

  /* Pass 2: collect the list of keys, dropping the prefix */
  {
    GHashTableIter iter;
    gpointer key;
    gint i = 0;

    changeset->paths = g_new (const gchar *, n_items + 1);

    g_hash_table_iter_init (&iter, changeset->table);
    while (g_hash_table_iter_next (&iter, &key, NULL))
      {
        const gchar *path = key;

        changeset->paths[i++] = path + prefix_length;
      }
    changeset->paths[i] = NULL;
    g_assert (i == n_items);

    /* Sort the list of keys */
    qsort (changeset->paths, n_items, sizeof (const gchar *), g_settings_backend_changeset_string_ptr_compare);
  }

  /* Pass 3: collect the list of values */
  {
    gint i;

    changeset->values = g_new (GVariant *, n_items);

    for (i = 0; i < n_items; i++)
      /* We dropped the prefix when collecting the array.
       * Bring it back temporarily, for the lookup.
       */
      changeset->values[i] = g_hash_table_lookup (changeset->table, changeset->paths[i] - prefix_length);
  }
}

/**
 * g_settings_backend_changeset_describe:
 * @changeset: a #GSettingsBackendChangeset
 * @prefix: the prefix under which changes have been requested
 * @paths: the list of paths changed, relative to @prefix
 * @values: the list of values changed
 *
 * Describes @changeset.
 *
 * @prefix and @paths are presented in the same way as they are for the
 * GSettingsBackend::keys-changed signal.  @values is an array of the
 * same length as @paths.  For each key described by an element in
 * @paths, @values will contain either a #GVariant (the requested new
 * value of that key) or %NULL (to reset a reset).
 *
 * The @paths array is returned in an order such that dir will always
 * come before keys contained within those dirs.
 *
 * If @changeset is not already sealed then this call will implicitly
 * seal it.  See g_settings_backend_changeset_seal().
 *
 * Returns: the number of changes (the length of @changes and @values).
 **/
guint
g_settings_backend_changeset_describe (GSettingsBackendChangeset  *changeset,
                                       const gchar               **prefix,
                                       const gchar * const       **paths,
                                       GVariant * const          **values)
{
  gint n_items;

  n_items = g_hash_table_size (changeset->table);

  g_settings_backend_changeset_seal (changeset);

  if (prefix)
    *prefix = changeset->prefix;

  if (paths)
    *paths = changeset->paths;

  if (values)
    *values = changeset->values;

  return n_items;
}

/**
 * g_settings_backend_changeset_serialise:
 * @changeset: a #GSettingsBackendChangeset
 *
 * Serialises a #GSettingsBackendChangeset.
 *
 * The returned value has no particular format and should only be passed
 * to g_settings_backend_changeset_deserialise().
 *
 * Returns: a floating #GVariant
 **/
GVariant *
g_settings_backend_changeset_serialise (GSettingsBackendChangeset *changeset)
{
  GVariantBuilder builder;
  GHashTableIter iter;
  gpointer key, value;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{smv}"));

  g_hash_table_iter_init (&iter, changeset->table);
  while (g_hash_table_iter_next (&iter, &key, &value))
    g_variant_builder_add (&builder, "{smv}", key, value);

  return g_variant_builder_end (&builder);
}

/**
 * g_settings_backend_changeset_deserialise:
 * @serialised: a #GVariant from g_settings_backend_changeset_serialise()
 *
 * Creates a #GSettingsBackendChangeset according to a serialised description
 * returned from an earlier call to g_settings_backend_changeset_serialise().
 *
 * @serialised has no particular format -- you should only pass a value
 * that resulted from an earlier serialise operation.
 *
 * This call never fails, even if @serialised is not in the correct
 * format.  Improperly-formatted parts are simply ignored.
 *
 * Returns: a new #GSettingsBackendChangeset
 **/
GSettingsBackendChangeset *
g_settings_backend_changeset_deserialise (GVariant *serialised)
{
  GSettingsBackendChangeset *changeset;
  GVariantIter iter;
  const gchar *key;
  GVariant *value;

  changeset = g_settings_backend_changeset_new ();
  g_variant_iter_init (&iter, serialised);
  while (g_variant_iter_loop (&iter, "{&smv}", &key, &value))
    {
      /* If value is NULL: we may be resetting a key or a dir (a path).
       * If value is non-NULL: we must be setting a key.
       *
       * ie: it is not possible to set a value to a directory.
       *
       * If we get an invalid case, just fall through and ignore it.
       */
      if (g_settings_backend_is_key (key))
        g_hash_table_insert (changeset->table, g_strdup (key), value ? g_variant_ref (value) : NULL);

      else if (g_settings_backend_is_dir (key) && value == NULL)
        g_settings_backend_changeset_record_dir_reset (changeset, key);
    }

  return changeset;
}

/**
 * g_settings_backend_changeset_new_write:
 * @path: a g_settings_backend path
 * @value: a #GVariant, or %NULL
 *
 * Creates a new #GSettingsBackendChangeset with one change.  This is equivalent to
 * calling g_settings_backend_changeset_new() and then g_settings_backend_changeset_set() with
 * @path and @value.
 *
 * Returns: a new #GSettingsBackendChangeset
 **/
GSettingsBackendChangeset *
g_settings_backend_changeset_new_write (const gchar *path,
                                        GVariant    *value)
{
  GSettingsBackendChangeset *changeset;

  changeset = g_settings_backend_changeset_new ();
  g_settings_backend_changeset_set (changeset, path, value);

  return changeset;
}

/**
 * g_settings_backend_changeset_is_empty:
 * @changeset: a #GSettingsBackendChangeset
 *
 * Checks if @changeset is empty (ie: contains no changes).
 *
 * Returns: %TRUE if @changeset is empty
 **/
gboolean
g_settings_backend_changeset_is_empty (GSettingsBackendChangeset *changeset)
{
  return !g_hash_table_size (changeset->table);
}

/**
 * g_settings_backend_changeset_change:
 * @changeset: a #GSettingsBackendChangeset (to be changed)
 * @changes: the changes to make to @changeset
 *
 * Applies @changes to @changeset.
 *
 * If @changeset is a normal changeset then reset requests in @changes
 * will be allied to @changeset and then copied down into it.  In this
 * case the two changesets are effectively being merged.
 *
 * If @changeset is in database mode then the reset operations in
 * @changes will simply be applied to @changeset.
 *
 * Since: 0.16
 **/
void
g_settings_backend_changeset_change (GSettingsBackendChangeset *changeset,
                                     GSettingsBackendChangeset *changes)
{
  gsize prefix_len;
  gint i;

  g_return_if_fail (!changeset->is_sealed);

  /* Handling resets is a little bit tricky...
   *
   * Consider the case that we have @changeset containing a key /a/b and
   * @changes containing a reset request for /a/ and a set request for
   * /a/c.
   *
   * It's clear that at the end of this all, we should have only /a/c
   * but in order for that to be the case, we need to make sure that we
   * process the reset of /a/ before we process the set of /a/c.
   *
   * The easiest way to do this is to visit the strings in sorted order.
   * That removes the possibility of iterating over the hash table, but
   * g_settings_backend_changeset_build_description() makes the list in the order we
   * need so just call it and then iterate over the result.
   */

  if (!g_settings_backend_changeset_describe (changes, NULL, NULL, NULL))
    return;

  prefix_len = strlen (changes->prefix);
  for (i = 0; changes->paths[i]; i++)
    {
      const gchar *path;
      GVariant *value;

      /* The changes->paths are just pointers into the keys of the
       * hashtable, fast-forwarded past the prefix.  Rewind a bit.
       */
      path = changes->paths[i] - prefix_len;
      value = changes->values[i];

      g_settings_backend_changeset_set (changeset, path, value);
    }
}

/**
 * g_settings_backend_changeset_diff:
 * @from: a database mode changeset
 * @to: a database mode changeset
 *
 * Compares to database-mode changesets and produces a changeset that
 * describes their differences.
 *
 * If there is no difference, %NULL is returned.
 *
 * Applying the returned changeset to @from using
 * g_settings_backend_changeset_change() will result in the two changesets being
 * equal.
 *
 * Returns: (transfer full): the changes, or %NULL
 *
 * Since: 0.16
 */
GSettingsBackendChangeset *
g_settings_backend_changeset_diff (GSettingsBackendChangeset *from,
                                   GSettingsBackendChangeset *to)
{
  GSettingsBackendChangeset *changeset = NULL;
  GHashTableIter iter;
  gpointer key, val;

  g_return_val_if_fail (from->is_database, NULL);
  g_return_val_if_fail (to->is_database, NULL);

  /* We make no attempt to do dir resets, but we could...
   *
   * For now, we just reset each key individually.
   *
   * We create our list of changes in two steps:
   *
   *   - iterate the 'to' changeset and note any keys that do not have
   *     the same value in the 'from' changeset
   *
   *   - iterate the 'from' changeset and note any keys not present in
   *     the 'to' changeset, recording resets for them
   *
   * This will cover all changes.
   *
   * Note: because 'from' and 'to' are database changesets we don't have
   * to worry about seeing NULL values or dirs.
   */
  g_hash_table_iter_init (&iter, to->table);
  while (g_hash_table_iter_next (&iter, &key, &val))
    {
      GVariant *from_val = g_hash_table_lookup (from->table, key);

      if (from_val == NULL || !g_variant_equal (val, from_val))
        {
          if (!changeset)
            changeset = g_settings_backend_changeset_new ();

          g_settings_backend_changeset_set (changeset, key, val);
        }
    }

  g_hash_table_iter_init (&iter, from->table);
  while (g_hash_table_iter_next (&iter, &key, &val))
    if (!g_hash_table_lookup (to->table, key))
      {
        if (!changeset)
          changeset = g_settings_backend_changeset_new ();

        g_settings_backend_changeset_set (changeset, key, NULL);
      }

  return changeset;
}
