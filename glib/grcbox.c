/* grcbox.h: Reference counted data
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
#include "grefcount.h"

#include "valgrind.h"

#include <string.h>

/**
 * SECTION:rcbox
 * @Title: Reference counted data
 * @Short_description: Allocated memory with reference counting semantics
 *
 * A "reference counted box", or "RcBox", is an opaque wrapper data type
 * that is guaranteed to be as big as the size of a given data type, and
 * which augments the given data type with reference counting semantics
 * for its memory management.
 *
 * RcBox is useful if you have a plain old data type, like a structure
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
 *   Point *res = g_rc_box_new (Point);
 *
 *   res->x = x;
 *   res->y = y;
 *
 *   return res;
 * }
 * ]|
 *
 * Every time you wish to acquire a reference on the memory, you should
 * call g_rc_box_acquire(); similarly, when you wish to release a reference
 * you should call g_rc_box_release():
 *
 * |[<!-- language="C" -->
 * Point *
 * point_ref (Point *p)
 * {
 *   return g_rc_box_acquire (p);
 * }
 *
 * void
 * point_unref (Point *p)
 * {
 *   g_rc_box_release (p);
 * }
 * ]|
 *
 * If you have additional memory allocated inside the structure, you can
 * use g_rc_box_release_full(), which takes a function pointer, which
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
 *   g_rc_box_release_full (p, (GDestroyNotify) person_clear);
 * }
 * ]|
 *
 * If you wish to transfer the ownership of a reference counted data
 * type without increasing the reference count, you can use g_steal_pointer():
 *
 * |[<!-- language="C" -->
 *   Person *p = g_rc_box_new (Person);
 *
 *   fill_person_details (p);
 *
 *   add_person_to_database (db, g_steal_pointer (&p));
 * ]|
 *
 * The reference counting operations on data allocated using g_rc_box_alloc(),
 * g_rc_box_new(), and g_rc_box_dup() are not thread safe; it is your code's
 * responsibility to ensure that references are acquired are released on the
 * same thread.
 *
 * Since: 2.58.
 */

typedef struct {
  grefcount ref_count;

  gsize mem_size;

#ifndef G_DISABLE_ASSERT
  /* A "magic" number, used to perform additional integrity
   * checks on the allocated data
   */
  guint32 magic;
#endif
} GRcBox;

#define G_RC_BOX_MAGIC          0x44ae2bf0
#define G_RC_BOX_SIZE           sizeof (GRcBox)
#define G_RC_BOX(p)             (GRcBox *) (((char *) (p)) - G_RC_BOX_SIZE)

/* We use the same alignment as GTypeInstance and GNU libc's malloc */
#define STRUCT_ALIGNMENT        (2 * sizeof (gsize))
#define ALIGN_STRUCT(offset)    ((offset + (STRUCT_ALIGNMENT - 1)) & -STRUCT_ALIGNMENT)

static gpointer
g_rc_box_alloc_full (gsize    block_size,
                     gboolean clear)
{
  gsize private_size = G_RC_BOX_SIZE;
  gsize real_size = private_size + block_size;
  char *allocated;
  GRcBox *real_box;

#ifdef ENABLE_VALGRIND
  if (RUNNING_ON_VALGRIND)
    {
      /* When running under Valgrind we massage the memory allocation
       * to include a pointer at the tail end of the block; the pointer
       * is then set to the start of the block. This trick allows
       * Valgrind to keep track of the over-allocation and not be
       * confused when passing the pointer around
       */
      private_size += ALIGN_STRUCT (1);

      if (clear)
        allocated = g_malloc0 (real_size + sizeof (gpointer));
      else
        allocated = g_malloc (real_size + sizeof (gpointer));

      *(gpointer *) (allocated + private_size + block_size) = allocated + ALIGN_STRUCT (1);

      VALGRIND_MALLOCLIKE_BLOCK (allocated + private_size, block_size + sizeof (gpointer), 0, TRUE);
      VALGRIND_MALLOCLIKE_BLOCK (allocated + ALIGN_STRUCT (1), private_size - ALIGN_STRUCT (1), 0, TRUE);
    }
  else
#endif /* ENABLE_VALGRIND */
    {
      if (clear)
        allocated = g_malloc0 (real_size);
      else
        allocated = g_malloc (real_size);
    }

  real_box = (GRcBox *) allocated;
  real_box->mem_size = block_size;
#ifndef G_DISABLE_ASSERT
  real_box->magic = G_RC_BOX_MAGIC;
#endif
  g_ref_count_init (&real_box->ref_count);

  return allocated + private_size;
}

/**
 * g_rc_box_alloc:
 * @block_size: the size of the allocation
 *
 * Allocates @block_size bytes of memory, and adds reference
 * counting semantics to it.
 *
 * The data will be freed when its reference count drops to
 * zero.
 *
 * Returns: a pointer to the allocated memory
 *
 * Since: 2.58
 */
gpointer
g_rc_box_alloc (gsize block_size)
{
  g_return_val_if_fail (block_size > 0, NULL);

  return g_rc_box_alloc_full (block_size, FALSE);
}

/**
 * g_rc_box_alloc0:
 * @block_size: the size of the allocation
 *
 * Allocates @block_size bytes of memory, and adds reference
 * counting semantics to it.
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
g_rc_box_alloc0 (gsize block_size)
{
  g_return_val_if_fail (block_size > 0, NULL);

  return g_rc_box_alloc_full (block_size, TRUE);
}

/**
 * g_rc_box_new:
 * @type: the type to allocate, typically a structure name
 *
 * A convenience macro to allocate reference counted data with
 * the size of the given @type.
 *
 * This macro calls g_rc_box_alloc() with `sizeof (@type)` and
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
 * g_rc_box_new0:
 * @type: the type to allocate, typically a structure name
 *
 * A convenience macro to allocate reference counted data with
 * the size of the given @type, and set its contents to 0.
 *
 * This macro calls g_rc_box_alloc0() with `sizeof (@type)` and
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
 * g_rc_box_dup:
 * @mem_block: (not nullable): a pointer to reference counted data
 *
 * Allocates a new block of data with reference counting
 * semantics, and copies the contents of @mem_block into
 * it.
 *
 * Returns: (not nullable): a pointer to the allocated memory
 *
 * Since: 2.58
 */
gpointer
(g_rc_box_dup) (gpointer mem_block)
{
  GRcBox *real_box = G_RC_BOX (mem_block);
  gpointer res;

  g_return_val_if_fail (mem_block != NULL, NULL);
#ifndef G_DISABLE_ASSERT
  g_return_val_if_fail (real_box->magic == G_RC_BOX_MAGIC, NULL);
#endif

  res = g_rc_box_alloc_full (real_box->mem_size, FALSE);
  memcpy (res, mem_block, real_box->mem_size);

  return res;
}

/**
 * g_rc_box_acquire:
 * @mem_block: (not nullable): a pointer to reference counted data
 *
 * Acquires a reference on the data pointed by @mem_block.
 *
 * Returns: (not nullabl): a pointer to the data, with its reference
 *   count increased
 *
 * Since: 2.58
 */
gpointer
(g_rc_box_acquire) (gpointer mem_block)
{
  GRcBox *real_box = G_RC_BOX (mem_block);

  g_return_val_if_fail (mem_block != NULL, NULL);
#ifndef G_DISABLE_ASSERT
  g_return_val_if_fail (real_box->magic == G_RC_BOX_MAGIC, NULL);
#endif

  g_ref_count_inc (&real_box->ref_count);

  return mem_block;
}

/**
 * g_rc_box_release:
 * @mem_block: (not nullable): a pointer to reference counted data
 *
 * Releases a reference on the data pointed by @mem_block.
 *
 * If the reference was the last one, it will free the
 * resources allocated for @mem_block.
 *
 * Since: 2.58
 */
void
g_rc_box_release (gpointer mem_block)
{
  GRcBox *real_box = G_RC_BOX (mem_block);

  g_return_if_fail (mem_block != NULL);
#ifndef G_DISABLE_ASSERT
  g_return_if_fail (real_box->magic == G_RC_BOX_MAGIC);
#endif

  if (g_ref_count_dec (&real_box->ref_count))
    g_free (real_box);
}

/**
 * g_rc_box_release_full:
 * @mem_block: (not nullabl): a pointer to reference counted data
 * @clear_func: (not nullable): a function to call when clearing the data
 *
 * Releases a reference on the data pointed by @mem_block.
 *
 * If the reference was the last one, it will call @clear_func
 * to clear the contents of @mem_block, and then will free the
 * resources allocated for @mem_block.
 *
 * Since: 2.58
 */
void
g_rc_box_release_full (gpointer       mem_block,
                       GDestroyNotify clear_func)
{
  GRcBox *real_box = G_RC_BOX (mem_block);

  g_return_if_fail (mem_block != NULL);
  g_return_if_fail (clear_func != NULL);
#ifndef G_DISABLE_ASSERT
  g_return_if_fail (real_box->magic == G_RC_BOX_MAGIC);
#endif

  if (g_ref_count_dec (&real_box->ref_count))
    {
      clear_func (mem_block);
      g_free (real_box);
    }
}
