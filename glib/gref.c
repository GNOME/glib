/* gref.h: Reference counted memory areas
 *
 * Copyright 2015  Emmanuele Bassi
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
 */

/**
 * SECTION:refcount
 * @short_description: reference counted memory areas
 * @title: References
 *
 * These functions provide support for allocating and freeing reference
 * counted memory areas.
 *
 * Reference counted memory areas are kept alive as long as something holds
 * a reference on them; as soon as their reference count drops to zero, the
 * associated memory is freed.
 */

#include "config.h"
#include "glibconfig.h"

#include "gref.h"

#include "gatomic.h"
#include "gconstructor.h"
#include "gmessages.h"
#include "gtestutils.h"
#include "valgrind.h"

#include <string.h>

/**
 * g_ref_count_init:
 * @refcount: a pointer to an integer used for reference counting
 * @atomic: whether reference counting should be atomic
 *
 * Initializes a reference counting value.
 *
 * Since: 2.44
 */
void
g_ref_count_init (volatile int *refcount,
                  gboolean      atomic)
{
  *refcount = atomic ? -1 : 1;
}

/**
 * g_ref_count_inc:
 * @refcount: a pointer to an integer used for reference counting
 *
 * Increases a reference counter, using atomic operations if needed.
 *
 * Since: 2.44
 */
void
g_ref_count_inc (volatile int *refcount)
{
  int refs;

test_again:
  refs = *refcount;

  if (refs > 0)
    *refcount += 1;
  else if (G_UNLIKELY (!g_atomic_int_compare_and_exchange (refcount, refs, refs - 1)))
    goto test_again;
}

/**
 * g_ref_count_dec:
 * @refcount: a pointer to an integer used for reference counting
 *
 * Decreases a reference counter, using atomic operations if needed.
 *
 * Returns: %TRUE if the reference counter hit zero
 *
 * Since: 2.44
 */
gboolean
g_ref_count_dec (volatile int *refcount)
{
  int refs;

test_again:
  refs = *refcount;
  if (refs == -1 || refs == 1)
    return TRUE;
  if (refs > 0)
    *refcount -= 1;
  else if (G_UNLIKELY (!g_atomic_int_compare_and_exchange (refcount, refs, refs + 1)))
    goto test_again;

  return FALSE;
}

/**
 * g_ref_count_make_atomic:
 * @refcount: a pointer to an integer used for reference counting
 *
 * Changes the reference counting semantics of a reference counter to
 * be atomic if they weren't.
 *
 * Since: 2.44
 */
void
g_ref_count_make_atomic (volatile int *refcount)
{
  int refs = g_atomic_int_get (refcount);

  if (refs > 0)
    *refcount = -refs;
}

#define STRUCT_ALIGNMENT        (2 * sizeof (gsize))
#define ALIGN_STRUCT(offset)    ((offset + (STRUCT_ALIGNMENT - 1)) & -STRUCT_ALIGNMENT)

#define G_REF_PTR_SIZE          sizeof (GRefPtr)
#define G_REF_PTR(ptr)          (GRefPtr *) (((char *) (ptr)) - G_REF_PTR_SIZE)

typedef struct _GRefPtr         GRefPtr;

struct _GRefPtr
{
  volatile int ref_count;

  gsize alloc_size;

  GDestroyNotify notify;
};

#ifdef G_ENABLE_DEBUG
#include "ghash.h"
#include "gthread.h"

static GHashTable *referenced_pointers;
G_LOCK_DEFINE_STATIC (referenced_pointers);

static inline void
g_ref_pointer_register (gpointer ref)
{
  G_LOCK (referenced_pointers);
  if (G_UNLIKELY (referenced_pointers == NULL))
    referenced_pointers = g_hash_table_new (NULL, NULL);

  g_hash_table_add (referenced_pointers, ref);
  G_UNLOCK (referenced_pointers);
}

static inline void
g_ref_pointer_unregister (gpointer ref)
{
  G_LOCK (referenced_pointers);
  if (G_LIKELY (referenced_pointers != NULL))
    g_hash_table_remove (referenced_pointers, ref);
  G_UNLOCK (referenced_pointers);
}

static gboolean
g_is_ref_pointer (gconstpointer ref)
{
  gboolean res = FALSE;

  G_LOCK (referenced_pointers);
  if (G_LIKELY (referenced_pointers != NULL))
    res = g_hash_table_contains (referenced_pointers, ref);
  G_UNLOCK (referenced_pointers);

  return res;
}

#ifdef G_DEFINE_DESTRUCTOR_NEEDS_PRAGMA
#pragma G_DEFINE_DESTRUCTOR_PRAGMA_ARGS(glib_ref_pointers_clear)
#endif
G_DEFINE_DESTRUCTOR(glib_ref_pointers_clear)

static void
glib_ref_pointers_clear (void)
{
  if (referenced_pointers != NULL)
    g_hash_table_unref (referenced_pointers);
}

#endif /* G_ENABLE_DEBUG */

/**
 * g_ref_pointer_new:
 * @Type: the type to allocate
 * @free_func: the free function
 *
 * Allocates a reference counted memory area corresponding to @Type.
 *
 * See also: g_ref_pointer_alloc()
 *
 * Returns: the newly allocated memory area
 *
 * Since: 2.44
 */

/**
 * g_ref_pointer_new0:
 * @Type: the type to allocate
 * @free_func: the free function
 *
 * Allocates and clears a reference counted memory area corresponding
 * to @Type.
 *
 * See also: g_ref_pointer_alloc0()
 *
 * Returns: the newly allocated memory area
 *
 * Since: 2.44
 */

/*< private >
 * g_ref_pointer_destroy:
 * @ref: a reference counted memory area
 *
 * Forces the destruction of a reference counter memory area.
 */
static void
g_ref_pointer_destroy (gpointer ref)
{
  GRefPtr *real = G_REF_PTR (ref);
  gsize alloc_size = real->alloc_size;
  gsize private_size = G_REF_PTR_SIZE;
  char *allocated = ((char *) ref) - private_size;

  if (real->notify != NULL)
    real->notify (ref);

#ifdef G_ENABLE_DEBUG
  g_ref_pointer_unregister (ref);
#endif

  if (RUNNING_ON_VALGRIND)
    {
      private_size += ALIGN_STRUCT (1);
      allocated -= ALIGN_STRUCT (1);

      *(gpointer *) (allocated + private_size + alloc_size) = NULL;
      g_free (allocated);
    }
  else
    g_free (allocated);
}

static gpointer
g_ref_pointer_alloc_internal (gsize          alloc_size,
                              gboolean       clear,
                              GDestroyNotify notify)
{
  gsize private_size = G_REF_PTR_SIZE;
  gchar *allocated;
  GRefPtr *real;

  g_return_val_if_fail (alloc_size != 0, NULL);

  if (RUNNING_ON_VALGRIND)
    {
      private_size += ALIGN_STRUCT (1);

      if (clear)
        allocated = g_malloc0 (private_size + alloc_size + sizeof (gpointer));
      else
        allocated = g_malloc (private_size + alloc_size + sizeof (gpointer));

      *(gpointer *) (allocated + private_size + alloc_size) = allocated + ALIGN_STRUCT (1);

      VALGRIND_MALLOCLIKE_BLOCK (allocated + private_size, alloc_size + sizeof (gpointer), 0, TRUE);
      VALGRIND_MALLOCLIKE_BLOCK (allocated + ALIGN_STRUCT (1), private_size - ALIGN_STRUCT (1), 0, TRUE);
    }
  else
    {
      if (clear)
        allocated = g_malloc0 (private_size + alloc_size);
      else
        allocated = g_malloc (private_size + alloc_size);
    }

#ifdef G_ENABLE_DEBUG
  g_ref_pointer_register (allocated + private_size);
#endif

  real = (GRefPtr *) allocated;
  real->notify = notify;
  real->alloc_size = alloc_size;
  g_ref_count_init (&real->ref_count, FALSE);

  return allocated + private_size;
}

/**
 * g_ref_pointer_alloc:
 * @alloc_size: the number of bytes to allocate
 * @notify: (nullable): a function to be called when the reference
 *   count drops to zero
 *
 * Allocates a reference counted memory area.
 *
 * Reference counted memory areas are automatically freed when their
 * reference count drops to zero.
 *
 * Use g_ref_pointer_acquire() to acquire a reference, and
 * g_ref_pointer_release() to release it when you're done.
 *
 * The contents of the returned memory are undefined.
 *
 * Returns: a pointer to the allocated memory
 *
 * Since: 2.44
 */
gpointer
g_ref_pointer_alloc (gsize          alloc_size,
                     GDestroyNotify notify)
{
  return g_ref_pointer_alloc_internal (alloc_size, FALSE, notify);
}

/**
 * g_ref_pointer_alloc0:
 * @alloc_size: the number of bytes to allocate
 * @notify: (nullable): a function to be called when the reference
 *   count drops to zero
 *
 * Allocates a reference counted memory area.
 *
 * Reference counted memory areas are automatically freed when their
 * reference count drops to zero.
 *
 * Use g_ref_pointer_acquire() to acquire a reference, and
 * g_ref_pointer_release() to release it.
 *
 * The contents of the returned memory are set to 0.
 *
 * Returns: a pointer to the allocated memory
 *
 * Since: 2.44
 */
gpointer
g_ref_pointer_alloc0 (gsize          alloc_size,
                      GDestroyNotify notify)
{
  return g_ref_pointer_alloc_internal (alloc_size, TRUE, notify);
}

/**
 * g_ref_pointer_take:
 * @data: data to duplicate
 * @alloc_size: the size of @data
 * @notify: notification function to be called when the reference
 *   count drops to zero
 *
 * Duplicates existing data into a reference counted memory area.
 *
 * Returns: the newly allocated reference counted area
 *
 * Since: 2.44
 */
gpointer
g_ref_pointer_take (gconstpointer  data,
                    gsize          alloc_size,
                    GDestroyNotify notify)
{
  gpointer res = g_ref_pointer_alloc (alloc_size, notify);

  memcpy (res, data, alloc_size);

  return res;
}

/**
 * g_ref_pointer_acquire:
 * @ref: a reference counted memory area
 *
 * Acquires a reference on the given memory area.
 *
 * Use g_ref_pointer_release() to release the reference when done.
 *
 * You should only call this function if you are implementing
 * a reference counted type.
 *
 * Returns: the memory area, with its reference count increased by 1
 *
 * Since: 2.44
 */
gpointer
g_ref_pointer_acquire (gpointer ref)
{
  GRefPtr *real = G_REF_PTR (ref);

  g_return_val_if_fail (ref != NULL, NULL);
#ifdef G_ENABLE_DEBUG
  g_return_val_if_fail (g_is_ref_pointer (ref), ref);
#endif

  g_ref_count_inc (&real->ref_count);

  return ref;
}

/**
 * g_ref_pointer_release:
 * @ref: a reference counted memory area
 *
 * Releases a reference acquired using g_ref_pointer_acquire().
 *
 * If the reference count drops to zero, the notification function
 * used when allocating the memory will be called, and then the memory
 * area will be freed.
 *
 * You should only call this function if you are implementing
 * a reference counted type.
 *
 * Since: 2.44
 */
void
g_ref_pointer_release (gpointer ref)
{
  GRefPtr *real = G_REF_PTR (ref);

  g_return_if_fail (ref != NULL);
#ifdef G_ENABLE_DEBUG
  g_return_if_fail (g_is_ref_pointer (ref));
#endif

  if (g_ref_count_dec (&real->ref_count))
    g_ref_pointer_destroy (ref);
}

/**
 * g_ref_pointer_make_atomic:
 * @ref: a reference counted memory area
 *
 * Makes reference count operations on a reference counted memory area
 * always atomic.
 *
 * Since: 2.44
 */
void
g_ref_pointer_make_atomic (gpointer ref)
{
  GRefPtr *real = G_REF_PTR (ref);

  g_return_if_fail (ref != NULL);
#ifdef G_ENABLE_DEBUG
  g_return_if_fail (g_is_ref_pointer (ref));
#endif

  g_ref_count_make_atomic (&real->ref_count);
}
