/* GLib - Library of useful routines for C programming
 * Copyright 2016  Emmanuele Bassi
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * MT safe
 */

#include "config.h"

#include "grefcount.h"

#include "gatomic.h"

/**
 * SECTION:refcount
 * @Title: Reference counting
 * @Short_desc: Reference counters and reference counted data
 *
 */

/**
 * g_ref_counter_init:
 * @ref_count: the address of a reference counter
 * @is_atomic: whether the reference counter should be atomic
 *
 * Initializes a reference counter to its initial state.
 *
 * If @is_atomic is %TRUE, acquiring and releasing a reference on the
 * counter will use an atomic operation.
 *
 * A reference counter is used to provide reference counting to a data
 * structure, e.g.:
 *
 * |[<!-- language="C" -->
 * typedef struct {
 *   volatile int ref_count;
 *
 *   int some_value;
 *   char *other_value;
 * } Object;
 *
 * Object *
 * object_new (void)
 * {
 *   Object *res = g_new0 (Object, 1);
 *
 *   g_ref_counter_init (&res->ref_count, FALSE);
 *
 *   return res;
 * }
 * ]|
 *
 * This allows safely passing references to instances of the data structure,
 * without necessarily copying them, and releasing allocated resources when
 * the data is not needed any more.
 *
 * Since: 2.52
 */
void
g_ref_counter_init (volatile int *ref_count,
                    gboolean      is_atomic)
{
  *ref_count = is_atomic ? -1 : 1;
}

/**
 * g_ref_counter_acquire:
 * @ref_count: the address of a reference counter
 *
 * Acquires a reference on the counter.
 *
 * This function should be used to implement a "ref" operation, e.g.:
 *
 * |[<!-- language="C" -->
 * Object *
 * object_ref (Object *obj)
 * {
 *   g_ref_counter_acquire (&obj->ref_count);
 * }
 * ]|
 *
 * Since: 2.52
 */
void
g_ref_counter_acquire (volatile int *ref_count)
{
  int refs;

test_again:
  refs = *ref_count;

  if (refs > 0)
    *ref_count += 1;
  else if (G_UNLIKELY (!g_atomic_int_compare_and_exchange (ref_count, refs, refs - 1)))
    goto test_again;
}

/**
 * g_ref_counter_release:
 * @ref_counter: the address of a reference counter
 *
 * Releases a reference on the counter.
 *
 * Returns: %TRUE if the reference released was the last one,
 *   and %FALSE otherwise
 *
 * Since: 2.52
 */
gboolean
g_ref_counter_release (volatile int *ref_count)
{
  int refs;

test_again:
  refs = *ref_count;

  if (refs == -1 || refs == 1)
    return TRUE;

  if (refs > 0)
    *ref_count -= 1;
  else if (G_UNLIKELY (!g_atomic_int_compare_and_exchange (ref_count, refs, refs + 1)))
    goto test_again;

  return FALSE;
}

/**
 * g_ref_counter_make_atomic:
 * @ref_count: the address of a reference counter
 *
 * Makes reference counting operations atomic.
 *
 * This operation cannot be undone.
 *
 * Since: 2.52
 */
void
g_ref_counter_make_atomic (volatile int *ref_count)
{
  int refs;

test_again:
  refs = *ref_count;

  if (refs > 0 && !g_atomic_int_compare_and_exchange (ref_count, refs, -refs))
    goto test_again;
}

/**
 * g_ref_counter_is_atomic:
 * @ref_count: the address of a reference counter
 *
 * Checks whether operations on a reference counter are atomic.
 *
 * Returns: %TRUE if the reference counting operations are atomic
 *
 * Since: 2.52
 */
gboolean
g_ref_counter_is_atomic (volatile int *ref_count)
{
  int sign = g_atomic_int_get (ref_count);

  return sign < 0;
}
