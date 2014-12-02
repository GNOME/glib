/*
 * Copyright © 2009, 2010 Codethink Limited
 * Copyright © 2011 Collabora Ltd.
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
 * Author: Ryan Lortie <desrt@desrt.ca>
 *         Stef Walter <stefw@collabora.co.uk>
 */

#include "config.h"

#include "gbytes.h"

#include <glib/garray.h>
#include <glib/gstrfuncs.h>
#include <glib/gatomic.h>
#include <glib/gslice.h>
#include <glib/gtestutils.h>
#include <glib/gmem.h>
#include <glib/gmessages.h>

#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef G_OS_UNIX
#include "glib-unix.h"
#include <sys/mman.h>
#endif

/**
 * GBytes:
 *
 * A simple refcounted data type representing an immutable sequence of zero or
 * more bytes from an unspecified origin.
 *
 * The purpose of a #GBytes is to keep the memory region that it holds
 * alive for as long as anyone holds a reference to the bytes.  When
 * the last reference count is dropped, the memory is released. Multiple
 * unrelated callers can use byte data in the #GBytes without coordinating
 * their activities, resting assured that the byte data will not change or
 * move while they hold a reference.
 *
 * A #GBytes can come from many different origins that may have
 * different procedures for freeing the memory region.  Examples are
 * memory from g_malloc(), from memory slices, from a #GMappedFile or
 * memory from other allocators.
 *
 * #GBytes work well as keys in #GHashTable. Use g_bytes_equal() and
 * g_bytes_hash() as parameters to g_hash_table_new() or g_hash_table_new_full().
 * #GBytes can also be used as keys in a #GTree by passing the g_bytes_compare()
 * function to g_tree_new().
 *
 * The data pointed to by this bytes must not be modified. For a mutable
 * array of bytes see #GByteArray. Use g_bytes_unref_to_array() to create a
 * mutable array for a #GBytes sequence. To create an immutable #GBytes from
 * a mutable #GByteArray, use the g_byte_array_free_to_bytes() function.
 *
 * Since: 2.32
 **/

struct _GBytes
{
  gsize size;
  gint  ref_count;
  gint  type_or_fd;
};

typedef struct
{
  GBytes bytes;
#if GLIB_SIZEOF_SIZE_T == 4
  guint pad;
#endif

  guchar data[1];
} GBytesInline;

/* important: the ->data field of GBytesInline should always be 'nicely
 * aligned'.
 */
G_STATIC_ASSERT (G_STRUCT_OFFSET (GBytesInline, data) % (2 * sizeof (gpointer)) == 0);
G_STATIC_ASSERT (G_STRUCT_OFFSET (GBytesInline, data) % 8 == 0);


typedef struct
{
  GBytes   bytes;

  gpointer data;
} GBytesData;

typedef struct
{
  GBytesData     data_bytes;

  GDestroyNotify notify;
  gpointer       user_data;
} GBytesNotify;

#define G_BYTES_TYPE_INLINE        (-1)
#define G_BYTES_TYPE_STATIC        (-2)
#define G_BYTES_TYPE_FREE          (-3)
#define G_BYTES_TYPE_NOTIFY        (-4)

/* All bytes are either inline or subtypes of GBytesData */
#define G_BYTES_IS_INLINE(bytes)   ((bytes)->type_or_fd == G_BYTES_TYPE_INLINE)
#define G_BYTES_IS_DATA(bytes)     (!G_BYTES_IS_INLINE(bytes))

/* More specific subtypes of GBytesData */
#define G_BYTES_IS_STATIC(bytes)   ((bytes)->type_or_fd == G_BYTES_TYPE_STATIC)
#define G_BYTES_IS_FREE(bytes)     ((bytes)->type_or_fd == G_BYTES_TYPE_FREE)
#define G_BYTES_IS_NOTIFY(bytes)   ((bytes)->type_or_fd == G_BYTES_TYPE_NOTIFY)

/* we have a memfd if type_or_fd >= 0 */
#define G_BYTES_IS_MEMFD(bytes)    ((bytes)->type_or_fd >= 0)

static gpointer
g_bytes_allocate (guint struct_size,
                  guint type_or_fd,
                  gsize data_size)
{
  GBytes *bytes;

  bytes = g_slice_alloc (struct_size);
  bytes->size = data_size;
  bytes->ref_count = 1;
  bytes->type_or_fd = type_or_fd;

  return bytes;
}

/**
 * g_bytes_new:
 * @data: (transfer none) (array length=size) (element-type guint8) (allow-none):
 *        the data to be used for the bytes
 * @size: the size of @data
 *
 * Creates a new #GBytes from @data.
 *
 * @data is copied. If @size is 0, @data may be %NULL.
 *
 * Returns: (transfer full): a new #GBytes
 *
 * Since: 2.32
 */
GBytes *
g_bytes_new (gconstpointer data,
             gsize         size)
{
  GBytesInline *bytes;

  g_return_val_if_fail (data != NULL || size == 0, NULL);

  bytes = g_bytes_allocate (G_STRUCT_OFFSET (GBytesInline, data[size]), G_BYTES_TYPE_INLINE, size);
  memcpy (bytes->data, data, size);

  return (GBytes *) bytes;
}

/**
 * g_bytes_new_take_zero_copy_fd:
 * @fd: a file descriptor capable of being zero-copy-safe
 *
 * Creates a new #GBytes from @fd.
 *
 * @fd must be capable of being made zero-copy-safe.  In concrete terms,
 * this means that a call to g_unix_fd_ensure_zero_copy_safe() on @fd
 * will succeed.  This call will be made before returning.
 *
 * This call consumes @fd, transferring ownership to the returned
 * #GBytes.
 *
 * Returns: (transfer full): a new #GBytes
 *
 * Since: 2.44
 */
#ifdef G_OS_UNIX
GBytes *
g_bytes_new_take_zero_copy_fd (gint fd)
{
  GBytesData *bytes;
  struct stat buf;

  g_return_val_if_fail_se (g_unix_fd_ensure_zero_copy_safe (fd), NULL);

  /* We already checked this is a memfd... */
  g_assert_se (fstat (fd, &buf) == 0);

  bytes = g_bytes_allocate (sizeof (GBytesData), fd, buf.st_size);
  bytes->data = mmap (NULL, buf.st_size, PROT_READ, MAP_SHARED, fd, 0);
  if (bytes->data == MAP_FAILED)
    /* this is similar to malloc() failing, so do the same... */
    g_error ("mmap() on memfd failed: %s\n", g_strerror (errno));

  return (GBytes *) bytes;
}
#endif /* G_OS_UNIX */

/**
 * g_bytes_new_take:
 * @data: (transfer full) (array length=size) (element-type guint8) (allow-none):
          the data to be used for the bytes
 * @size: the size of @data
 *
 * Creates a new #GBytes from @data.
 *
 * After this call, @data belongs to the bytes and may no longer be
 * modified by the caller.  g_free() will be called on @data when the
 * bytes is no longer in use. Because of this @data must have been created by
 * a call to g_malloc(), g_malloc0() or g_realloc() or by one of the many
 * functions that wrap these calls (such as g_new(), g_strdup(), etc).
 *
 * For creating #GBytes with memory from other allocators, see
 * g_bytes_new_with_free_func().
 *
 * @data may be %NULL if @size is 0.
 *
 * Returns: (transfer full): a new #GBytes
 *
 * Since: 2.32
 */
GBytes *
g_bytes_new_take (gpointer data,
                  gsize    size)
{
  GBytesData *bytes;

  bytes = g_bytes_allocate (sizeof (GBytesNotify), G_BYTES_TYPE_FREE, size);
  bytes->data = data;

  return (GBytes *) bytes;
}

/**
 * g_bytes_new_static: (skip)
 * @data: (transfer full) (array length=size) (element-type guint8) (allow-none):
          the data to be used for the bytes
 * @size: the size of @data
 *
 * Creates a new #GBytes from static data.
 *
 * @data must be static (ie: never modified or freed). It may be %NULL if @size
 * is 0.
 *
 * Returns: (transfer full): a new #GBytes
 *
 * Since: 2.32
 */
GBytes *
g_bytes_new_static (gconstpointer data,
                    gsize         size)
{
  GBytesData *bytes;

  g_return_val_if_fail (data != NULL || size == 0, NULL);

  bytes = g_bytes_allocate (sizeof (GBytesData), G_BYTES_TYPE_STATIC, size);
  bytes->data = (gpointer) data;

  return (GBytes *) bytes;
}

/**
 * g_bytes_new_with_free_func:
 * @data: (array length=size) (allow-none): the data to be used for the bytes
 * @size: the size of @data
 * @free_func: the function to call to release the data
 * @user_data: data to pass to @free_func
 *
 * Creates a #GBytes from @data.
 *
 * When the last reference is dropped, @free_func will be called with the
 * @user_data argument.
 *
 * @data must not be modified after this call is made until @free_func has
 * been called to indicate that the bytes is no longer in use.
 *
 * @data may be %NULL if @size is 0.
 *
 * Returns: (transfer full): a new #GBytes
 *
 * Since: 2.32
 */
GBytes *
g_bytes_new_with_free_func (gconstpointer  data,
                            gsize          size,
                            GDestroyNotify free_func,
                            gpointer       user_data)
{
  GBytesNotify *bytes;

  if (!free_func)
    return g_bytes_new_static (data, size);

  bytes = g_bytes_allocate (sizeof (GBytesNotify), G_BYTES_TYPE_NOTIFY, size);
  bytes->data_bytes.data = (gpointer) data;
  bytes->notify = free_func;
  bytes->user_data = user_data;

  return (GBytes *) bytes;
}

/**
 * g_bytes_new_from_bytes:
 * @bytes: a #GBytes
 * @offset: offset which subsection starts at
 * @length: length of subsection
 *
 * Creates a #GBytes which is a subsection of another #GBytes. The @offset +
 * @length may not be longer than the size of @bytes.
 *
 * A reference to @bytes will be held by the newly created #GBytes until
 * the byte data is no longer needed.
 *
 * Returns: (transfer full): a new #GBytes
 *
 * Since: 2.32
 */
GBytes *
g_bytes_new_from_bytes (GBytes  *bytes,
                        gsize    offset,
                        gsize    length)
{
  /* Note that length may be 0. */
  g_return_val_if_fail (bytes != NULL, NULL);
  g_return_val_if_fail (offset <= bytes->size, NULL);
  g_return_val_if_fail (offset + length <= bytes->size, NULL);

  return g_bytes_new_with_free_func ((gchar *) g_bytes_get_data (bytes, NULL) + offset, length,
                                     (GDestroyNotify)g_bytes_unref, g_bytes_ref (bytes));
}

/**
 * g_bytes_get_data:
 * @bytes: a #GBytes
 * @size: (out) (allow-none): location to return size of byte data
 *
 * Get the byte data in the #GBytes. This data should not be modified.
 *
 * This function will always return the same pointer for a given #GBytes.
 *
 * %NULL may be returned if @size is 0. This is not guaranteed, as the #GBytes
 * may represent an empty string with @data non-%NULL and @size as 0. %NULL will
 * not be returned if @size is non-zero.
 *
 * Returns: (transfer none) (array length=size) (type guint8) (allow-none): a pointer to the
 *          byte data, or %NULL
 *
 * Since: 2.32
 */
gconstpointer
g_bytes_get_data (GBytes *bytes,
                  gsize  *size)
{
  g_return_val_if_fail (bytes != NULL, NULL);

  if (size)
    *size = bytes->size;

  if (G_BYTES_IS_DATA (bytes))
    {
      GBytesData *data_bytes = (GBytesData *) bytes;

      return data_bytes->data;
    }
  else if (G_BYTES_IS_INLINE (bytes))
    {
      GBytesInline *inline_bytes = (GBytesInline *) bytes;

      return inline_bytes->data;
    }
  else
    g_assert_not_reached ();
}

/**
 * g_bytes_get_size:
 * @bytes: a #GBytes
 *
 * Get the size of the byte data in the #GBytes.
 *
 * This function will always return the same value for a given #GBytes.
 *
 * Returns: the size
 *
 * Since: 2.32
 */
gsize
g_bytes_get_size (GBytes *bytes)
{
  g_return_val_if_fail (bytes != NULL, 0);
  return bytes->size;
}

/**
 * g_bytes_get_zero_copy_fd:
 * @bytes: a #GBytes
 *
 * Gets the zero-copy fd from a #GBytes, if it has one.
 *
 * Returns -1 if @bytes was not created from a zero-copy fd.
 *
 * A #GBytes created with a zero-copy fd may have been internally
 * converted into another type of #GBytes for any reason at all.  This
 * function may therefore return -1 at any time, even for a #GBytes that
 * was created with g_bytes_new_take_zero_copy_fd().
 *
 * The returned file descriptor belongs to @bytes.  Do not close it.
 *
 * Returns: a file descriptor, or -1
 *
 * Since: 2.44
 */
gint
g_bytes_get_zero_copy_fd (GBytes *bytes)
{
  g_return_val_if_fail (bytes != NULL, -1);

  if (G_BYTES_IS_MEMFD (bytes))
    return bytes->type_or_fd;
  else
    return -1;
}

/**
 * g_bytes_ref:
 * @bytes: a #GBytes
 *
 * Increase the reference count on @bytes.
 *
 * Returns: the #GBytes
 *
 * Since: 2.32
 */
GBytes *
g_bytes_ref (GBytes *bytes)
{
  g_return_val_if_fail (bytes != NULL, NULL);

  g_atomic_int_inc (&bytes->ref_count);

  return bytes;
}

/**
 * g_bytes_unref:
 * @bytes: (allow-none): a #GBytes
 *
 * Releases a reference on @bytes.  This may result in the bytes being
 * freed.
 *
 * Since: 2.32
 */
void
g_bytes_unref (GBytes *bytes)
{
  if (bytes == NULL)
    return;

  if (g_atomic_int_dec_and_test (&bytes->ref_count))
    {
      switch (bytes->type_or_fd)
        {
        case G_BYTES_TYPE_STATIC:
          /* data does not need to be freed */
          g_slice_free (GBytesData, (GBytesData *) bytes);
          break;

        case G_BYTES_TYPE_INLINE:
          /* data will be freed along with struct */
          g_slice_free1 (G_STRUCT_OFFSET (GBytesInline, data[bytes->size]), bytes);
          break;

        case G_BYTES_TYPE_FREE:
          {
            GBytesData *data_bytes = (GBytesData *) bytes;

            g_free (data_bytes->data);

            g_slice_free (GBytesData, data_bytes);
            break;
          }

        case G_BYTES_TYPE_NOTIFY:
          {
            GBytesNotify *notify_bytes = (GBytesNotify *) bytes;

            /* We don't create GBytesNotify if callback was NULL */
            (* notify_bytes->notify) (notify_bytes->user_data);

            g_slice_free (GBytesNotify, notify_bytes);
            break;
          }

        default:
          {
            GBytesData *data_bytes = (GBytesData *) bytes;

            g_assert (bytes->type_or_fd >= 0);

            g_assert_se (munmap (data_bytes->data, bytes->size) == 0);
            g_assert_se (close (bytes->type_or_fd) == 0);

            g_slice_free (GBytesData, data_bytes);
            break;
          }
        }
    }
}

/**
 * g_bytes_equal:
 * @bytes1: (type GLib.Bytes): a pointer to a #GBytes
 * @bytes2: (type GLib.Bytes): a pointer to a #GBytes to compare with @bytes1
 *
 * Compares the two #GBytes values being pointed to and returns
 * %TRUE if they are equal.
 *
 * This function can be passed to g_hash_table_new() as the @key_equal_func
 * parameter, when using non-%NULL #GBytes pointers as keys in a #GHashTable.
 *
 * Returns: %TRUE if the two keys match.
 *
 * Since: 2.32
 */
gboolean
g_bytes_equal (gconstpointer bytes1,
               gconstpointer bytes2)
{
  gconstpointer d1, d2;
  gsize s1, s2;

  g_return_val_if_fail (bytes1 != NULL, FALSE);
  g_return_val_if_fail (bytes2 != NULL, FALSE);

  d1 = g_bytes_get_data ((GBytes *) bytes1, &s1);
  d2 = g_bytes_get_data ((GBytes *) bytes2, &s2);

  if (s1 != s2)
    return FALSE;

  if (d1 == d2)
    return TRUE;

  return memcmp (d1, d2, s1) == 0;
}

/**
 * g_bytes_hash:
 * @bytes: (type GLib.Bytes): a pointer to a #GBytes key
 *
 * Creates an integer hash code for the byte data in the #GBytes.
 *
 * This function can be passed to g_hash_table_new() as the @key_hash_func
 * parameter, when using non-%NULL #GBytes pointers as keys in a #GHashTable.
 *
 * Returns: a hash value corresponding to the key.
 *
 * Since: 2.32
 */
guint
g_bytes_hash (gconstpointer bytes)
{
  const guchar *data;
  const guchar *end;
  gsize size;
  guint32 h = 5381;

  g_return_val_if_fail (bytes != NULL, 0);

  data = g_bytes_get_data ((GBytes *) bytes, &size);
  end = data + size;

  while (data != end)
    h = (h << 5) + h + *(data++);

  return h;
}

/**
 * g_bytes_compare:
 * @bytes1: (type GLib.Bytes): a pointer to a #GBytes
 * @bytes2: (type GLib.Bytes): a pointer to a #GBytes to compare with @bytes1
 *
 * Compares the two #GBytes values.
 *
 * This function can be used to sort GBytes instances in lexographical order.
 *
 * Returns: a negative value if bytes2 is lesser, a positive value if bytes2 is
 *          greater, and zero if bytes2 is equal to bytes1
 *
 * Since: 2.32
 */
gint
g_bytes_compare (gconstpointer bytes1,
                 gconstpointer bytes2)
{
  gconstpointer d1, d2;
  gsize s1, s2;
  gint ret;

  g_return_val_if_fail (bytes1 != NULL, 0);
  g_return_val_if_fail (bytes2 != NULL, 0);

  d1 = g_bytes_get_data ((GBytes *) bytes1, &s1);
  d2 = g_bytes_get_data ((GBytes *) bytes2, &s2);

  ret = memcmp (d1, d2, MIN (s1, s2));
  if (ret == 0 && s1 != s2)
    ret = s1 < s2 ? -1 : 1;

  return ret;
}

/**
 * g_bytes_unref_to_data:
 * @bytes: (transfer full): a #GBytes
 * @size: location to place the length of the returned data
 *
 * Unreferences the bytes, and returns a pointer the same byte data
 * contents.
 *
 * As an optimization, the byte data is returned without copying if this was
 * the last reference to bytes and bytes was created with g_bytes_new(),
 * g_bytes_new_take() or g_byte_array_free_to_bytes(). In all other cases the
 * data is copied.
 *
 * Returns: (transfer full): a pointer to the same byte data, which should
 *          be freed with g_free()
 *
 * Since: 2.32
 */
gpointer
g_bytes_unref_to_data (GBytes *bytes,
                       gsize  *size)
{
  gpointer result;

  g_return_val_if_fail (bytes != NULL, NULL);
  g_return_val_if_fail (size != NULL, NULL);


  /*
   * Optimal path: if this is was the last reference, then we can return
   * the data from this GBytes without copying.
   */
  if (G_BYTES_IS_FREE(bytes) && g_atomic_int_get (&bytes->ref_count) == 1)
    {
      GBytesData *data_bytes = (GBytesData *) bytes;

      result = data_bytes->data;
      *size = bytes->size;

      g_slice_free (GBytesData, data_bytes);
    }
  else
    {
      gconstpointer data;

      data = g_bytes_get_data (bytes, size);
      result = g_memdup (data, *size);
      g_bytes_unref (bytes);
    }

  return result;
}

/**
 * g_bytes_unref_to_array:
 * @bytes: (transfer full): a #GBytes
 *
 * Unreferences the bytes, and returns a new mutable #GByteArray containing
 * the same byte data.
 *
 * As an optimization, the byte data is transferred to the array without copying
 * if this was the last reference to bytes and bytes was created with
 * g_bytes_new(), g_bytes_new_take() or g_byte_array_free_to_bytes(). In all
 * other cases the data is copied.
 *
 * Returns: (transfer full): a new mutable #GByteArray containing the same byte data
 *
 * Since: 2.32
 */
GByteArray *
g_bytes_unref_to_array (GBytes *bytes)
{
  gpointer data;
  gsize size;

  g_return_val_if_fail (bytes != NULL, NULL);

  data = g_bytes_unref_to_data (bytes, &size);
  return g_byte_array_new_take (data, size);
}
