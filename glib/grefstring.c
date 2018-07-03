/* grefstring.c: Reference counted strings
 *
 * Copyright 2018  Emmanuele Bassi
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:refstring
 * @Title: Reference counted strings
 * @Short_description: Strings with reference counted memory management
 *
 * Reference counted strings are normal C strings that have been augmented
 * with a reference counter to manage their resources. You allocate a new
 * reference counted string and acquire and release references as needed,
 * instead of copying the string among callers; when the last reference on
 * the string is released, the resources allocated for it are freed.
 *
 * Since: 2.58
 */

#include "config.h"

#include "grefstring.h"

#include "ghash.h"
#include "gmessages.h"
#include "grcbox.h"
#include "gthread.h"

#include <string.h>

G_LOCK_DEFINE_STATIC (interned_ref_strings);
static GHashTable *interned_ref_strings;

/**
 * g_ref_string_new:
 * @str: (not nullable): a NUL-terminated string
 *
 * Creates a new reference counted string and copies the contents of @str
 * into it.
 *
 * Returns: (not nullable): the newly created reference counted string
 *
 * Since: 2.58
 */
char *
g_ref_string_new (const char *str)
{
  char *res;
  gsize len;

  g_return_val_if_fail (str != NULL && *str != '\0', NULL);
  
  len = strlen (str);
  
  res = (char *) g_atomic_rc_box_dup (sizeof (char) * len + 1, str);
  res[len] = '\0';

  return res;
}

/**
 * g_ref_string_new_intern:
 * @str: (not nullable): a NUL-terminated string
 *
 * Creates a new reference counted string and copies the content of @str
 * into it.
 *
 * If you call this function multiple times with the same @str, or with
 * the same contents of @str, it will return a new reference, instead of
 * creating a new string.
 *
 * Returns: (not nullable): the newly created reference counted string, or
 *   a new reference to an existing string
 *
 * Since: 2.58
 */
char *
g_ref_string_new_intern (const char *str)
{
  gsize len;
  char *res;

  g_return_val_if_fail (str != NULL && *str != '\0', NULL);

  G_LOCK (interned_ref_strings);

  if (G_UNLIKELY (interned_ref_strings == NULL))
    interned_ref_strings = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                  (GDestroyNotify) g_atomic_rc_box_release,
                                                  NULL);

  res = g_hash_table_lookup (interned_ref_strings, str);
  if (res != NULL)
    {
      G_UNLOCK (interned_ref_strings);
      return g_atomic_rc_box_acquire (res);
    }

  len = strlen (str);
  res = (char *) g_atomic_rc_box_dup (sizeof (char) * len + 1, str);
  res[len] = '\0';

  g_hash_table_add (interned_ref_strings, g_atomic_rc_box_acquire (res));

  G_UNLOCK (interned_ref_strings);

  return res;
}

/**
 * g_ref_string_acquire:
 * @str: a reference counted string
 *
 * Acquires a reference on a string.
 *
 * Returns: the given string, with its reference count increased
 *
 * Since: 2.58
 */
char *
g_ref_string_acquire (char *str)
{
  g_return_val_if_fail (str != NULL && *str != '\0', NULL);

  return g_atomic_rc_box_acquire (str);
}

static void
remove_if_interned (gpointer data)
{
  char *str = data;

  G_LOCK (interned_ref_strings);

  if (G_LIKELY (interned_ref_strings != NULL))
    {
      if (g_hash_table_contains (interned_ref_strings, str))
        g_hash_table_remove (interned_ref_strings, str);

      if (g_hash_table_size (interned_ref_strings) == 0)
        g_clear_pointer (&interned_ref_strings, g_hash_table_destroy);
    }

  G_UNLOCK (interned_ref_strings);
}

/**
 * g_ref_string_release:
 * @str: a reference counted string
 *
 * Releases a reference on a string; if it was the last reference, the
 * resources allocated by the string are freed as well.
 *
 * Since: 2.58
 */
void
g_ref_string_release (char *str)
{
  g_return_if_fail (str != NULL && *str != '\0');

  g_atomic_rc_box_release_full (str, remove_if_interned);
}

/**
 * g_ref_string_length:
 * @str: a reference counted string
 *
 * Retrieves the length of @str.
 *
 * Returns: the length of the given string, in bytes
 *
 * Since: 2.58
 */
gsize
g_ref_string_length (char *str)
{
  g_return_val_if_fail (str != NULL && *str != '\0', 0);

  return g_atomic_rc_box_get_size (str) - 1;
}
