/* garcbox.c: Atomically reference counted data
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

#include "config.h"

#include "grcbox.h"

#include "gmessages.h"
#include "grcboxprivate.h"
#include "grefcount.h"

#ifdef ENABLE_VALGRIND
#include "valgrind.h"
#endif

#include <string.h>

#define G_ARC_BOX(p)            (GArcBox *) (((char *) (p)) - G_ARC_BOX_SIZE)

/**
 * SECTION:arcbox
 * @Title: Atomically reference counted data
 * @Short_description: Allocated memory with atomic reference counting semantics
 *
 * An "atomically reference counted box", or "ArcBox", is an opaque wrapper
 * data type that is guaranteed to be as big as the size of a given data type,
 * and which augments the given data type with thread safe reference counting
 * semantics for its memory management.
 *
 * ArcBox is useful if you have a plain old data type, like a structure
 * typically placed on the stack, and you wish to provide additional API
 * to use it on the heap, without necessarily implementing copy/free
 * semantics, or your own reference counting.
 *
 * The typical use is:
 *
 * |[<!-- language="C" -->
 * typedef struct {
 *   float x, y;
 * } Point;
 *
 * Point *
 * point_new (float x, float y)
 * {
 *   Point *res = g_arc_box_new (Point);
 *
 *   res->x = x;
 *   res->y = y;
 *
 *   return res;
 * }
 * ]|
 *
 * Every time you wish to acquire a reference on the memory, you should
 * call g_arc_box_acquire(); similarly, when you wish to release a reference
 * you should call g_arc_box_release():
 *
 * |[<!-- language="C" -->
 * Point *
 * point_ref (Point *p)
 * {
 *   return g_arc_box_acquire (p);
 * }
 *
 * void
 * point_unref (Point *p)
 * {
 *   g_arc_box_release (p);
 * }
 * ]|
 *
 * If you have additional memory allocated inside the structure, you can
 * use g_arc_box_release_full(), which takes a function pointer, which
 * will be called if the reference released was the last:
 *
 * |[<!-- language="C" -->
 * typedef struct {
 *   char *name;
 *   char *address;
 *   char *city;
 *   char *state;
 *   int age;
 * } Person;
 *
 * void
 * person_clear (Person *p)
 * {
 *   g_free (p->name);
 *   g_free (p->address);
 *   g_free (p->city);
 *   g_free (p->state);
 * }
 *
 * void
 * person_unref (Person *p)
 * {
 *   g_arc_box_release_full (p, (GDestroyNotify) person_clear);
 * }
 * ]|
 *
 * If you wish to transfer the ownership of a reference counted data
 * type without increasing the reference count, you can use g_steal_pointer():
 *
 * |[<!-- language="C" -->
 *   Person *p = g_arc_box_new (Person);
 *
 *   fill_person_details (p);
 *
 *   add_person_to_database (db, g_steal_pointer (&p));
 * ]|
 *
 * The reference counting operations on data allocated using g_arc_box_alloc(),
 * g_arc_box_new(), and g_arc_box_dup() are guaranteed to be atomic, and thus
 * can be safely be performed by different threads. It is important to note that
 * only the reference acquisition and release are atomic; changes to the content
 * of the data are your responsibility.
 *
 * Since: 2.58.
 */

/**
 * g_arc_box_alloc:
 * @block_size: the size of the allocation
 *
 * Allocates @block_size bytes of memory, and adds atomic
 * reference counting semantics to it.
 *
 * The data will be freed when its reference count drops to
 * zero.
 *
 * Returns: a pointer to the allocated memory
 *
 * Since: 2.58
 */
gpointer
g_arc_box_alloc (gsize block_size)
{
  g_return_val_if_fail (block_size > 0, NULL);

  return g_rc_box_alloc_full (block_size, TRUE, FALSE);
}

/**
 * g_arc_box_alloc0:
 * @block_size: the size of the allocation
 *
 * Allocates @block_size bytes of memory, and adds atomic
 * referenc counting semantics to it.
 *
 * The contents of the returned data is set to 0's.
 *
 * The data will be freed when its reference count drops to
 * zero.
 *
 * Returns: a pointer to the allocated memory
 *
 * Since: 2.58
 */
gpointer
g_arc_box_alloc0 (gsize block_size)
{
  g_return_val_if_fail (block_size > 0, NULL);

  return g_rc_box_alloc_full (block_size, TRUE, TRUE);
}

/**
 * g_arc_box_new:
 * @type: the type to allocate, typically a structure name
 *
 * A convenience macro to allocate atomically reference counted
 * data with the size of the given @type.
 *
 * This macro calls g_arc_box_alloc() with `sizeof (@type)` and
 * casts the returned pointer to a pointer of the given @type,
 * avoiding a type cast in the source code.
 *
 * This macro cannot return %NULL, as the minimum allocation
 * size from `sizeof (@type)` is 1 byte.
 *
 * Returns: (not nullable): a pointer to the allocated memory,
 *   cast to a pointer for the given @type
 *
 * Since: 2.58
 */

/**
 * g_arc_box_new0:
 * @type: the type to allocate, typically a structure name
 *
 * A convenience macro to allocate atomically reference counted
 * data with the size of the given @type, and set its contents
 * to 0.
 *
 * This macro calls g_arc_box_alloc0() with `sizeof (@type)` and
 * casts the returned pointer to a pointer of the given @type,
 * avoiding a type cast in the source code.
 *
 * This macro cannot return %NULL, as the minimum allocation
 * size from `sizeof (@type)` is 1 byte.
 *
 * Returns: (not nullable): a pointer to the allocated memory,
 *   cast to a pointer for the given @type
 *
 * Since: 2.58
 */

/**
 * g_arc_box_dup:
 * @mem_block: (not nullable): a pointer to reference counted data
 *
 * Allocates a new block of data with atomic reference counting
 * semantics, and copies the contents of @mem_block into
 * it.
 *
 * Returns: (not nullable): a pointer to the allocated memory
 *
 * Since: 2.58
 */
gpointer
(g_arc_box_dup) (gpointer mem_block)
{
  GArcBox *real_box = G_ARC_BOX (mem_block);
  gpointer res;

  g_return_val_if_fail (mem_block != NULL, NULL);
#ifndef G_DISABLE_ASSERT
  g_return_val_if_fail (real_box->magic == G_BOX_MAGIC, NULL);
#endif

  res = g_rc_box_alloc_full (real_box->mem_size, TRUE, FALSE);
  memcpy (res, mem_block, real_box->mem_size);

  return res;
}

/**
 * g_arc_box_acquire:
 * @mem_block: (not nullable): a pointer to reference counted data
 *
 * Atomically acquires a reference on the data pointed by @mem_block.
 *
 * Returns: (not nullabl): a pointer to the data, with its reference
 *   count increased
 *
 * Since: 2.58
 */
gpointer
(g_arc_box_acquire) (gpointer mem_block)
{
  GArcBox *real_box = G_ARC_BOX (mem_block);

  g_return_val_if_fail (mem_block != NULL, NULL);
#ifndef G_DISABLE_ASSERT
  g_return_val_if_fail (real_box->magic == G_BOX_MAGIC, NULL);
#endif

  g_atomic_ref_count_inc (&real_box->ref_count);

  return mem_block;
}

/**
 * g_arc_box_release:
 * @mem_block: (not nullable): a pointer to reference counted data
 *
 * Atomically releases a reference on the data pointed by @mem_block.
 *
 * If the reference was the last one, it will free the
 * resources allocated for @mem_block.
 *
 * Since: 2.58
 */
void
g_arc_box_release (gpointer mem_block)
{
  GArcBox *real_box = G_ARC_BOX (mem_block);

  g_return_if_fail (mem_block != NULL);
#ifndef G_DISABLE_ASSERT
  g_return_if_fail (real_box->magic == G_BOX_MAGIC);
#endif

  if (g_atomic_ref_count_dec (&real_box->ref_count))
    g_free (real_box);
}

/**
 * g_arc_box_release_full:
 * @mem_block: (not nullable): a pointer to reference counted data
 * @clear_func: (not nullable): a function to call when clearing the data
 *
 * Atomically releases a reference on the data pointed by @mem_block.
 *
 * If the reference was the last one, it will call @clear_func
 * to clear the contents of @mem_block, and then will free the
 * resources allocated for @mem_block.
 *
 * Since: 2.58
 */
void
g_arc_box_release_full (gpointer       mem_block,
                        GDestroyNotify clear_func)
{
  GArcBox *real_box = G_ARC_BOX (mem_block);

  g_return_if_fail (mem_block != NULL);
  g_return_if_fail (clear_func != NULL);
#ifndef G_DISABLE_ASSERT
  g_return_if_fail (real_box->magic == G_BOX_MAGIC);
#endif

  if (g_atomic_ref_count_dec (&real_box->ref_count))
    {
      clear_func (mem_block);
      g_free (real_box);
    }
}
