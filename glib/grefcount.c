/*
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
 * SECTION:refcount
 * @Title: Reference counting
 * @Short_description: Reference counting types and functions
 *
 * The #grefcount and #gatomicrefcount types provide simple and atomic
 * reference counting, respectively.
 *
 * You should use these types when implementing reference counting
 * semantics on a data type. You should initialize the type using
 * g_ref_count_init() or g_atomic_ref_count_init(); every time you
 * acquire a reference, you should call g_ref_count_inc() or
 * g_atomic_ref_count_inc(); and every time you release a reference,
 * you should call g_ref_count_dec() or g_atomic_ref_count_dec(), and
 * check the return value to know if it was the last reference held,
 * and it's time to free the resources associated with your reference
 * counted data type.
 *
 * |[<!-- language="C" -->
 * // A simple reference counted string data type
 * typedef struct {
 *   gatomicrefcount ref_count;
 *
 *   char *str;
 *   int length;
 * } StringRef;
 *
 * StringRef *
 * string_ref_new (const char *str)
 * {
 *   StringRef *ref = g_new0 (StringRef, 1);
 *
 *   ref->str = g_strdup (str);
 *   ref->len = strlen (str);
 *
 *   g_atomic_ref_count_init (&ref->ref_count);
 *
 *   return ref;
 * }
 *
 * // Acquire a reference on StringRef
 * StringRef *
 * string_ref_acquire (StringRef *ref)
 * {
 *   g_atomic_ref_count_inc (&ref->ref_count);
 *   return ref;
 * }
 *
 * // Release a reference on StringRef, and frees the data
 * // if it's the last reference
 * void
 * string_ref_release (StringRef *ref)
 * {
 *   if (g_atomic_ref_count_dec (&ref->ref_count))
 *     {
 *       g_free (ref->str);
 *       g_free (ref);
 *     }
 * }
 * ]|
 */

#include "config.h"

#include "grefcount.h"

#include "gatomic.h"
#include "gmessages.h"

/**
 * g_ref_count_init:
 * @rc: the address of a reference count variable
 *
 * Initializes a reference count variable.
 *
 * Since: 2.56
 */
void
g_ref_count_init (grefcount *rc)
{
  g_return_if_fail (rc != NULL);

  *rc = -1;
}

/**
 * g_ref_count_inc:
 * @rc: the address of a reference count variable
 *
 * Increases the reference count.
 *
 * Since: 2.56
 */
void
g_ref_count_inc (grefcount *rc)
{
  g_return_if_fail (rc != NULL);

  grefcount rrc = *rc;

  g_return_if_fail (rrc < 0);

  rrc -= 1;
  if (rrc == G_MININT)
    {
      g_critical ("Reference counter %p is saturated", rc);
      return;
    }

  *rc = rrc;
}

/**
 * g_ref_count_dec:
 * @rc: the address of a reference count variable
 *
 * Decreases the reference count.
 *
 * Returns: %TRUE if the reference count reached 0, and %FALSE otherwise
 *
 * Since: 2.56
 */
gboolean
g_ref_count_dec (grefcount *rc)
{
  g_return_val_if_fail (rc != NULL, FALSE);

  grefcount rrc = *rc;
  g_return_val_if_fail (rrc < 0, FALSE);

  rrc += 1;

  *rc = rrc;

  if (rrc == 0)
    return TRUE;

  return FALSE;
}

/**
 * g_ref_count_compare:
 * @rc: the address of a reference count variable
 * @val: the value to compare
 *
 * Compares the current value of @rc with @val.
 *
 * Returns: %TRUE if the reference count is the same
 *   as the given value
 *
 * Since: 2.56
 */
gboolean
g_ref_count_compare (grefcount *rc,
                     gint       val)
{
  g_return_val_if_fail (rc != NULL, FALSE);

  grefcount rrc = *rc;

  return rrc == -val;
}

/**
 * g_atomic_ref_count_init:
 * @arc: the address of an atomic reference count variable
 *
 * Atomically initializes a reference count variable.
 *
 * Since: 2.56
 */
void
g_atomic_ref_count_init (gatomicrefcount *arc)
{
  g_return_if_fail (arc != NULL);

  g_atomic_int_set (arc, 1);
}

/**
 * g_atomic_ref_count_inc:
 * @arc: the address of an atomic reference count variable
 *
 * Atomically increases the reference count.
 *
 * Since: 2.56
 */
void
g_atomic_ref_count_inc (gatomicrefcount *arc)
{
  gatomicrefcount rc;

  g_return_if_fail (arc != NULL);

  rc = g_atomic_int_get (arc);
  g_return_if_fail (rc > 0);

  while (TRUE)
    {
      gatomicrefcount new = rc + 1;

      if (new == G_MAXINT)
        {
          g_critical ("Atomic reference counter %p is saturated", arc);
          return;
        }

      if (g_atomic_int_compare_and_exchange (arc, rc, new))
        break;
   }
}

/**
 * g_atomic_ref_count_dec:
 * @arc: the address of an atomic reference count variable
 *
 * Atomically decreases the reference count.
 *
 * Returns: %TRUE if the reference count reached 0, and %FALSE otherwise
 *
 * Since: 2.56
 */
gboolean
g_atomic_ref_count_dec (gatomicrefcount *arc)
{
  g_return_val_if_fail (arc != NULL, FALSE);
  g_return_val_if_fail (g_atomic_int_get (arc) > 0, FALSE);

  return g_atomic_int_dec_and_test (arc);
}

/**
 * g_atomic_ref_count_compare:
 * @arc: the address of an atomic reference count variable
 * @val: the value to compare
 *
 * Atomically compares the current value of @arc with @val.
 *
 * Returns: %TRUE if the reference count is the same
 *   as the given value
 *
 * Since: 2.56
 */
gboolean
g_atomic_ref_count_compare (gatomicrefcount *arc,
                            gint             val)
{
  g_return_val_if_fail (arc != NULL, FALSE);

  return g_atomic_int_get (arc) == val;
}
