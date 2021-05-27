/* GLIB sliced memory - fast concurrent memory chunk allocator
 * Copyright (C) 2005 Tim Janik
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
/* MT safe */

#include "config.h"
#include "glibconfig.h"

#include <stdio.h>
#include <string.h>

#include "gslice.h"

#include "gmem.h"               /* gslice.h */
#include "glib_trace.h"
#include "gprintf.h"

/**
 * SECTION:memory_slices
 * @title: Memory Slices
 * @short_description: efficient way to allocate groups of equal-sized
 *     chunks of memory
 *
 * GSlice was a space-efficient and multi-processing scalable way to allocate
 * equal sized pieces of memory. Since GLib 2.76, its implementation has been
 * removed and it calls g_malloc() and g_free_sized(), because the performance
 * of the system-default allocators has improved on all platforms since GSlice
 * was written.
 *
 * The GSlice APIs have not been deprecated, as they are widely in use and doing
 * so would be very disruptive for little benefit.
 *
 * New code should be written using g_new()/g_malloc() and g_free_sized() or
 * g_free(). There is no particular benefit in porting existing code away from
 * g_slice_new()/g_slice_free() unless it’s being rewritten anyway.
 *
 * Here is an example for using the slice allocator:
 * |[<!-- language="C" --> 
 * gchar *mem[10000];
 * gint i;
 *
 * // Allocate 10000 blocks.
 * for (i = 0; i < 10000; i++)
 *   {
 *     mem[i] = g_slice_alloc (50);
 *
 *     // Fill in the memory with some junk.
 *     for (j = 0; j < 50; j++)
 *       mem[i][j] = i * j;
 *   }
 *
 * // Now free all of the blocks.
 * for (i = 0; i < 10000; i++)
 *   g_slice_free1 (50, mem[i]);
 * ]|
 *
 * And here is an example for using the using the slice allocator
 * with data structures:
 * |[<!-- language="C" --> 
 * GRealArray *array;
 *
 * // Allocate one block, using the g_slice_new() macro.
 * array = g_slice_new (GRealArray);
 *
 * // We can now use array just like a normal pointer to a structure.
 * array->data            = NULL;
 * array->len             = 0;
 * array->alloc           = 0;
 * array->zero_terminated = (zero_terminated ? 1 : 0);
 * array->clear           = (clear ? 1 : 0);
 * array->elt_size        = elt_size;
 *
 * // We can free the block, so it can be reused.
 * g_slice_free (GRealArray, array);
 * ]|
 */

/* --- auxiliary functions --- */
void
g_slice_set_config (GSliceConfig ckey,
                    gint64       value)
{
  /* deprecated, no implementation */
}

gint64
g_slice_get_config (GSliceConfig ckey)
{
  /* deprecated, no implementation */
  return 0;
}

gint64*
g_slice_get_config_state (GSliceConfig ckey,
                          gint64       address,
                          guint       *n_values)
{
  /* deprecated, no implementation */
  return NULL;
}

/* --- API functions --- */

/**
 * g_slice_new:
 * @type: the type to allocate, typically a structure name
 *
 * A convenience macro to allocate a block of memory from the
 * slice allocator.
 *
 * It calls g_slice_alloc() with `sizeof (@type)` and casts the
 * returned pointer to a pointer of the given type, avoiding a type
 * cast in the source code.
 *
 * This can never return %NULL as the minimum allocation size from
 * `sizeof (@type)` is 1 byte.
 *
 * Since GLib 2.76 this always uses the system malloc() implementation
 * internally.
 *
 * Returns: (not nullable): a pointer to the allocated block, cast to a pointer
 *    to @type
 *
 * Since: 2.10
 */

/**
 * g_slice_new0:
 * @type: the type to allocate, typically a structure name
 *
 * A convenience macro to allocate a block of memory from the
 * slice allocator and set the memory to 0.
 *
 * It calls g_slice_alloc0() with `sizeof (@type)`
 * and casts the returned pointer to a pointer of the given type,
 * avoiding a type cast in the source code.
 *
 * This can never return %NULL as the minimum allocation size from
 * `sizeof (@type)` is 1 byte.
 *
 * Since GLib 2.76 this always uses the system malloc() implementation
 * internally.
 *
 * Returns: (not nullable): a pointer to the allocated block, cast to a pointer
 *    to @type
 *
 * Since: 2.10
 */

/**
 * g_slice_dup:
 * @type: the type to duplicate, typically a structure name
 * @mem: (not nullable): the memory to copy into the allocated block
 *
 * A convenience macro to duplicate a block of memory using
 * the slice allocator.
 *
 * It calls g_slice_copy() with `sizeof (@type)`
 * and casts the returned pointer to a pointer of the given type,
 * avoiding a type cast in the source code.
 *
 * This can never return %NULL.
 *
 * Since GLib 2.76 this always uses the system malloc() implementation
 * internally.
 *
 * Returns: (not nullable): a pointer to the allocated block, cast to a pointer
 *    to @type
 *
 * Since: 2.14
 */

/**
 * g_slice_free:
 * @type: the type of the block to free, typically a structure name
 * @mem: (nullable): a pointer to the block to free
 *
 * A convenience macro to free a block of memory that has
 * been allocated from the slice allocator.
 *
 * It calls g_slice_free1() using `sizeof (type)`
 * as the block size.
 * Note that the exact release behaviour can be changed with the
 * [`G_DEBUG=gc-friendly`][G_DEBUG] environment variable.
 *
 * If @mem is %NULL, this macro does nothing.
 *
 * Since GLib 2.76 this always uses the system free() implementation internally.
 *
 * Since: 2.10
 */

/**
 * g_slice_free_chain:
 * @type: the type of the @mem_chain blocks
 * @mem_chain: (nullable): a pointer to the first block of the chain
 * @next: the field name of the next pointer in @type
 *
 * Frees a linked list of memory blocks of structure type @type.
 *
 * The memory blocks must be equal-sized, allocated via
 * g_slice_alloc() or g_slice_alloc0() and linked together by
 * a @next pointer (similar to #GSList). The name of the
 * @next field in @type is passed as third argument.
 * Note that the exact release behaviour can be changed with the
 * [`G_DEBUG=gc-friendly`][G_DEBUG] environment variable.
 *
 * If @mem_chain is %NULL, this function does nothing.
 *
 * Since GLib 2.76 this always uses the system free() implementation internally.
 *
 * Since: 2.10
 */
#include <gatomic.h>
G_LOCK_DEFINE_STATIC (metrics);
static GMetricsTable *metrics_table;
static GMetricsInstanceCounter *instance_counter = NULL;
static GMetricsInstanceCounter *stack_trace_counter = NULL;
static GMetricsStackTraceSampler *stack_trace_sampler = NULL;
static gsize g_slice_allocated_memory = 0;

/**
 * g_slice_alloc:
 * @block_size: the number of bytes to allocate
 *
 * Allocates a block of memory from the libc allocator.
 *
 * The block address handed out can be expected to be aligned
 * to at least `1 * sizeof (void*)`.
 *
 * Since GLib 2.76 this always uses the system malloc() implementation
 * internally.
 *
 * Returns: (nullable): a pointer to the allocated memory block, which will
 *   be %NULL if and only if @mem_size is 0
 *
 * Since: 2.10
 */
gpointer
g_slice_alloc (gsize mem_size)
{
  char *name = g_strdup_printf ("%lu", mem_size);
  gpointer mem = g_slice_alloc_with_name (mem_size, name);
  g_free (name);

  return mem;
}

gpointer
g_slice_alloc_with_name (gsize mem_size,
                         const char *name)
{
  ThreadMemory *tmem;
  gsize chunk_size;
  gpointer mem;

  mem = g_malloc (mem_size);
  TRACE (GLIB_SLICE_ALLOC((void*)mem, mem_size));

  g_assert (((gssize) mem_size) == mem_size);

  G_LOCK (metrics);
  if (g_metrics_requested ("slice-memory-usage") && mem_size != 0)
    {
      GSliceMetrics *metrics = NULL;

      if (metrics_table == NULL)
        {
          metrics_table = g_metrics_table_new (sizeof (GSliceMetrics));
          instance_counter = g_metrics_instance_counter_new ();
          stack_trace_counter = g_metrics_instance_counter_new ();
          stack_trace_sampler = g_metrics_stack_trace_sampler_new ();
        }

      g_slice_allocated_memory += mem_size;
      metrics = g_metrics_table_get_record (metrics_table, name);
      if (metrics == NULL)
        {
          GSliceMetrics empty_metrics = { 0 };
          g_metrics_table_set_record (metrics_table, name, &empty_metrics);
          metrics = g_metrics_table_get_record (metrics_table, name);
        }
      metrics->total_usage += mem_size;
      metrics->number_of_allocations++;

      if (g_metrics_instance_counter_instance_is_interesting (instance_counter, name))
        g_metrics_stack_trace_sampler_take_sample (stack_trace_sampler, name, mem);
    }
  G_UNLOCK (metrics);

  return mem;
}

/**
 * g_slice_alloc0:
 * @block_size: the number of bytes to allocate
 *
 * Allocates a block of memory via g_slice_alloc() and initializes
 * the returned memory to 0.
 *
 * Since GLib 2.76 this always uses the system malloc() implementation
 * internally.
 *
 * Returns: (nullable): a pointer to the allocated block, which will be %NULL
 *    if and only if @mem_size is 0
 *
 * Since: 2.10
 */
gpointer
g_slice_alloc0 (gsize mem_size)
{
  gpointer mem = g_slice_alloc (mem_size);
  if (mem)
    memset (mem, 0, mem_size);
  return mem;
}

gpointer
g_slice_alloc0_with_name (gsize mem_size,
                          const char *name)
{
  gpointer mem = g_slice_alloc_with_name (mem_size, name);
  if (mem)
    memset (mem, 0, mem_size);
  return mem;
}

/**
 * g_slice_copy:
 * @block_size: the number of bytes to allocate
 * @mem_block: the memory to copy
 *
 * Allocates a block of memory from the slice allocator
 * and copies @block_size bytes into it from @mem_block.
 *
 * @mem_block must be non-%NULL if @block_size is non-zero.
 *
 * Since GLib 2.76 this always uses the system malloc() implementation
 * internally.
 *
 * Returns: (nullable): a pointer to the allocated memory block,
 *    which will be %NULL if and only if @mem_size is 0
 *
 * Since: 2.14
 */
gpointer
g_slice_copy (gsize         mem_size,
              gconstpointer mem_block)
{
  gpointer mem;
  char *name = g_strdup_printf ("%lu", mem_size);
  mem = g_slice_copy_with_name (mem_size, mem_block, name);
  g_free (name);

  return mem;
}

gpointer
g_slice_copy_with_name (gsize         mem_size,
                        gconstpointer mem_block,
                        const char    *name)
{
  gpointer mem = g_slice_alloc_with_name (mem_size, name);
  if (mem)
    memcpy (mem, mem_block, mem_size);
  return mem;
}

/**
 * g_slice_free1:
 * @block_size: the size of the block
 * @mem_block: (nullable): a pointer to the block to free
 *
 * Frees a block of memory.
 *
 * The memory must have been allocated via g_slice_alloc() or
 * g_slice_alloc0() and the @block_size has to match the size
 * specified upon allocation. Note that the exact release behaviour
 * can be changed with the [`G_DEBUG=gc-friendly`][G_DEBUG] environment
 * variable.
 *
 * If @mem_block is %NULL, this function does nothing.
 *
 * Since GLib 2.76 this always uses the system free_sized() implementation
 * internally.
 *
 * Since: 2.10
 */
void
g_slice_free1 (gsize    mem_size,
               gpointer mem_block)
{
  if (G_UNLIKELY (g_mem_gc_friendly && mem_block))
    memset (mem_block, 0, mem_size);

  char *name = g_strdup_printf ("%lu", mem_size);
  g_slice_free1_with_name (mem_size, mem_block, name);
  g_free (name);
}

void
g_slice_free1_with_name (gsize    mem_size,
                         gpointer mem_block,
                         const char *name)
{
  gsize chunk_size = P2ALIGN (mem_size);
  guint acat = allocator_categorize (chunk_size);

  if (G_UNLIKELY (!mem_block))
    return;

  if (G_UNLIKELY (g_mem_gc_friendly))
    memset (mem_block, 0, mem_size);

  g_free_sized (current, mem_size);

  TRACE (GLIB_SLICE_FREE((void*)mem_block, mem_size));

  g_assert (((gssize) mem_size) == mem_size);
  G_LOCK (metrics);
  g_slice_allocated_memory -= mem_size;
  if (metrics_table != NULL && mem_size != 0)
    {
      GSliceMetrics *metrics = NULL;

      metrics = g_metrics_table_get_record (metrics_table, name);
      if (metrics != NULL)
        {
          metrics->total_usage -= mem_size;
          metrics->number_of_allocations--;

          if (metrics->total_usage <= 0)
            g_metrics_table_remove_record (metrics_table, name);
        }

      g_metrics_stack_trace_sampler_remove_sample (stack_trace_sampler, mem_block);
    }
  G_UNLOCK (metrics);
}

gsize
g_slice_get_total_allocated_memory (void)
{
  return g_slice_allocated_memory;
}

void
g_slice_lock_metrics (GMetricsInstanceCounter   **instance_counter_out,
                      GMetricsInstanceCounter   **stack_trace_counter_out)
{
  GMetricsTableIter table_iter;
  GSliceMetrics *metrics;
  GMetricsStackTraceSample *sample;
  GMetricsStackTraceSamplerIter sampler_iter;
  const char *name;
  G_LOCK (metrics);

  *instance_counter_out = instance_counter;
  *stack_trace_counter_out = stack_trace_counter;

  if (instance_counter == NULL || stack_trace_sampler == NULL)
    return;

  g_metrics_instance_counter_start_record (instance_counter);
  g_metrics_table_iter_init (&table_iter, metrics_table);
  while (g_metrics_table_iter_next (&table_iter, &name, &metrics))
    g_metrics_instance_counter_add_instances (instance_counter, name, NULL, metrics->number_of_allocations, metrics->total_usage);
  g_metrics_instance_counter_end_record (instance_counter);

  g_metrics_instance_counter_start_record (stack_trace_counter);
  g_metrics_stack_trace_sampler_iter_init (&sampler_iter, stack_trace_sampler);
  while (g_metrics_stack_trace_sampler_iter_next (&sampler_iter, &sample))
    {
      const char *output;

      output = g_metrics_stack_trace_get_output (sample->stack_trace);
      g_metrics_instance_counter_add_instances (stack_trace_counter, output, sample->name, sample->number_of_hits, 1);
    }
  g_metrics_instance_counter_end_record (stack_trace_counter);

}

void
g_slice_unlock_metrics (void)
{
  G_UNLOCK (metrics);
}

/**
 * g_slice_free_chain_with_offset:
 * @block_size: the size of the blocks
 * @mem_chain: (nullable):  a pointer to the first block of the chain
 * @next_offset: the offset of the @next field in the blocks
 *
 * Frees a linked list of memory blocks of structure type @type.
 *
 * The memory blocks must be equal-sized, allocated via
 * g_slice_alloc() or g_slice_alloc0() and linked together by a
 * @next pointer (similar to #GSList). The offset of the @next
 * field in each block is passed as third argument.
 * Note that the exact release behaviour can be changed with the
 * [`G_DEBUG=gc-friendly`][G_DEBUG] environment variable.
 *
 * If @mem_chain is %NULL, this function does nothing.
 *
 * Since GLib 2.76 this always uses the system free_sized() implementation
 * internally.
 *
 * Since: 2.10
 */
void
g_slice_free_chain_with_offset (gsize    mem_size,
                                gpointer mem_chain,
                                gsize    next_offset)
{
  char *name = g_strdup_printf ("%lu", mem_size);
  g_slice_free_chain_with_offset_and_name (mem_size, mem_chain, next_offset, name);
  g_free (name);
}

void
g_slice_free_chain_with_offset_and_name (gsize    mem_size,
                                         gpointer mem_chain,
                                         gsize    next_offset,
                                         const char *name)
{
  gssize chain_total = 0, chain_length = 0;
  gboolean is_interesting = FALSE;
  gpointer slice = mem_chain;

  G_LOCK (metrics);
  if (instance_counter && stack_trace_sampler)
    is_interesting = g_metrics_instance_counter_instance_is_interesting (instance_counter, name);
  G_UNLOCK (metrics);

  while (slice)
    {
      guint8 *current = slice;
      slice = *(gpointer *) (current + next_offset);
      if (G_UNLIKELY (g_mem_gc_friendly))
        memset (current, 0, mem_size);

      chain_total += mem_size;
      chain_length++;

       if (is_interesting)
         {
           G_LOCK (metrics);
           g_metrics_stack_trace_sampler_remove_sample (stack_trace_sampler, current);
           G_UNLOCK (metrics);
         }

      g_free_sized (current, mem_size);
    }

    G_LOCK (metrics);
    g_slice_allocated_memory -= chain_total;
    if (metrics_table != NULL)
      {
        GSliceMetrics *metrics = NULL;

        metrics = g_metrics_table_get_record (metrics_table, name);
        if (metrics != NULL)
          {
            metrics->total_usage -= chain_total;
            metrics->number_of_allocations -= chain_length;

            if (metrics->total_usage <= 0)
              g_metrics_table_remove_record (metrics_table, name);
          }
      }
    G_UNLOCK (metrics);
}

#ifdef G_ENABLE_DEBUG
void
g_slice_debug_tree_statistics (void)
{
  g_fprintf (stderr, "GSlice: Implementation dropped in GLib 2.76\n");
}
#endif /* G_ENABLE_DEBUG */
