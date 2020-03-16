/*
 * Copyright © 2020 Canonical Ltd.
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

#include "gstrvbuilder.h"
#include "garray.h"
#include "gmem.h"
#include "grefcount.h"
#include "gslice.h"

/**
 * SECTION:gstrvbuilder
 * @title: GStrvBuilder
 * @short_description: Object to create NULL-terminated sting arrays.
 *
 * #GStrvBuilder is a method of easily building dynamically sized
 * NULL-terminated string arrays.
 *
 * The following example shows how to build a two element array:
 *
 * |[<!-- language="C" -->
 *   g_autoptr(GStrvBuilder) builder = g_strv_builder_new ();
 *   g_strv_builder_add (builder, "hello");
 *   g_strv_builder_add (builder, "world");
 *   g_auto(GStrv) array = g_strv_builder_end (builder);
 * ]|
 */

struct _GStrvBuilder
{
  GPtrArray *array;
  gatomicrefcount ref_count;
};

/**
 * g_strv_builder_new:
 *
 * Creates a new #GStrvBuilder with a reference count of 1.
 * Use g_strv_builder_unref() on the returned value when no longer needed.
 *
 * Returns: the new #GStrvBuilder
 *
 * Since: 2.66
 */
GStrvBuilder *
g_strv_builder_new (void)
{
  GStrvBuilder *builder;

  builder = (GStrvBuilder *) g_slice_new (GStrvBuilder);
  builder->array = g_ptr_array_new_with_free_func (g_free);

  g_atomic_ref_count_init (&builder->ref_count);

  return builder;
}

/**
 * g_strv_builder_unref:
 * @builder: (transfer full): a #GStrvBuilder allocated by g_strv_builder_new()
 *
 * Decreases the reference count on @builder.
 *
 * In the event that there are no more references, releases all memory
 * associated with the #GStrvBuilder.
 *
 * Since: 2.66
 **/
void
g_strv_builder_unref (GStrvBuilder *builder)
{
  if (g_atomic_ref_count_dec (&builder->ref_count))
    {
      g_ptr_array_unref (builder->array);
      g_slice_free (GStrvBuilder, builder);
    }
}

/**
 * g_strv_builder_ref:
 * @builder: a #GStrvBuilder
 *
 * Atomically increments the reference count of @builder by one.
 * This function is thread-safe and may be called from any thread.
 *
 * Returns: The passed in #GStrvBuilder
 *
 * Since: 2.66
 */
GStrvBuilder *
g_strv_builder_ref (GStrvBuilder *builder)
{
  g_atomic_ref_count_inc (&builder->ref_count);

  return builder;
}

/**
 * g_strv_builder_add:
 * @builder: a #GStrvBuilder
 * @value: a string.
 *
 * Add a string to the end of the array.
 *
 * Since 2.66
 */
void
g_strv_builder_add (GStrvBuilder *builder,
                    const char   *value)
{
  g_ptr_array_add (builder->array, g_strdup (value));
}

/**
 * g_strv_builder_end:
 * @builder: a #GStrvBuilder
 *
 * Ends the builder process and returns the constructed NULL-terminated string
 * array. The returned value should be freed with g_strfreev() when no longer
 * needed.
 *
 * Returns: (transfer full): the constructured string array.
 *
 * Since 2.66
 */
GStrv
g_strv_builder_end (GStrvBuilder *builder)
{
  /* Add NULL terminator */
  g_ptr_array_add (builder->array, NULL);
  return (GStrv) g_ptr_array_steal (builder->array, NULL);
}
