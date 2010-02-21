/*
 * Copyright © 2007, 2008 Ryan Lortie
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include <glib/gvariant-serialiser.h>
#include <glib/gvariant-core.h>
#include <glib/gtestutils.h>
#include <glib/gstrfuncs.h>
#include <glib/ghash.h>
#include <glib/gmem.h>

#include <string.h>

#include "galias.h"

/**
 * SECTION: gvariant
 * @title: GVariant
 * @short_description: strongly typed value datatype
 * @see_also: GVariantType
 *
 * #GVariant is a variant datatype; it stores a value along with
 * information about the type of that value.  The range of possible
 * values is determined by the type.  The type system used by #GVariant
 * is #GVariantType.
 *
 * #GVariant instances always have a type and a value (which are given
 * at construction time).  The type and value of a #GVariant instance
 * can never change other than by the #GVariant itself being
 * destroyed.  A #GVariant can not contain a pointer.
 *
 * #GVariant is reference counted using g_variant_ref() and
 * g_variant_unref().  #GVariant also has floating reference counts --
 * see g_variant_ref_sink().
 *
 * #GVariant is completely threadsafe.  A #GVariant instance can be
 * concurrently accessed in any way from any number of threads without
 * problems.
 *
 * #GVariant is heavily optimised for dealing with data in serialised
 * form.  It works particularly well with data located in memory-mapped
 * files.  It can perform nearly all deserialisation operations in a
 * small constant time, usually touching only a single memory page.
 * Serialised #GVariant data can also be sent over the network.
 *
 * #GVariant is largely compatible with DBus.  Almost all types of
 * #GVariant instances can be sent over DBus.  See #GVariantType for
 * exceptions.
 *
 * For convenience to C programmers, #GVariant features powerful
 * varargs-based value construction and destruction.  This feature is
 * designed to be embedded in other libraries.
 *
 * There is a Python-inspired text language for describing #GVariant
 * values.  #GVariant includes a printer for this language and a parser
 * with type inferencing.
 *
 * <refsect2>
 *  <title>Memory Use</title>
 *  <para>
 *   #GVariant tries to be quite efficient with respect to memory use.
 *   This section gives a rough idea of how much memory is used by the
 *   current implementation.  The information here is subject to change
 *   in the future.
 *  </para>
 *  <para>
 *   The memory allocated by #GVariant can be grouped into 4 broad
 *   purposes: memory for serialised data, memory for the type
 *   information cache, buffer management memory and memory for the
 *   #GVariant structure itself.
 *  </para>
 *  <refsect3>
 *   <title>Serialised Data Memory</title>
 *   <para>
 *    This is the memory that is used for storing GVariant data in
 *    serialised form.  This is what would be sent over the network or
 *    what would end up on disk.
 *   </para>
 *   <para>
 *    The amount of memory required to store a boolean is 1 byte.  16,
 *    32 and 64 bit integers and double precision floating point numbers
 *    use their "natural" size.  Strings (including object path and
 *    signature strings) are stored with a nul terminator, and as such
 *    use the length of the string plus 1 byte.
 *   </para>
 *   <para>
 *    Maybe types use no space at all to represent the null value and
 *    use the same amount of space (sometimes plus one byte) as the
 *    equivalent non-maybe-typed value to represent the non-null case.
 *   </para>
 *   <para>
 *    Arrays use the amount of space required to store each of their
 *    members, concatenated.  Additionally, if the items stored in an
 *    array are not of a fixed-size (ie: strings, other arrays, etc)
 *    then an additional framing offset is stored for each item.  The
 *    size of this offset is either 1, 2 or 4 bytes depending on the
 *    overall size of the container.  Additionally, extra padding bytes
 *    are added as required for alignment of child values.
 *   </para>
 *   <para>
 *    Tuples (including dictionary entries) use the amount of space
 *    required to store each of their members, concatenated, plus one
 *    framing offset (as per arrays) for each non-fixed-sized item in
 *    the tuple, except for the last one.  Additionally, extra padding
 *    bytes are added as required for alignment of child values.
 *   </para>
 *   <para>
 *    Variants use the same amount of space as the item inside of the
 *    variant, plus 1 byte, plus the length of the type string for the
 *    item inside the variant.
 *   </para>
 *   <para>
 *    As an example, consider a dictionary mapping strings to variants.
 *    In the case that the dictionary is empty, 0 bytes are required for
 *    the serialisation.
 *   </para>
 *   <para>
 *    If we add an item "width" that maps to the int32 value of 500 then
 *    we will use 4 byte to store the int32 (so 6 for the variant
 *    containing it) and 6 bytes for the string.  The variant must be
 *    aligned to 8 after the 6 bytes of the string, so that's 2 extra
 *    bytes.  6 (string) + 2 (padding) + 6 (variant) is 14 bytes used
 *    for the dictionary entry.  An additional 1 byte is added to the
 *    array as a framing offset making a total of 15 bytes.
 *   </para>
 *   <para>
 *    If we add another entry, "title" that maps to a nullable string
 *    that happens to have a value of null, then we use 0 bytes for the
 *    null value (and 3 bytes for the variant to contain it along with
 *    its type string) plus 6 bytes for the string.  Again, we need 2
 *    padding bytes.  That makes a total of 6 + 2 + 3 = 11 bytes.
 *   </para>
 *   <para>
 *    We now require extra padding between the two items in the array.
 *    After the 14 bytes of the first item, that's 2 bytes required.  We
 *    now require 2 framing offsets for an extra two bytes.  14 + 2 + 11
 *    + 2 = 29 bytes to encode the entire two-item dictionary.
 *   </para>
 *  </refsect3>
 *  <refsect3>
 *   <title>Type Information Cache</title>
 *   <para>
 *    For each GVariant type that currently exists in the program a type
 *    information structure is kept in the type information cache.  The
 *    type information structure is required for rapid deserialisation.
 *   </para>
 *   <para>
 *    Continuing with the above example, if a #GVariant exists with the
 *    type "a{sv}" then a type information struct will exist for
 *    "a{sv}", "{sv}", "s", and "v".  Multiple uses of the same type
 *    will share the same type information.  Additionally, all
 *    single-digit types are stored in read-only static memory and do
 *    not contribute to the writable memory footprint of a program using
 *    #GVariant.
 *   </para>
 *   <para>
 *    Aside from the type information structures stored in read-only
 *    memory, there are two forms of type information.  One is used for
 *    container types where there is a single element type: arrays and
 *    maybe types.  The other is used for container types where there
 *    are multiple element types: tuples and dictionary entries.
 *   </para>
 *   <para>
 *    Array type info structures are 6 * sizeof (void *), plus the
 *    memory required to store the type string itself.  This means that
 *    on 32bit systems, the cache entry for "a{sv}" would require 30
 *    bytes of memory (plus malloc overhead).
 *   </para>
 *   <para>
 *    Tuple type info structures are 6 * sizeof (void *), plus 4 *
 *    sizeof (void *) for each item in the tuple, plus the memory
 *    required to store the type string itself.  A 2-item tuple, for
 *    example, would have a type information structure that consumed
 *    writable memory in the size of 14 * sizeof (void *) (plus type
 *    string)  This means that on 32bit systems, the cache entry for
 *    "{sv}" would require 61 bytes of memory (plus malloc overhead).
 *   </para>
 *   <para>
 *    This means that in total, for our "a{sv}" example, 91 bytes of
 *    type information would be allocated.
 *   </para>
 *   <para>
 *    The type information cache, additionally, uses a #GHashTable to
 *    store and lookup the cached items and stores a pointer to this
 *    hash table in static storage.  The hash table is freed when there
 *    are zero items in the type cache.
 *   </para>
 *   <para>
 *    Although these sizes may seem large it is important to remember
 *    that a program will probably only have a very small number of
 *    different types of values in it and that only one type information
 *    structure is required for many different values of the same type.
 *   </para>
 *  </refsect3>
 *  <refsect3>
 *   <title>Buffer Management Memory</title>
 *   <para>
 *    #GVariant uses an internal buffer management structure to deal
 *    with the various different possible sources of serialised data
 *    that it uses.  The buffer is responsible for ensuring that the
 *    correct call is made when the data is no longer in use by
 *    #GVariant.  This may involve a g_free() or a g_slice_free() or
 *    even g_mapped_file_unref().
 *   </para>
 *   <para>
 *    One buffer management structure is used for each chunk of
 *    serialised data.  The size of the buffer management structure is 4
 *    * (void *).  On 32bit systems, that's 16 bytes.
 *   </para>
 *  </refsect3>
 *  <refsect3>
 *   <title>GVariant structure</title>
 *   <para>
 *    The size of a #GVariant structure is 6 * (void *).  On 32 bit
 *    systems, that's 24 bytes.
 *   </para>
 *   <para>
 *    #GVariant structures only exist if they are explicitly created
 *    with API calls.  For example, if a #GVariant is constructed out of
 *    serialised data for the example given above (with the dictionary)
 *    then although there are 9 individual values that comprise the
 *    entire dictionary (two keys, two values, two variants containing
 *    the values, two dictionary entries, plus the dictionary itself),
 *    only 1 #GVariant instance exists -- the one refering to the
 *    dictionary.
 *   </para>
 *   <para>
 *    If calls are made to start accessing the other values then
 *    #GVariant instances will exist for those values only for as long
 *    as they are in use (ie: until you call g_variant_unref()).  The
 *    type information is shared.  The serialised data and the buffer
 *    management structure for that serialised data is shared by the
 *    child.
 *   </para>
 *  </refsect3>
 *  <refsect3>
 *   <title>Summary</title>
 *   <para>
 *    To put the entire example together, for our dictionary mapping
 *    strings to variants (with two entries, as given above), we are
 *    using 91 bytes of memory for type information, 29 byes of memory
 *    for the serialised data, 16 bytes for buffer management and 24
 *    bytes for the #GVariant instance, or a total of 160 bytes, plus
 *    malloc overhead.  If we were to use g_variant_get_child_value() to
 *    access the two dictionary entries, we would use an additional 48
 *    bytes.  If we were to have other dictionaries of the same type, we
 *    would use more memory for the serialised data and buffer
 *    management for those dictionaries, but the type information would
 *    be shared.
 *   </para>
 *  </refsect3>
 * </refsect2>
 */

/* definition of GVariant structure is in gvariant-core.c */

/* this is a g_return_val_if_fail() for making
 * sure a (GVariant *) has the required type.
 */
#define TYPE_CHECK(value, TYPE, val) \
  if G_UNLIKELY (!g_variant_is_of_type (value, TYPE)) {           \
    g_return_if_fail_warning (G_LOG_DOMAIN, __PRETTY_FUNCTION__,  \
                              "g_variant_is_of_type (" #value     \
                              ", " #TYPE ")");                    \
    return val;                                                   \
  }

/* < private >
 * g_variant_new_from_trusted:
 * @type: the #GVariantType
 * @data: the data to use
 * @size: the size of @data
 * @returns: a new floating #GVariant
 *
 * Constructs a new trusted #GVariant instance from the provided data.
 * This is used to implement g_variant_new_* for all the basic types.
 */
static GVariant *
g_variant_new_from_trusted (const GVariantType *type,
                            gconstpointer       data,
                            gsize               size)
{
  GVariant *value;
  GBuffer *buffer;

  buffer = g_buffer_new_from_data (data, size);
  value = g_variant_new_from_buffer (type, buffer, TRUE);
  g_buffer_unref (buffer);

  return value;
}

/**
 * g_variant_new_boolean:
 * @boolean: a #gboolean value
 * @returns: a new boolean #GVariant instance
 *
 * Creates a new boolean #GVariant instance -- either %TRUE or %FALSE.
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_boolean (gboolean value)
{
  guchar v = value;

  return g_variant_new_from_trusted (G_VARIANT_TYPE_BOOLEAN, &v, 1);
}

/**
 * g_variant_get_boolean:
 * @value: a boolean #GVariant instance
 * @returns: %TRUE or %FALSE
 *
 * Returns the boolean value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_BOOLEAN.
 *
 * Since: 2.24
 **/
gboolean
g_variant_get_boolean (GVariant *value)
{
  const guchar *data;

  TYPE_CHECK (value, G_VARIANT_TYPE_BOOLEAN, FALSE);

  data = g_variant_get_data (value);

  return data != NULL ? *data != 0 : FALSE;
}

/* the constructors and accessors for byte, int{16,32,64}, handles and
 * doubles all look pretty much exactly the same, so we reduce
 * copy/pasting here.
 */
#define NUMERIC_TYPE(TYPE, type, ctype) \
  GVariant *g_variant_new_##type (ctype value) {                \
    return g_variant_new_from_trusted (G_VARIANT_TYPE_##TYPE,   \
                                       &value, sizeof value);   \
  }                                                             \
  ctype g_variant_get_##type (GVariant *value) {                \
    const ctype *data;                                          \
    TYPE_CHECK (value, G_VARIANT_TYPE_ ## TYPE, 0);             \
    data = g_variant_get_data (value);                          \
    return data != NULL ? *data : 0;                            \
  }


/**
 * g_variant_new_byte:
 * @byte: a #guint8 value
 * @returns: a new byte #GVariant instance
 *
 * Creates a new byte #GVariant instance.
 *
 * Since: 2.24
 **/
/**
 * g_variant_get_byte:
 * @value: a byte #GVariant instance
 * @returns: a #guchar
 *
 * Returns the byte value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_BYTE.
 *
 * Since: 2.24
 **/
NUMERIC_TYPE (BYTE, byte, guchar)

/**
 * g_variant_new_int16:
 * @int16: a #gint16 value
 * @returns: a new int16 #GVariant instance
 *
 * Creates a new int16 #GVariant instance.
 *
 * Since: 2.24
 **/
/**
 * g_variant_get_int16:
 * @value: a int16 #GVariant instance
 * @returns: a #gint16
 *
 * Returns the 16-bit signed integer value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_INT16.
 *
 * Since: 2.24
 **/
NUMERIC_TYPE (INT16, int16, gint16)

/**
 * g_variant_new_uint16:
 * @uint16: a #guint16 value
 * @returns: a new uint16 #GVariant instance
 *
 * Creates a new uint16 #GVariant instance.
 *
 * Since: 2.24
 **/
/**
 * g_variant_get_uint16:
 * @value: a uint16 #GVariant instance
 * @returns: a #guint16
 *
 * Returns the 16-bit unsigned integer value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_UINT16.
 *
 * Since: 2.24
 **/
NUMERIC_TYPE (UINT16, uint16, guint16)

/**
 * g_variant_new_int32:
 * @int32: a #gint32 value
 * @returns: a new int32 #GVariant instance
 *
 * Creates a new int32 #GVariant instance.
 *
 * Since: 2.24
 **/
/**
 * g_variant_get_int32:
 * @value: a int32 #GVariant instance
 * @returns: a #gint32
 *
 * Returns the 32-bit signed integer value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_INT32.
 *
 * Since: 2.24
 **/
NUMERIC_TYPE (INT32, int32, gint32)

/**
 * g_variant_new_uint32:
 * @uint32: a #guint32 value
 * @returns: a new uint32 #GVariant instance
 *
 * Creates a new uint32 #GVariant instance.
 *
 * Since: 2.24
 **/
/**
 * g_variant_get_uint32:
 * @value: a uint32 #GVariant instance
 * @returns: a #guint32
 *
 * Returns the 32-bit unsigned integer value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_UINT32.
 *
 * Since: 2.24
 **/
NUMERIC_TYPE (UINT32, uint32, guint32)

/**
 * g_variant_new_int64:
 * @int64: a #gint64 value
 * @returns: a new int64 #GVariant instance
 *
 * Creates a new int64 #GVariant instance.
 *
 * Since: 2.24
 **/
/**
 * g_variant_get_int64:
 * @value: a int64 #GVariant instance
 * @returns: a #gint64
 *
 * Returns the 64-bit signed integer value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_INT64.
 *
 * Since: 2.24
 **/
NUMERIC_TYPE (INT64, int64, gint64)

/**
 * g_variant_new_uint64:
 * @uint64: a #guint64 value
 * @returns: a new uint64 #GVariant instance
 *
 * Creates a new uint64 #GVariant instance.
 *
 * Since: 2.24
 **/
/**
 * g_variant_get_uint64:
 * @value: a uint64 #GVariant instance
 * @returns: a #guint64
 *
 * Returns the 64-bit unsigned integer value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_UINT64.
 *
 * Since: 2.24
 **/
NUMERIC_TYPE (UINT64, uint64, guint64)

/**
 * g_variant_new_handle:
 * @handle: a #gint32 value
 * @returns: a new handle #GVariant instance
 *
 * Creates a new handle #GVariant instance.
 *
 * By convention, handles are indexes into an array of file descriptors
 * that are sent alongside a DBus message.  If you're not interacting
 * with DBus, you probably don't need them.
 *
 * Since: 2.24
 **/
/**
 * g_variant_get_handle:
 * @value: a handle #GVariant instance
 * @returns: a #gint32
 *
 * Returns the 32-bit signed integer value of @value.
 *
 * It is an error to call this function with a @value of any type other
 * than %G_VARIANT_TYPE_HANDLE.
 *
 * By convention, handles are indexes into an array of file descriptors
 * that are sent alongside a DBus message.  If you're not interacting
 * with DBus, you probably don't need them.
 *
 * Since: 2.24
 **/
NUMERIC_TYPE (HANDLE, handle, gint32)

/**
 * g_variant_new_double:
 * @floating: a #gdouble floating point value
 * @returns: a new double #GVariant instance
 *
 * Creates a new double #GVariant instance.
 *
 * Since: 2.24
 **/
/**
 * g_variant_get_double:
 * @value: a double #GVariant instance
 * @returns: a #gdouble
 *
 * Returns the double precision floating point value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_DOUBLE.
 *
 * Since: 2.24
 **/
NUMERIC_TYPE (DOUBLE, double, gdouble)

/**
 * g_variant_get_type:
 * @value: a #GVariant
 * @returns: a #GVariantType
 *
 * Determines the type of @value.
 *
 * The return value is valid for the lifetime of @value and must not
 * be freed.
 *
 * Since: 2.24
 **/
const GVariantType *
g_variant_get_type (GVariant *value)
{
  GVariantTypeInfo *type_info;

  g_return_val_if_fail (value != NULL, NULL);

  type_info = g_variant_get_type_info (value);

  return (GVariantType *) g_variant_type_info_get_type_string (type_info);
}

/**
 * g_variant_get_type_string:
 * @value: a #GVariant
 * @returns: the type string for the type of @value
 *
 * Returns the type string of @value.  Unlike the result of calling
 * g_variant_type_peek_string(), this string is nul-terminated.  This
 * string belongs to #GVariant and must not be freed.
 *
 * Since: 2.24
 **/
const gchar *
g_variant_get_type_string (GVariant *value)
{
  GVariantTypeInfo *type_info;

  g_return_val_if_fail (value != NULL, NULL);

  type_info = g_variant_get_type_info (value);

  return g_variant_type_info_get_type_string (type_info);
}

/**
 * g_variant_is_of_type:
 * @value: a #GVariant instance
 * @type: a #GVariantType
 * @returns: %TRUE if the type of @value matches @type
 *
 * Checks if a value has a type matching the provided type.
 *
 * Since: 2.24
 **/
gboolean
g_variant_is_of_type (GVariant           *value,
                      const GVariantType *type)
{
  return g_variant_type_is_subtype_of (g_variant_get_type (value), type);
}

/**
 * g_variant_is_container:
 * @value: a #GVariant instance
 * @returns: %TRUE if @value is a container
 *
 * Checks if @value is a container.
 */
gboolean
g_variant_is_container (GVariant *value)
{
  return g_variant_type_is_container (g_variant_get_type (value));
}

/**
 * g_variant_new_maybe:
 * @child_type: the #GVariantType of the child
 * @child: the child value, or %NULL
 * @returns: a new #GVariant maybe instance
 *
 * Depending on if @value is %NULL, either wraps @value inside of a
 * maybe container or creates a Nothing instance for the given @type.
 *
 * At least one of @type and @value must be non-%NULL.  If @type is
 * non-%NULL then it must be a definite type.  If they are both
 * non-%NULL then @type must be the type of @value.
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_maybe (const GVariantType *child_type,
                     GVariant           *child)
{
  GVariantType *maybe_type;
  GVariant *value;

  g_return_val_if_fail (child_type == NULL || g_variant_type_is_definite
                        (child_type), 0);
  g_return_val_if_fail (child_type != NULL || child != NULL, NULL);
  g_return_val_if_fail (child_type == NULL || child == NULL ||
                        g_variant_is_of_type (child, child_type),
                        NULL);

  if (child_type == NULL)
    child_type = g_variant_get_type (child);

  maybe_type = g_variant_type_new_maybe (child_type);

  if (child != NULL)
    {
      GVariant **children;
      gboolean trusted;

      children = g_new (GVariant *, 1);
      children[0] = g_variant_ref_sink (child);
      trusted = g_variant_is_trusted (children[0]);

      value = g_variant_new_from_children (maybe_type, children, 1, trusted);
    }
  else
    value = g_variant_new_from_children (maybe_type, NULL, 0, TRUE);

  g_variant_type_free (maybe_type);

  return value;
}

/**
 * g_variant_get_maybe:
 * @value: a maybe-typed value
 * @returns: the contents of @value, or %NULL
 *
 * Given a maybe-typed #GVariant instance, extract its value.  If the
 * value is Nothing, then this function returns %NULL.
 *
 * Since: 2.24
 **/
GVariant *
g_variant_get_maybe (GVariant *value)
{
  TYPE_CHECK (value, G_VARIANT_TYPE_MAYBE, NULL);

  if (g_variant_n_children (value))
    return g_variant_get_child_value (value, 0);

  return NULL;
}

/**
 * g_variant_new_variant:
 * @value: a #GVariance instance
 * @returns: a new variant #GVariant instance
 *
 * Boxes @value.  The result is a #GVariant instance representing a
 * variant containing the original value.
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_variant (GVariant *value)
{
  g_return_val_if_fail (value != NULL, NULL);

  return g_variant_new_from_children (G_VARIANT_TYPE_VARIANT,
                                      g_memdup (&value, sizeof value),
                                      1, g_variant_is_trusted (value));
}

/**
 * g_variant_get_variant:
 * @value: a variant #GVariance instance
 * @returns: the item contained in the variant
 *
 * Unboxes @value.  The result is the #GVariant instance that was
 * contained in @value.
 *
 * Since: 2.24
 **/
GVariant *
g_variant_get_variant (GVariant *value)
{
  TYPE_CHECK (value, G_VARIANT_TYPE_VARIANT, NULL);

  return g_variant_get_child_value (value, 0);
}

/**
 * g_variant_new_string:
 * @string: a normal C nul-terminated string
 * @returns: a new string #GVariant instance
 *
 * Creates a string #GVariant with the contents of @string.
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_string (const gchar *string)
{
  g_return_val_if_fail (string != NULL, NULL);

  return g_variant_new_from_trusted (G_VARIANT_TYPE_STRING,
                                     string, strlen (string) + 1);
}

/**
 * g_variant_new_object_path:
 * @object_path: a normal C nul-terminated string
 * @returns: a new object path #GVariant instance
 *
 * Creates a DBus object path #GVariant with the contents of @string.
 * @string must be a valid DBus object path.  Use
 * g_variant_is_object_path() if you're not sure.
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_object_path (const gchar *object_path)
{
  g_return_val_if_fail (g_variant_is_object_path (object_path), NULL);

  return g_variant_new_from_trusted (G_VARIANT_TYPE_OBJECT_PATH,
                                     object_path, strlen (object_path) + 1);
}

/**
 * g_variant_is_object_path:
 * @string: a normal C nul-terminated string
 * @returns: %TRUE if @string is a DBus object path
 *
 * Determines if a given string is a valid DBus object path.  You
 * should ensure that a string is a valid DBus object path before
 * passing it to g_variant_new_object_path().
 *
 * A valid object path starts with '/' followed by zero or more
 * sequences of characters separated by '/' characters.  Each sequence
 * must contain only the characters "[A-Z][a-z][0-9]_".  No sequence
 * (including the one following the final '/' character) may be empty.
 *
 * Since: 2.24
 **/
gboolean
g_variant_is_object_path (const gchar *string)
{
  g_return_val_if_fail (string != NULL, FALSE);

  return g_variant_serialiser_is_object_path (string, strlen (string) + 1);
}


/**
 * g_variant_new_signature:
 * @signature: a normal C nul-terminated string
 * @returns: a new signature #GVariant instance
 *
 * Creates a DBus type signature #GVariant with the contents of
 * @string.  @string must be a valid DBus type signature.  Use
 * g_variant_is_signature() if you're not sure.
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_signature (const gchar *signature)
{
  g_return_val_if_fail (g_variant_is_signature (signature), NULL);

  return g_variant_new_from_trusted (G_VARIANT_TYPE_SIGNATURE,
                                     signature, strlen (signature) + 1);
}

/**
 * g_variant_is_signature:
 * @string: a normal C nul-terminated string
 * @returns: %TRUE if @string is a DBus type signature
 *
 * Determines if a given string is a valid DBus type signature.  You
 * should ensure that a string is a valid DBus object path before
 * passing it to g_variant_new_signature().
 *
 * DBus type signatures consist of zero or more definite #GVariantType
 * strings in sequence.
 *
 * Since: 2.24
 **/
gboolean
g_variant_is_signature (const gchar *string)
{
  g_return_val_if_fail (string != NULL, FALSE);

  return g_variant_serialiser_is_signature (string, strlen (string) + 1);
}

/**
 * g_variant_get_string:
 * @value: a string #GVariant instance
 * @length: a pointer to a #gsize, to store the length
 * @returns: the constant string
 *
 * Returns the string value of a #GVariant instance with a string
 * type.  This includes the types %G_VARIANT_TYPE_STRING,
 * %G_VARIANT_TYPE_OBJECT_PATH and %G_VARIANT_TYPE_SIGNATURE.
 *
 * If @length is non-%NULL then the length of the string (in bytes) is
 * returned there.  For trusted values, this information is already
 * known.  For untrusted values, a strlen() will be performed.
 *
 * It is an error to call this function with a @value of any type
 * other than those three.
 *
 * The return value remains valid as long as @value exists.
 *
 * Since: 2.24
 **/
const gchar *
g_variant_get_string (GVariant *value,
                      gsize    *length)
{
  g_return_val_if_fail (value != NULL, NULL);
  g_return_val_if_fail (
    g_variant_is_of_type (value, G_VARIANT_TYPE_STRING) ||
    g_variant_is_of_type (value, G_VARIANT_TYPE_OBJECT_PATH) ||
    g_variant_is_of_type (value, G_VARIANT_TYPE_SIGNATURE), NULL);
  gconstpointer data = g_variant_get_data (value);
  gsize size = g_variant_get_size (value);

  if (!g_variant_is_trusted (value))
    {
      switch (g_variant_classify (value))
        {
        case G_VARIANT_CLASS_STRING:
          if (g_variant_serialiser_is_string (data, size))
            break;

          data = "";
          size = 1;
          break;

        case G_VARIANT_CLASS_OBJECT_PATH:
          if (g_variant_serialiser_is_object_path (data, size))
            break;

          data = "/";
          size = 2;
          break;

        case G_VARIANT_CLASS_SIGNATURE:
          if (g_variant_serialiser_is_signature (data, size))
            break;

          data = "";
          size = 1;
          break;

        default:
          g_assert_not_reached ();
        }
    }

  if (length)
    *length = size - 1;

  return data;
}

/**
 * g_variant_dup_string:
 * @value: a string #GVariant instance
 * @length: a pointer to a #gsize, to store the length
 * @returns: a newly allocated string
 *
 * Similar to g_variant_get_string() except that instead of returning
 * a constant string, the string is duplicated.
 *
 * The return value must be freed using g_free().
 *
 * Since: 2.24
 **/
gchar *
g_variant_dup_string (GVariant *value,
                      gsize    *length)
{
  return g_strdup (g_variant_get_string (value, length));
}

/**
 * g_variant_new_strv:
 * @strv: an array of strings
 * @length: the length of @strv, or -1
 * @returns: a new floating #GVariant instance
 *
 * Constructs an array of strings #GVariant from the given array of
 * strings.
 *
 * If @length is not -1 then it gives the maximum length of @strv.  In
 * any case, a %NULL pointer in @strv is taken as a terminator.
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_strv (const gchar * const *strv,
                    gssize               length)
{
  GVariant **strings;
  gsize i;

  g_return_val_if_fail (length == 0 || strv != NULL, NULL);

  if (length < 0)
    for (length = 0; strv[length]; length++);

  strings = g_new (GVariant *, length);
  for (i = 0; i < length; i++)
    strings[i] = g_variant_new_string (strv[i]);

  return g_variant_new_from_children (G_VARIANT_TYPE ("as"),
                                      strings, length, TRUE);
}

/**
 * g_variant_get_strv:
 * @value: an array of strings #GVariant
 * @length: the length of the result, or %NULL
 * @returns: an array of constant strings
 *
 * Gets the contents of an array of strings #GVariant.  This call
 * makes a shallow copy; the return result should be released with
 * g_free(), but the individual strings must not be modified.
 *
 * If @length is non-%NULL then the number of elements in the result
 * is stored there.  In any case, the resulting array will be
 * %NULL-terminated.
 *
 * For an empty array, @length will be set to 0 and a pointer to a
 * %NULL pointer will be returned.
 *
 * Since: 2.24
 **/
const gchar **
g_variant_get_strv (GVariant *value,
                    gsize    *length)
{
  const gchar **strv;
  gsize n;
  gsize i;

  TYPE_CHECK (value, G_VARIANT_TYPE ("as"), NULL);

  g_variant_get_data (value);
  n = g_variant_n_children (value);
  strv = g_new (const gchar *, n + 1);

  for (i = 0; i < n; i++)
    {
      GVariant *string;

      string = g_variant_get_child_value (value, i);
      strv[i] = g_variant_get_string (string, NULL);
      g_variant_unref (string);
    }
  strv[i] = NULL;

  if (length)
    *length = n;

  return strv;
}

/**
 * g_variant_dup_strv:
 * @value: an array of strings #GVariant
 * @length: the length of the result, or %NULL
 * @returns: an array of constant strings
 *
 * Gets the contents of an array of strings #GVariant.  This call
 * makes a deep copy; the return result should be released with
 * g_strfreev().
 *
 * If @length is non-%NULL then the number of elements in the result
 * is stored there.  In any case, the resulting array will be
 * %NULL-terminated.
 *
 * For an empty array, @length will be set to 0 and a pointer to a
 * %NULL pointer will be returned.
 *
 * Since: 2.24
 **/
gchar **
g_variant_dup_strv (GVariant *value,
                    gsize    *length)
{
  gchar **strv;
  gsize n;
  gsize i;

  TYPE_CHECK (value, G_VARIANT_TYPE ("as"), NULL);

  n = g_variant_n_children (value);
  strv = g_new (gchar *, n + 1);

  for (i = 0; i < n; i++)
    {
      GVariant *string;

      string = g_variant_get_child_value (value, i);
      strv[i] = g_variant_dup_string (string, NULL);
      g_variant_unref (string);
    }
  strv[i] = NULL;

  if (length)
    *length = n;

  return strv;
}

/**
 * g_variant_new_array:
 * @child_type: the element type of the new array
 * @children: an array of #GVariant pointers, the children
 * @n_children: the length of @children
 * @returns: a new #GVariant array
 *
 * Creates a new #GVariant array from @children.
 *
 * @child_type must be non-%NULL if @n_children is zero.  Otherwise, the
 * child type is determined by inspecting the first element of the
 * @children array.  If @child_type is non-%NULL then it must be a
 * definite type.
 *
 * The items of the array are taken from the @children array.  No entry
 * in the @children array may be %NULL.
 *
 * All items in the array must have the same type, which must be the
 * same as @child_type, if given.
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_array (const GVariantType *child_type,
                     GVariant * const   *children,
                     gsize               n_children)
{
  GVariantType *array_type;
  GVariant **my_children;
  gboolean trusted;
  GVariant *value;
  gsize i;

  g_return_val_if_fail (n_children > 0 || child_type != NULL, NULL);
  g_return_val_if_fail (n_children == 0 || children != NULL, NULL);
  g_return_val_if_fail (child_type == NULL ||
                        g_variant_type_is_definite (child_type), NULL);

  my_children = g_new (GVariant *, n_children);
  trusted = TRUE;

  if (child_type == NULL)
    child_type = g_variant_get_type (children[0]);
  array_type = g_variant_type_new_array (child_type);

  for (i = 0; i < n_children; i++)
    {
      TYPE_CHECK (children[i], child_type, NULL);
      my_children[i] = g_variant_ref_sink (children[i]);
      trusted &= g_variant_is_trusted (children[i]);
    }

  value = g_variant_new_from_children (array_type, my_children,
                                       n_children, trusted);
  g_variant_type_free (array_type);

  return value;
}

/**
 * g_variant_new_tuple:
 * @children: the items to make the tuple out of
 * @n_children: the length of @children
 * @returns: a new #GVariant tuple
 *
 * Creates a new tuple #GVariant out of the items in @children.  The
 * type is determined from the types of @children.  No entry in the
 * @children array may be %NULL.
 *
 * If @n_children is 0 then the unit tuple is constructed.
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_tuple (GVariant * const *children,
                     gsize             n_children)
{
  const GVariantType **types;
  GVariantType *tuple_type;
  GVariant **my_children;
  gboolean trusted;
  GVariant *value;
  gsize i;

  g_return_val_if_fail (n_children == 0 || children != NULL, NULL);

  types = g_new (const GVariantType *, n_children);
  my_children = g_new (GVariant *, n_children);
  trusted = TRUE;

  for (i = 0; i < n_children; i++)
    {
      types[i] = g_variant_get_type (children[i]);
      my_children[i] = g_variant_ref_sink (children[i]);
      trusted &= g_variant_is_trusted (children[i]);
    }

  tuple_type = g_variant_type_new_tuple (types, n_children);
  value = g_variant_new_from_children (tuple_type, my_children,
                                       n_children, trusted);
  g_variant_type_free (tuple_type);
  g_free (types);

  return value;
}

/**
 * g_variant_new_dict_entry:
 * @key: a basic #GVariant, the key
 * @value: a #GVariant, the value
 * @returns: a new dictionary entry #GVariant
 *
 * Creates a new dictionary entry #GVariant.  @key and @value must be
 * non-%NULL.
 *
 * @key must be a value of a basic type (ie: not a container).
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_dict_entry (GVariant *key,
                          GVariant *value)
{
  GVariantType *dict_type;
  GVariant **children;
  gboolean trusted;

  g_return_val_if_fail (key != NULL && value != NULL, NULL);
  g_return_val_if_fail (!g_variant_is_container (key), NULL);

  children = g_new (GVariant *, 2);
  children[0] = g_variant_ref_sink (key);
  children[1] = g_variant_ref_sink (value);
  trusted = g_variant_is_trusted (key) && g_variant_is_trusted (value);

  dict_type = g_variant_type_new_dict_entry (g_variant_get_type (key),
                                             g_variant_get_type (value));
  value = g_variant_new_from_children (dict_type, children, 2, trusted);
  g_variant_type_free (dict_type);

  return value;
}

/**
 * g_variant_get_fixed_array:
 * @value: a #GVariant array with fixed-sized elements
 * @n_elements: a pointer to the location to store the number of items
 * @element_size: the size of each element
 * @returns: a pointer to the fixed array
 *
 * Provides access to the serialised data for an array of fixed-sized
 * items.
 *
 * @value must be an array with fixed-sized elements.  Numeric types are
 * fixed-size as are tuples containing only other fixed-sized types.
 *
 * @element_size must be the size of a single element in the array.  For
 * example, if calling this function for an array of 32 bit integers,
 * you might say <code>sizeof (gint32)</code>.  This value isn't used
 * except for the purpose of a double-check that the form of the
 * seralised data matches the caller's expectation.
 *
 * @n_elements, which must be non-%NULL is set equal to the number of
 * items in the array.
 *
 * Since: 2.24
 **/
gconstpointer
g_variant_get_fixed_array (GVariant *value,
                           gsize    *n_elements,
                           gsize     element_size)
{
  GVariantTypeInfo *array_info;
  gsize array_element_size;
  gconstpointer data;
  gsize size;

  TYPE_CHECK (value, G_VARIANT_TYPE_ARRAY, NULL);

  g_return_val_if_fail (n_elements != NULL, NULL);
  g_return_val_if_fail (element_size > 0, NULL);

  array_info = g_variant_get_type_info (value);
  g_variant_type_info_query_element (array_info, NULL, &array_element_size);

  g_return_val_if_fail (array_element_size, NULL);

  if G_UNLIKELY (array_element_size != element_size)
    {
      if (array_element_size)
        g_critical ("g_variant_get_fixed_array: assertion "
                    "`g_variant_array_has_fixed_size (value, element_size)' "
                    "failed: array size %"G_GSIZE_FORMAT" does not match "
                    "given element_size %"G_GSIZE_FORMAT".",
                    array_element_size, element_size);
      else
        g_critical ("g_variant_get_fixed_array: assertion "
                    "`g_variant_array_has_fixed_size (value, element_size)' "
                    "failed: array does not have fixed size.");
    }

  data = g_variant_get_data (value);
  size = g_variant_get_size (value);

  if (size % element_size)
    *n_elements = 0;
  else
    *n_elements = size / element_size;

  if (*n_elements)
    return data;

  return NULL;
}

/**
 * g_variant_classify:
 * @value: a #GVariant
 * @returns: the #GVariantClass of @value
 *
 * Classifies @value according to its top-level type.
 *
 * Since: 2.24
 **/
/**
 * GVariantClass:
 * @G_VARIANT_CLASS_BOOLEAN: The #GVariant is a boolean.
 * @G_VARIANT_CLASS_BYTE: The #GVariant is a byte.
 * @G_VARIANT_CLASS_INT16: The #GVariant is a signed 16 bit integer.
 * @G_VARIANT_CLASS_UINT16: The #GVariant is an unsigned 16 bit integer.
 * @G_VARIANT_CLASS_INT32: The #GVariant is a signed 32 bit integer.
 * @G_VARIANT_CLASS_UINT32: The #GVariant is an unsigned 32 bit integer.
 * @G_VARIANT_CLASS_INT64: The #GVariant is a signed 64 bit integer.
 * @G_VARIANT_CLASS_UINT64: The #GVariant is an unsigned 64 bit integer.
 * @G_VARIANT_CLASS_HANDLE: The #GVariant is a file handle index.
 * @G_VARIANT_CLASS_DOUBLE: The #GVariant is a double precision floating 
 *                          point value.
 * @G_VARIANT_CLASS_STRING: The #GVariant is a normal string.
 * @G_VARIANT_CLASS_OBJECT_PATH: The #GVariant is a DBus object path 
 *                               string.
 * @G_VARIANT_CLASS_SIGNATURE: The #GVariant is a DBus signature string.
 * @G_VARIANT_CLASS_VARIANT: The #GVariant is a variant.
 * @G_VARIANT_CLASS_MAYBE: The #GVariant is a maybe-typed value.
 * @G_VARIANT_CLASS_ARRAY: The #GVariant is an array.
 * @G_VARIANT_CLASS_TUPLE: The #GVariant is a tuple.
 * @G_VARIANT_CLASS_DICT_ENTRY: The #GVariant is a dictionary entry.
 *
 * The range of possible top-level types of #GVariant instances.
 *
 * Since: 2.24
 **/
GVariantClass
g_variant_classify (GVariant *value)
{
  g_return_val_if_fail (value != NULL, 0);

  return *g_variant_get_type_string (value);
}

/**
 * g_variant_print_string:
 * @value: a #GVariant
 * @string: a #GString, or %NULL
 * @type_annotate: %TRUE if type information should be included in
 *                 the output
 * @returns: a #GString containing the string
 *
 * Behaves as g_variant_print(), but operates on a #GString.
 *
 * If @string is non-%NULL then it is appended to and returned.  Else,
 * a new empty #GString is allocated and it is returned.
 *
 * Since: 2.24
 **/
GString *
g_variant_print_string (GVariant *value,
                        GString  *string,
                        gboolean  type_annotate)
{
  if G_UNLIKELY (string == NULL)
    string = g_string_new (NULL);

  switch (g_variant_classify (value))
    {
    case G_VARIANT_CLASS_MAYBE:
      if (type_annotate)
        g_string_append_printf (string, "@%s ",
                                g_variant_get_type_string (value));

      if (g_variant_n_children (value))
        {
          gchar *printed_child;
          GVariant *element;

          /* Nested maybes:
           *
           * Consider the case of the type "mmi".  In this case we could
           * write "Just Just 4", but "4" alone is totally unambiguous,
           * so we try to drop "Just" where possible.
           *
           * We have to be careful not to always drop "Just", though,
           * since "Nothing" needs to be distinguishable from "Just
           * Nothing".  The case where we need to ensure we keep the
           * "Just" is actually exactly the case where we have a nested
           * Nothing.
           *
           * Instead of searching for that nested Nothing, we just print
           * the contained value into a separate string and see if we
           * end up with "Nothing" at the end of it.  If so, we need to
           * add "Just" at our level.
           */
          element = g_variant_get_child_value (value, 0);
          printed_child = g_variant_print (element, FALSE);
          g_variant_unref (element);

          if (g_str_has_suffix (printed_child, "Nothing"))
            g_string_append (string, "Just ");
          g_string_append (string, printed_child);
          g_free (printed_child);
        }
      else
        g_string_append (string, "Nothing");

      break;

    case G_VARIANT_CLASS_ARRAY:
      /* it's an array so the first character of the type string is 'a'
       *
       * if the first two characters are 'a{' then it's an array of
       * dictionary entries (ie: a dictionary) so we print that
       * differently.
       */
      if (g_variant_get_type_string (value)[1] == '{')
        /* dictionary */
        {
          const gchar *comma = "";
          gsize n, i;

          if ((n = g_variant_n_children (value)) == 0)
            {
              if (type_annotate)
                g_string_append_printf (string, "@%s ",
                                        g_variant_get_type_string (value));
              g_string_append (string, "{}");
              break;
            }

          g_string_append_c (string, '{');
          for (i = 0; i < n; i++)
            {
              GVariant *entry, *key, *val;

              g_string_append (string, comma);
              comma = ", ";

              entry = g_variant_get_child_value (value, i);
              key = g_variant_get_child_value (entry, 0);
              val = g_variant_get_child_value (entry, 1);
              g_variant_unref (entry);

              g_variant_print_string (key, string, type_annotate);
              g_variant_unref (key);
              g_string_append (string, ": ");
              g_variant_print_string (val, string, type_annotate);
              g_variant_unref (val);
              type_annotate = FALSE;
            }
          g_string_append_c (string, '}');
        }
      else
        /* normal (non-dictionary) array */
        {
          const gchar *comma = "";
          gsize n, i;

          if ((n = g_variant_n_children (value)) == 0)
            {
              if (type_annotate)
                g_string_append_printf (string, "@%s ",
                                        g_variant_get_type_string (value));
              g_string_append (string, "[]");
              break;
            }

          g_string_append_c (string, '[');
          for (i = 0; i < n; i++)
            {
              GVariant *element;

              g_string_append (string, comma);
              comma = ", ";

              element = g_variant_get_child_value (value, i);

              g_variant_print_string (element, string, type_annotate);
              g_variant_unref (element);
              type_annotate = FALSE;
            }
          g_string_append_c (string, ']');
        }

      break;

    case G_VARIANT_CLASS_TUPLE:
      {
        gsize n, i;

        n = g_variant_n_children (value);

        g_string_append_c (string, '(');
        for (i = 0; i < n; i++)
          {
            GVariant *element;

            element = g_variant_get_child_value (value, i);
            g_variant_print_string (element, string, type_annotate);
            g_string_append (string, ", ");
          }

        /* for >1 item:  remove final ", "
         * for 1 item:   remove final " ", but leave the ","
         * for 0 items:  there is only "(", so remove nothing
         */
        g_string_truncate (string, string->len - (n > 0) - (n > 1));
        g_string_append_c (string, ')');
      }
      break;

    case G_VARIANT_CLASS_DICT_ENTRY:
      {
        GVariant *element;

        g_string_append_c (string, '{');

        element = g_variant_get_child_value (value, 0);
        g_variant_print_string (element, string, type_annotate);
        g_variant_unref (element);

        g_string_append (string, ", ");

        element = g_variant_get_child_value (value, 1);
        g_variant_print_string (element, string, type_annotate);
        g_variant_unref (element);

        g_string_append_c (string, '}');
      }
      break;

    case G_VARIANT_CLASS_VARIANT:
      {
        GVariant *child = g_variant_get_variant (value);

        /* Always annotate types in nested variants, because they are
         * (by nature) of variable type.
         */
        g_string_append_c (string, '<');
        g_variant_print_string (child, string, TRUE);
        g_string_append_c (string, '>');

        g_variant_unref (child);
      }
      break;

    case G_VARIANT_CLASS_BOOLEAN:
      if (g_variant_get_boolean (value))
        g_string_append (string, "true");
      else
        g_string_append (string, "false");
      break;

    case G_VARIANT_CLASS_STRING:
      {
        const gchar *str = g_variant_get_string (value, NULL);
        gchar *escaped = g_strescape (str, NULL);

        g_string_append_printf (string, "\"%s\"", escaped);

        g_free (escaped);
      }
      break;

    case G_VARIANT_CLASS_BYTE:
      if (type_annotate)
        g_string_append (string, "byte ");
      g_string_append_printf (string, "0x%02x",
                              g_variant_get_byte (value));
      break;

    case G_VARIANT_CLASS_INT16:
      if (type_annotate)
        g_string_append (string, "int16 ");
      g_string_append_printf (string, "%"G_GINT16_FORMAT,
                              g_variant_get_int16 (value));
      break;

    case G_VARIANT_CLASS_UINT16:
      if (type_annotate)
        g_string_append (string, "uint16 ");
      g_string_append_printf (string, "%"G_GUINT16_FORMAT,
                              g_variant_get_uint16 (value));
      break;

    case G_VARIANT_CLASS_INT32:
      /* Never annotate this type because it is the default for numbers
       * (and this is a *pretty* printer)
       */
      g_string_append_printf (string, "%"G_GINT32_FORMAT,
                              g_variant_get_int32 (value));
      break;

    case G_VARIANT_CLASS_HANDLE:
      if (type_annotate)
        g_string_append (string, "handle ");
      g_string_append_printf (string, "%"G_GINT32_FORMAT,
                              g_variant_get_handle (value));
      break;

    case G_VARIANT_CLASS_UINT32:
      if (type_annotate)
        g_string_append (string, "uint32 ");
      g_string_append_printf (string, "%"G_GUINT32_FORMAT,
                              g_variant_get_uint32 (value));
      break;

    case G_VARIANT_CLASS_INT64:
      if (type_annotate)
        g_string_append (string, "int64 ");
      g_string_append_printf (string, "%"G_GINT64_FORMAT,
                              g_variant_get_int64 (value));
      break;

    case G_VARIANT_CLASS_UINT64:
      if (type_annotate)
        g_string_append (string, "uint64 ");
      g_string_append_printf (string, "%"G_GUINT64_FORMAT,
                              g_variant_get_uint64 (value));
      break;

    case G_VARIANT_CLASS_DOUBLE:
      {
        gchar buffer[100];
        gint i;

        g_ascii_dtostr (buffer, sizeof buffer, g_variant_get_double (value));

        for (i = 0; buffer[i]; i++)
          if (buffer[i] == '.' || buffer[i] == 'e' ||
              buffer[i] == 'n' || buffer[i] == 'N')
            break;

        /* if there is no '.' or 'e' in the float then add one */
        if (buffer[i] == '\0')
          {
            buffer[i++] = '.';
            buffer[i++] = '0';
            buffer[i++] = '\0';
          }

        g_string_append (string, buffer);
      }
      break;

    case G_VARIANT_CLASS_OBJECT_PATH:
      if (type_annotate)
        g_string_append (string, "objectpath ");
      g_string_append_printf (string, "\"%s\"",
                              g_variant_get_string (value, NULL));
      break;

    case G_VARIANT_CLASS_SIGNATURE:
      if (type_annotate)
        g_string_append (string, "signature ");
      g_string_append_printf (string, "\"%s\"",
                              g_variant_get_string (value, NULL));
      break;

    default:
      g_assert_not_reached ();
  }

  return string;
}

/**
 * g_variant_print:
 * @value: a #GVariant
 * @type_annotate: %TRUE if type information should be included in
 *                 the output
 * @returns: a newly-allocated string holding the result.
 *
 * Pretty-prints @value in the format understood by g_variant_parse().
 *
 * If @type_annotate is %TRUE, then type information is included in
 * the output.
 */
gchar *
g_variant_print (GVariant *value,
                 gboolean  type_annotate)
{
  return g_string_free (g_variant_print_string (value, NULL, type_annotate),
                        FALSE);
};

/**
 * g_variant_hash:
 * @value: a basic #GVariant value as a #gconstpointer
 * @returns: a hash value corresponding to @value
 *
 * Generates a hash value for a #GVariant instance.
 *
 * The output of this function is guaranteed to be the same for a given
 * value only per-process.  It may change between different processor
 * architectures or even different versions of GLib.  Do not use this
 * function as a basis for building protocols or file formats.
 *
 * The type of @value is #gconstpointer only to allow use of this
 * function with #GHashTable.  @value must be a #GVariant.
 *
 * Since: 2.24
 **/
guint
g_variant_hash (gconstpointer value_)
{
  GVariant *value = (GVariant *) value_;

  switch (g_variant_classify (value))
    {
    case G_VARIANT_CLASS_STRING:
    case G_VARIANT_CLASS_OBJECT_PATH:
    case G_VARIANT_CLASS_SIGNATURE:
      return g_str_hash (g_variant_get_string (value, NULL));

    case G_VARIANT_CLASS_BOOLEAN:
      /* this is a very odd thing to hash... */
      return g_variant_get_boolean (value);

    case G_VARIANT_CLASS_BYTE:
      return g_variant_get_byte (value);

    case G_VARIANT_CLASS_INT16:
    case G_VARIANT_CLASS_UINT16:
      {
        const guint16 *ptr;

        ptr = g_variant_get_data (value);

        if (ptr)
          return *ptr;
        else
          return 0;
      }

    case G_VARIANT_CLASS_INT32:
    case G_VARIANT_CLASS_UINT32:
    case G_VARIANT_CLASS_HANDLE:
      {
        const guint *ptr;

        ptr = g_variant_get_data (value);

        if (ptr)
          return *ptr;
        else
          return 0;
      }

    case G_VARIANT_CLASS_INT64:
    case G_VARIANT_CLASS_UINT64:
    case G_VARIANT_CLASS_DOUBLE:
      /* need a separate case for these guys because otherwise
       * performance could be quite bad on big endian systems
       */
      {
        const guint *ptr;

        ptr = g_variant_get_data (value);

        if (ptr)
          return ptr[0] + ptr[1];
        else
          return 0;
      }

    default:
      g_return_val_if_fail (!g_variant_is_container (value), 0);
      g_assert_not_reached ();
    }
}

/**
 * g_variant_equal:
 * @one: a #GVariant instance
 * @two: a #GVariant instance
 * @returns: %TRUE if @one and @two are equal
 *
 * Checks if @one and @two have the same type and value.
 *
 * The types of @one and @two are #gconstpointer only to allow use of
 * this function with #GHashTable.  They must each be a #GVariant.
 *
 * Since: 2.24
 **/
gboolean
g_variant_equal (gconstpointer one,
                 gconstpointer two)
{
  gboolean equal;

  g_return_val_if_fail (one != NULL && two != NULL, FALSE);

  if (g_variant_get_type_info ((GVariant *) one) !=
      g_variant_get_type_info ((GVariant *) two))
    return FALSE;

  /* if both values are trusted to be in their canonical serialised form
   * then a simple memcmp() of their serialised data will answer the
   * question.
   *
   * if not, then this might generate a false negative (since it is
   * possible for two different byte sequences to represent the same
   * value).  for now we solve this by pretty-printing both values and
   * comparing the result.
   */
  if (g_variant_is_trusted ((GVariant *) one) &&
      g_variant_is_trusted ((GVariant *) two))
    {
      gconstpointer data_one, data_two;
      gsize size_one, size_two;

      size_one = g_variant_get_size ((GVariant *) one);
      size_two = g_variant_get_size ((GVariant *) two);

      if (size_one != size_two)
        return FALSE;

      data_one = g_variant_get_data ((GVariant *) one);
      data_two = g_variant_get_data ((GVariant *) two);

      equal = memcmp (data_one, data_two, size_one) == 0;
    }
  else
    {
      gchar *strone, *strtwo;

      strone = g_variant_print ((GVariant *) one, FALSE);
      strtwo = g_variant_print ((GVariant *) two, FALSE);
      equal = strcmp (strone, strtwo) == 0;
      g_free (strone);
      g_free (strtwo);
    }

  return equal;
}

#define __G_VARIANT_C__
#include "galiasdef.c"
