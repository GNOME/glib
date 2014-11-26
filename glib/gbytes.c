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
#include <glib/gmappedfile.h>
#include <glib/gstdio.h>
#include <glib/gslice.h>
#include <glib/gtestutils.h>
#include <glib/gmem.h>
#include <glib/gmessages.h>

#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef HAVE_LINUX_MEMFD_H
#include <fcntl.h>
#include <linux/memfd.h>
#include <unistd.h>
/* FIXME: Use a similar approach to systemd/src/shared/missing.h */
#ifndef F_LINUX_SPECIFIC_BASE
#define F_LINUX_SPECIFIC_BASE 1024
#endif

#ifndef F_SETPIPE_SZ
#define F_SETPIPE_SZ (F_LINUX_SPECIFIC_BASE + 7)
#endif

#ifndef F_GETPIPE_SZ
#define F_GETPIPE_SZ (F_LINUX_SPECIFIC_BASE + 8)
#endif

#ifndef F_ADD_SEALS
#define F_ADD_SEALS (F_LINUX_SPECIFIC_BASE + 9)
#define F_GET_SEALS (F_LINUX_SPECIFIC_BASE + 10)

#define F_SEAL_SEAL     0x0001  /* prevent further seals from being set */
#define F_SEAL_SHRINK   0x0002  /* prevent file from shrinking */
#define F_SEAL_GROW     0x0004  /* prevent file from growing */
#define F_SEAL_WRITE    0x0008  /* prevent writes */
#endif

#ifndef MFD_ALLOW_SEALING
#define MFD_ALLOW_SEALING 0x0002U
#endif

#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif
/* end FIXME. */
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
  gconstpointer data;  /* may be NULL iff (size == 0) */
  gsize size;  /* may be 0 */
  gint ref_count;
  gint fd; /* -1 when data is not backed by an fd. */
  GDestroyNotify free_func;
  gpointer user_data; /* used to store a GMappedFile when backed by an fd (and
                       * only after g_bytes_get_data() has been called. */
};

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
  g_return_val_if_fail (data != NULL || size == 0, NULL);

  return g_bytes_new_take (g_memdup (data, size), size);
}

/*
 * _g_bytes_new_internal:
 * Same as g_bytes_new_with_free_func() except for the fd parameter, used by
 * g_bytes_new_take_fd().
 */
static GBytes *
_g_bytes_new_internal (gconstpointer  data,
                       gsize          size,
                       gint           fd,
                       GDestroyNotify free_func,
                       gpointer       user_data)
{
  GBytes *bytes;

  g_return_val_if_fail (data != NULL || fd != -1, NULL);

  bytes = g_slice_new (GBytes);
  bytes->data = data;
  bytes->size = size;
  bytes->fd = fd;
  bytes->free_func = free_func;
  bytes->user_data = user_data;
  bytes->ref_count = 1;

  return (GBytes *)bytes;
}

static void
unref_mapped_file (gpointer data)
{
  if (data != NULL)
    {
      g_mapped_file_unref ((GMappedFile *)data);
    }
}

/**
 * g_bytes_new_take_fd:
 * @fd: the file descriptor containg data from which to create a #GBytes
 *
 * Creates a new #GBytes from @fd.
 *
 * This call "consumes" the file descriptor, transferring ownership to the
 * returned #GBytes.
 *
 * Returns: (transfer full): a new #GBytes
 *
 * Since: 2.44
 */
GBytes *
g_bytes_new_take_fd (gint fd)
{
#ifdef HAVE_LINUX_MEMFD_H
  gint seals;
  const gint IMMUTABLE_SEALS = F_SEAL_WRITE |
                               F_SEAL_SHRINK |
                               F_SEAL_GROW |
                               F_SEAL_SEAL;
#endif /* HAVE_LINUX_MEMFD_H */
  gint error;
  struct stat sb;

#ifdef HAVE_LINUX_MEMFD_H
  /* FIXME: seal the fd if possible (only on Linux 3.17+, and if the fd was
   * created with memfd_create()). */
  /* FIXME: Check if the provided fd is one of the standard ones, and warn? */
  seals = fcntl (fd, F_GET_SEALS);
  if (seals == -1)
    {
      g_debug ("Retrieving fd seals failed: %s", g_strerror (errno));
    }

  /* Seal the fd, if it is not already. */
  if ((seals & (IMMUTABLE_SEALS)) >= IMMUTABLE_SEALS)
    {
      g_debug ("%s", "fd already sealed");
    }
  else
    {
      error = fcntl (fd, F_ADD_SEALS, IMMUTABLE_SEALS);
      if (error == -1)
        {
          g_debug ("fd sealing failed: %s", g_strerror (errno));
          /* FIXME: use shm_open() on non-Linux, or when memfd_create() is
           * otherwise unavailable? */
        }
    }
#endif /* HAVE_LINUX_MEMFD_H */

  error = fstat (fd, &sb);
  if (error == -1)
    {
      g_debug ("fstat() on fd failed: %s", g_strerror (errno));
    }

  return _g_bytes_new_internal (NULL, sb.st_size, fd, NULL, unref_mapped_file);
}

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
  return g_bytes_new_with_free_func (data, size, g_free, data);
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
  return g_bytes_new_with_free_func (data, size, NULL, NULL);
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
  g_return_val_if_fail (data != NULL || size == 0, NULL);

  return _g_bytes_new_internal (data, size, -1, free_func, user_data);
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

  return g_bytes_new_with_free_func ((gchar *)bytes->data + offset, length,
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
                  gsize *size)
{
  GError *error = NULL;

  g_return_val_if_fail (bytes != NULL, NULL);

  if (size)
    *size = bytes->size;

  if (bytes->fd > -1)
    {
      if (bytes->user_data == NULL)
        {
          bytes->user_data = g_mapped_file_new_from_fd (bytes->fd, FALSE,
                                                        &error);
          if (bytes->user_data == NULL)
            {
              g_debug ("Failed to map fd: %s", error->message);
              *size = 0;
              return NULL;
            }

          *size = g_mapped_file_get_length (bytes->user_data);
          if (*size != bytes->size)
            {
              g_debug ("Mapped size differs from that returned by fstat(): %"
                       G_GSIZE_FORMAT " versus %" G_GOFFSET_FORMAT, *size,
                       bytes->size);
            }

          bytes->data = g_mapped_file_get_contents (bytes->user_data);
        }
    }

  return bytes->data;
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
      if (bytes->free_func != NULL)
        bytes->free_func (bytes->user_data);
      g_slice_free (GBytes, bytes);
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
  const GBytes *b1 = bytes1;
  const GBytes *b2 = bytes2;

  g_return_val_if_fail (bytes1 != NULL, FALSE);
  g_return_val_if_fail (bytes2 != NULL, FALSE);

  return b1->size == b2->size &&
         memcmp (b1->data, b2->data, b1->size) == 0;
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
  const GBytes *a = bytes;
  const signed char *p, *e;
  guint32 h = 5381;

  g_return_val_if_fail (bytes != NULL, 0);

  for (p = (signed char *)a->data, e = (signed char *)a->data + a->size; p != e; p++)
    h = (h << 5) + h + *p;

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
  const GBytes *b1 = bytes1;
  const GBytes *b2 = bytes2;
  gint ret;

  g_return_val_if_fail (bytes1 != NULL, 0);
  g_return_val_if_fail (bytes2 != NULL, 0);

  ret = memcmp (b1->data, b2->data, MIN (b1->size, b2->size));
  if (ret == 0 && b1->size != b2->size)
      ret = b1->size < b2->size ? -1 : 1;
  return ret;
}

static gpointer
try_steal_and_unref (GBytes         *bytes,
                     GDestroyNotify  free_func,
                     gsize          *size)
{
  gpointer result;

  if (bytes->free_func != free_func || bytes->data == NULL)
    return NULL;

  /* Are we the only reference? */
  if (g_atomic_int_get (&bytes->ref_count) == 1)
    {
      *size = bytes->size;
      result = (gpointer)bytes->data;
      g_slice_free (GBytes, bytes);
      return result;
    }

  return NULL;
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

  result = try_steal_and_unref (bytes, g_free, size);
  if (result == NULL)
    {
      /*
       * Copy: Non g_malloc (or compatible) allocator, or static memory,
       * so we have to copy, and then unref.
       */
      if (!bytes->data && bytes->fd > -1)
        {
          /* Map the fd if the data is to be copied. */
          g_bytes_get_data (bytes, NULL);
        }
      result = g_memdup (bytes->data, bytes->size);
      *size = bytes->size;
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
