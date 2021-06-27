/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.         See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */

/*
 * MT safe
 */

#include "config.h"

#include <execinfo.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <locale.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <sys/user.h>

#define uthash_malloc(_s) g_metrics_allocation_block_store_allocate_with_name (metrics_allocation_block_store, _s, "uthash_malloc")
#define uthash_free(_p,_s) g_metrics_allocation_block_store_deallocate (metrics_allocation_block_store, _p)
#include <uthash.h>
#include <utlist.h>
#include <utstring.h>


#include "glib-init.h"
#include "galloca.h"
#include "gbacktrace.h"
#include "gcharset.h"
#include "gconvert.h"
#include "genviron.h"
#include "gfileutils.h"
#include "gmain.h"
#include "gmem.h"
#include "gmetrics.h"
#include "gprintfint.h"
#include "gtestutils.h"
#include "gthread.h"
#include "gstdio.h"
#include "gstrfuncs.h"
#include "gstring.h"
#include "gtimer.h"

#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <zlib.h>

#undef G_BREAKPOINT
#define G_BREAKPOINT() sleep(10)

extern void *__libc_malloc (size_t size);
extern void *__libc_realloc (void *ptr, size_t size);
extern void *__libc_calloc (size_t num, size_t size);
extern void __libc_free (void *ptr);
typedef struct _GMetricsConfig GMetricsConfig;
typedef struct _GMetricsAllocationHeader GMetricsAllocationHeader;
typedef union _GMetricsAllocationBlock GMetricsAllocationBlock;
typedef struct _GMetricsAllocation GMetricsAllocation;
typedef struct _GMetricsAllocationBlockStore GMetricsAllocationBlockStore;
typedef struct _GMetricsAllocationBlockStoreIter GMetricsAllocationBlockStoreIter;
typedef struct _GMetricsAllocationFileMapIter GMetricsAllocationFileMapIter;
typedef struct _GMetricsAllocationPageTable GMetricsAllocationPageTable;
typedef struct _GMetricsAllocationPage GMetricsAllocationPage;
typedef struct _GMetricsAllocationFileMap GMetricsAllocationFileMap;
typedef struct _GMetricsListNode GMetricsListNode;

#define round_to_power_of_two(n) (1UL << (64 - __builtin_clzl (n - 1)))
#define round_to_multiple(n, m) (((n) + ((round_to_power_of_two (m) - 1))) & ~(round_to_power_of_two (m) - 1))

struct _GMetricsConfig
{
  char log_dir[512];
  char allocation_map_dir[512];
  char skipped_metrics[512];
  char included_metrics[512];
  char collection_ignore_list[512];
  char collection_include_list[512];
  guint collection_interval;
  int stack_trace_size;
  int stack_trace_annotation_size;
  gsize max_allocation_block_stores;
  gsize allocation_block_store_size;
  gsize dedicated_allocation_block_store_threshold;
  gsize allocated_blocks_threshold;
  gsize generations_to_settle;
  gsize generations_to_reset_average_window;
  gsize number_of_interesting_instances;
  gsize stack_trace_sample_interval;
  guint32 metrics_enabled : 1;
  guint32 override_system_malloc : 1;
  guint32 override_glib_malloc : 1;
  guint32 validate_allocation_blocks : 1;
  guint32 validate_heap : 1;
  guint32 use_map_files : 2;
  guint32 give_back_deallocated_pages : 2;
};

struct _GMetricsFile
{
  gzFile gzipped_file;
  char *static_format_string;
  char *variadic_format_string;
  gdouble now;
};

struct _GMetricsAllocationHeader
{
  char name[64];
  guint32 is_allocated;
  gsize number_of_blocks;
  GMetricsAllocationBlock *previous_block;
};

union _GMetricsAllocationBlock
{
  GMetricsAllocationHeader header;
  char payload[128];
};

struct _GMetricsAllocation
{
  GMetricsAllocationBlock header_block;
  GMetricsAllocationBlock payload_blocks[0];
};

typedef enum
{
  G_METRICS_INDICES_HEAP_OFFSET_LEFT_CHILD = 0,
  G_METRICS_INDICES_HEAP_OFFSET_RIGHT_CHILD,
  G_METRICS_INDICES_HEAP_NUMBER_OF_CHILDREN
} GMetricsIndicesHeapChildOffset;

struct _GMetricsAllocationPageTable
{
  gsize size;
  gsize max_number_of_pages;
  gsize number_of_pages;
  gsize page_indices_heap_size;
  union
  {
    char *map_address;
    struct
    {
      gsize *page_indices_heap;
      GMetricsAllocationPage *pages;
    };
  };
};

struct _GMetricsAllocationPage
{
  GMetricsAllocationBlock *run_of_free_blocks;
  gsize number_of_allocated_blocks;
  gssize number_of_pages_in_run;
  gssize heap_index;
};

struct _GMetricsAllocationFileMap
{
  GMetricsAllocationBlockStore *block_store;
  GMetricsAllocationPageTable page_table;
  union
  {
    char *map_address;
    GMetricsAllocationBlock *blocks;
  };
  gsize size;
  gsize number_of_blocks;
};

struct _GMetricsAllocationBlockStore
{
  char name[128];
  char thread_name[32];
  GMetricsStackTrace *stack_trace;
  GMetricsAllocationFileMap file_map;
  gsize number_of_allocations;
  gsize total_bytes_allocated;
  guint32 is_dedicated : 1;
  guint32 is_thread_default : 1;
};

struct _GMetricsAllocationBlockStoreIter
{
  GMetricsListIter list_iter;
};

struct _GMetricsAllocationFileMapIter
{
  GMetricsAllocationPage    *page;
  GMetricsAllocationFileMap *file_map;
  GMetricsAllocationBlock *starting_block;
  GMetricsAllocationBlock *previous_block;
  gsize items_examined;
};

static gsize g_metrics_allocation_page_table_get_number_of_pages_for_blocks (GMetricsAllocationPageTable *page_table,
                                                                             gsize                        number_of_blocks);
static void g_metrics_allocation_page_table_decrease_page_allocation (GMetricsAllocationPageTable *page_table,
                                                                      GMetricsAllocationFileMap   *file_map,
                                                                      GMetricsAllocationPage      *page,
                                                                      gsize                        number_of_blocks);

static GMetricsAllocationPage * g_metrics_allocation_page_table_deallocate_pages (GMetricsAllocationPageTable *page_table,
                                                                                  GMetricsAllocationFileMap   *file_map,
                                                                                  GMetricsAllocationPage      *pages,
                                                                                  gsize                        number_of_pages);

static void g_metrics_allocation_file_map_iter_init_at_block (GMetricsAllocationFileMapIter *iter,
                                                              GMetricsAllocationFileMap     *file_map,
                                                              GMetricsAllocationPage        *page,
                                                              GMetricsAllocationBlock       *block);
static void g_metrics_allocation_file_map_iter_init_after_block (GMetricsAllocationFileMapIter *iter,
                                                                 GMetricsAllocationFileMap     *file_map,
                                                                 GMetricsAllocationPage        *page,
                                                                 GMetricsAllocationBlock       *block);
static gboolean g_metrics_allocation_file_map_iter_next (GMetricsAllocationFileMapIter  *iter,
                                                         GMetricsAllocationBlock       **next_block);

static gboolean g_metrics_allocation_file_map_is_open (GMetricsAllocationFileMap *file_map);

static GMetricsAllocationBlock *get_first_block_in_page (GMetricsAllocationFileMap *file_map,
                                                         GMetricsAllocationPage    *page);
static volatile gboolean needs_flush;
static gulong g_metrics_generation;
static int timeout_fd = -1;
GMetricsList *timeout_handlers;
G_LOCK_DEFINE_STATIC (timeouts);
static GMetricsAllocationBlockStore store_for_allocation_block_stores;
G_LOCK_DEFINE_STATIC (allocation_block_stores);
static GMetricsAllocationBlockStore *metrics_allocation_block_store;
static __thread GMetricsList block_store_stack;
static GMetricsList allocation_block_stores;
static GMetricsFile *allocation_block_store_metrics_file;
G_LOCK_DEFINE_STATIC (allocations);

static __thread GMetricsStackTraceAnnotationHandler stack_trace_annotation_handler;
static __thread gpointer stack_trace_annotation_handler_user_data;

static GMetricsConfig metrics_config;

static const char *default_skipped_metrics="arrays lists metrics-allocations objects-by-type ptr-arrays signals";
static const char *default_collection_ignore_list="Handler GSList";

static gsize
g_metrics_allocation_page_table_get_number_of_blocks_per_page (GMetricsAllocationPageTable *page_table)
{
  g_assert (PAGE_SIZE % sizeof (GMetricsAllocationBlock) == 0);

  return PAGE_SIZE / sizeof (GMetricsAllocationBlock);
}

static void
g_metrics_allocation_page_table_swap_page_indices (GMetricsAllocationPageTable *page_table,
                                                   gsize                       *page_index_1,
                                                   gsize                       *page_index_2)
{
  gssize page_index_store;

  if (*page_index_1 >= page_table->number_of_pages)
    G_BREAKPOINT ();

  if (*page_index_2 >= page_table->number_of_pages)
    G_BREAKPOINT ();

  page_index_store = *page_index_2;
  *page_index_2 = *page_index_1;
  *page_index_1 = page_index_store;
}

static void
g_metrics_allocation_page_table_swap_pages_on_heap (GMetricsAllocationPageTable *page_table,
                                                    GMetricsAllocationPage      *page_1,
                                                    GMetricsAllocationPage      *page_2)
{
  gssize heap_index_store;

  if (page_1->heap_index == -1)
    G_BREAKPOINT ();

  if (page_2->heap_index == -1)
    G_BREAKPOINT ();

  if (page_1->heap_index >= page_table->page_indices_heap_size)
    G_BREAKPOINT ();

  if (page_2->heap_index >= page_table->page_indices_heap_size)
    G_BREAKPOINT ();

  g_metrics_allocation_page_table_swap_page_indices (page_table,
                                                     &page_table->page_indices_heap[page_1->heap_index],
                                                     &page_table->page_indices_heap[page_2->heap_index]);

  heap_index_store = page_2->heap_index;
  page_2->heap_index = page_1->heap_index;
  page_1->heap_index = heap_index_store;
}

static gssize
get_number_of_allocated_blocks_for_pages (GMetricsAllocationPageTable *page_table,
                                          GMetricsAllocationPage      *pages)
{

  gssize number_of_allocated_blocks;
  gssize i;

  number_of_allocated_blocks = 0;
  for (i = 0; i < pages->number_of_pages_in_run; i++)
    {
      GMetricsAllocationPage *page = &pages[i];

      number_of_allocated_blocks += page->number_of_allocated_blocks;
    }

  return number_of_allocated_blocks;
}

#if 0
static void
dump_heap (GMetricsAllocationPageTable *page_table)
{
  gsize blocks_per_page;
  gsize i;

  blocks_per_page = g_metrics_allocation_page_table_get_number_of_blocks_per_page (page_table);

  printf("heap (%ld/%ld total pages): ", page_table->page_indices_heap_size, page_table->number_of_pages);
  for (i = 0; i < page_table->page_indices_heap_size; i++)
    {
      GMetricsAllocationPage *page;

      page = &page_table->pages[page_table->page_indices_heap[i]];
      printf ("[%ld] = %ld/%ld, ", i,
              get_number_of_allocated_blocks_for_pages (page_table, page),
              page->number_of_pages_in_run * blocks_per_page);
    }
  printf("\n");

  printf("unheapable pages: ");
  for (i = 0; i < page_table->number_of_pages; i++)
    {
      GMetricsAllocationPage *page;

      page = &page_table->pages[i];

      if (page->heap_index != -1)
        continue;

      if (page->number_of_allocated_blocks == 0)
        continue;

      if (page->number_of_pages_in_run == -1)
        continue;

      if (page->number_of_pages_in_run > 1)
        printf ("[%ld...%ld] = %ld/%ld, ", i, i + page->number_of_pages_in_run - 1,
                get_number_of_allocated_blocks_for_pages (page_table, page),
                page->number_of_pages_in_run * blocks_per_page);
      else
        printf ("[%ld] = %ld/%ld, ", i,
                get_number_of_allocated_blocks_for_pages (page_table, page),
                page->number_of_pages_in_run * blocks_per_page);
    }
  printf("\n");
}
#endif

static void
validate_heap (GMetricsAllocationPageTable *page_table,
               gssize                       heap_index,
               gssize                      *subheap_most_filled_blocks)
{
  gssize child_heap_index, last_heap_index;
  gssize child_filled_blocks, num_filled_blocks;
  GMetricsAllocationPage *page;
  GMetricsIndicesHeapChildOffset child_offset;

  if (!metrics_config.validate_heap)
    return;

  if (page_table->page_indices_heap_size == 0)
    {
      if (subheap_most_filled_blocks)
        *subheap_most_filled_blocks = 0;
      return;
    }

  if (page_table->page_indices_heap[heap_index] > page_table->number_of_pages - 1)
    G_BREAKPOINT();

  page = &page_table->pages[page_table->page_indices_heap[heap_index]];

  num_filled_blocks = get_number_of_allocated_blocks_for_pages (page_table, page);

  last_heap_index = page_table->page_indices_heap_size - 1;

  if (heap_index == last_heap_index)
    {
      if (subheap_most_filled_blocks)
        *subheap_most_filled_blocks = num_filled_blocks;
      return;
    }

  for (child_offset = G_METRICS_INDICES_HEAP_OFFSET_LEFT_CHILD;
       child_offset <= G_METRICS_INDICES_HEAP_OFFSET_RIGHT_CHILD;
       child_offset++)
    {
      child_heap_index = G_METRICS_INDICES_HEAP_NUMBER_OF_CHILDREN * heap_index + 1 + child_offset;

      if (child_heap_index > last_heap_index)
        break;

      validate_heap (page_table, child_heap_index, &child_filled_blocks);

      if (child_filled_blocks > num_filled_blocks)
          G_BREAKPOINT ();
    }

  if (subheap_most_filled_blocks)
    *subheap_most_filled_blocks = num_filled_blocks;
}

static int
pages_allocated_blocks_compare (GMetricsAllocationPageTable *page_table,
                                GMetricsAllocationPage      *first_page_in_run_1,
                                GMetricsAllocationPage      *first_page_in_run_2)
{
  gssize page_run_1_blocks, page_run_2_blocks;

  page_run_1_blocks = get_number_of_allocated_blocks_for_pages (page_table,
                                                                first_page_in_run_1);
  page_run_2_blocks = get_number_of_allocated_blocks_for_pages (page_table,
                                                                first_page_in_run_2);
  if (page_run_1_blocks != page_run_2_blocks)
    return page_run_1_blocks - page_run_2_blocks;

  return -(first_page_in_run_1->number_of_pages_in_run - first_page_in_run_2->number_of_pages_in_run);
}

static gssize
g_metrics_allocation_page_table_sort_page_with_ancestors (GMetricsAllocationPageTable *page_table,
                                                          gsize                        current_heap_index)
{
  gsize last_heap_index, parent_heap_index;
  GMetricsAllocationPage *page, *parent_page;
  gssize page_index, parent_page_index;

  last_heap_index = page_table->page_indices_heap_size - 1;

  if (current_heap_index > last_heap_index)
    return -1;

  page_index = page_table->page_indices_heap[current_heap_index];
  page = &page_table->pages[page_index];

  if (current_heap_index == 0)
    return page_index;

  parent_heap_index = (current_heap_index - 1) / 2;

  parent_page_index = page_table->page_indices_heap[parent_heap_index];

  parent_page = &page_table->pages[parent_page_index];

  if (pages_allocated_blocks_compare (page_table, parent_page, page) > 0)
    return page_index;

  g_metrics_allocation_page_table_swap_pages_on_heap (page_table, parent_page, page);

  return g_metrics_allocation_page_table_sort_page_with_ancestors (page_table,
                                                                   parent_heap_index);
}

static gssize
g_metrics_allocation_page_table_sort_page_with_children (GMetricsAllocationPageTable *page_table,
                                                         gsize                        current_heap_index)
{
  gsize last_heap_index, most_filled_page_heap_index = 0;
  GMetricsAllocationPage *page, *child_page, *most_filled_page;
  gssize current_page_index, most_filled_page_index = -1;

  GMetricsIndicesHeapChildOffset child_offset;

  last_heap_index = page_table->page_indices_heap_size - 1;

  if (current_heap_index > last_heap_index)
    return -1;

  current_page_index = page_table->page_indices_heap[current_heap_index];
  page = &page_table->pages[current_page_index];

  most_filled_page_index = current_page_index;
  most_filled_page_heap_index = current_heap_index;
  most_filled_page = page;

  for (child_offset = G_METRICS_INDICES_HEAP_OFFSET_LEFT_CHILD;
       child_offset <= G_METRICS_INDICES_HEAP_OFFSET_RIGHT_CHILD;
       child_offset++)
    {
      gsize child_heap_index;
      gssize child_page_index;

      child_heap_index = G_METRICS_INDICES_HEAP_NUMBER_OF_CHILDREN * current_heap_index + 1 + child_offset;
      if (child_heap_index > last_heap_index)
        break;

      child_page_index = page_table->page_indices_heap[child_heap_index];

      if (child_page_index >= page_table->number_of_pages)
        G_BREAKPOINT ();

       child_page = &page_table->pages[child_page_index];

      if (pages_allocated_blocks_compare (page_table, child_page, most_filled_page) > 0)
        {
          most_filled_page_index = child_page_index;
          most_filled_page_heap_index = child_heap_index;
          most_filled_page = &page_table->pages[most_filled_page_index];
        }
    }

  if (most_filled_page_index == current_page_index)
    return current_page_index;

  g_metrics_allocation_page_table_swap_pages_on_heap (page_table, most_filled_page, page);

  return g_metrics_allocation_page_table_sort_page_with_children (page_table, most_filled_page_heap_index);
}

static gboolean
g_metrics_allocation_page_has_free_blocks (GMetricsAllocationPage *page)
{
  GMetricsAllocationBlock *block;

  if (page->run_of_free_blocks == NULL)
    return page->number_of_allocated_blocks == 0;

  block = page->run_of_free_blocks;

  if (block->header.is_allocated)
    G_BREAKPOINT ();

  return block->header.number_of_blocks != 0;
}

static GMetricsAllocationPage *
g_metrics_allocation_page_table_allocate_pages (GMetricsAllocationPageTable *page_table,
                                                gsize                        number_of_pages)
{
  GMetricsAllocationPage *pages = NULL;
  gssize last_page_index, new_page_index, i;

  if (page_table->number_of_pages + number_of_pages > page_table->max_number_of_pages)
    return NULL;

  if (page_table->number_of_pages == 0)
    {
      new_page_index = 0;
    }
  else
    {
      last_page_index = page_table->number_of_pages - 1;
      new_page_index = last_page_index + 1;
    }

  page_table->number_of_pages += number_of_pages;

  for (i = 0; i < number_of_pages; i++)
    {
      GMetricsAllocationPage *page;

      page = &page_table->pages[new_page_index];
      page->heap_index = -1;

      new_page_index++;

      if (pages == NULL)
        {
          page->number_of_pages_in_run = number_of_pages;
          pages = page;
        }
      else
        {
          page->number_of_pages_in_run = -1;
        }

      page->number_of_allocated_blocks = 0;
      page->run_of_free_blocks = NULL;
    }

  return pages;
}

static gsize
g_metrics_allocation_page_table_get_free_blocks_for_pages (GMetricsAllocationPageTable *page_table,
                                                           GMetricsAllocationFileMap   *file_map,
                                                           gsize                        index)
{
  GMetricsAllocationPage *page;
  GMetricsAllocationBlock *block;
  GMetricsAllocationHeader *header;

  page = &page_table->pages[index];

  if (page->number_of_pages_in_run == -1)
    G_BREAKPOINT ();

  if (page->number_of_allocated_blocks == 0)
    {
      gsize blocks_per_page;

      blocks_per_page = g_metrics_allocation_page_table_get_number_of_blocks_per_page (page_table);

      return blocks_per_page * page->number_of_pages_in_run;
    }

  /* No free blocks left
   */
  if (!g_metrics_allocation_page_has_free_blocks (page))
    return 0;

  block = page->run_of_free_blocks;
  header = &block->header;

  if (header->is_allocated)
    G_BREAKPOINT ();

  if (header->number_of_blocks > (PAGE_SIZE / sizeof (GMetricsAllocationBlock)))
    G_BREAKPOINT ();

  return header->number_of_blocks;
}

static gssize
g_metrics_allocation_page_table_find_child_page_for_blocks (GMetricsAllocationPageTable *page_table,
                                                            GMetricsAllocationFileMap   *file_map,
                                                            gsize                        current_heap_index,
                                                            gsize                        number_of_blocks_needed)
{
  gssize best_page_index = -1;
  gsize last_heap_index;

  GMetricsIndicesHeapChildOffset child_offset;

  last_heap_index = page_table->page_indices_heap_size - 1;

  for (child_offset = G_METRICS_INDICES_HEAP_OFFSET_LEFT_CHILD;
       child_offset <= G_METRICS_INDICES_HEAP_OFFSET_RIGHT_CHILD;
       child_offset++)
    {
      gsize child_heap_index, child_page_free_blocks, candidate_page_free_blocks;
      gssize child_page_index, candidate_page_index = -1;

      child_heap_index = G_METRICS_INDICES_HEAP_NUMBER_OF_CHILDREN * current_heap_index + 1 + child_offset;
      if (child_heap_index > last_heap_index)
        break;

      child_page_index = page_table->page_indices_heap[child_heap_index];

      if (child_page_index > page_table->number_of_pages - 1)
          G_BREAKPOINT ();

      child_page_free_blocks = g_metrics_allocation_page_table_get_free_blocks_for_pages (page_table, file_map, child_page_index);

      if (child_page_free_blocks >= number_of_blocks_needed)
        {
          candidate_page_index = child_page_index;
          candidate_page_free_blocks = child_page_free_blocks;
        }
      else
        {
          candidate_page_index = g_metrics_allocation_page_table_find_child_page_for_blocks (page_table,
                                                                                             file_map,
                                                                                             child_heap_index,
                                                                                             number_of_blocks_needed);
          if (candidate_page_index >= 0)
            candidate_page_free_blocks = g_metrics_allocation_page_table_get_free_blocks_for_pages (page_table, file_map, candidate_page_index);
        }

      if (candidate_page_index >= 0)
        {
          if (best_page_index < 0)
            {
              best_page_index = candidate_page_index;
            }
          else
            {
              gsize best_page_free_blocks;

              best_page_free_blocks = g_metrics_allocation_page_table_get_free_blocks_for_pages (page_table, file_map, best_page_index);

              if (candidate_page_free_blocks < best_page_free_blocks)
                best_page_index = candidate_page_index;
            }
        }
    }

  return best_page_index;
}

static GMetricsAllocationPage *
g_metrics_allocation_page_table_find_pages_for_blocks (GMetricsAllocationPageTable *page_table,
                                                      GMetricsAllocationFileMap   *file_map,
                                                      gsize                        number_of_blocks_needed)
{
  gsize root_heap_index;
  gsize number_of_free_blocks;
  gssize page_index;

  if (page_table->page_indices_heap_size != 0)
    {
      root_heap_index = 0;

      page_index = page_table->page_indices_heap[root_heap_index];

      if (page_index > page_table->number_of_pages - 1)
          G_BREAKPOINT ();

      number_of_free_blocks = g_metrics_allocation_page_table_get_free_blocks_for_pages (page_table, file_map, page_index);

      if (number_of_free_blocks >= number_of_blocks_needed)
        return &page_table->pages[page_index];

      page_index = g_metrics_allocation_page_table_find_child_page_for_blocks (page_table,
                                                                               file_map,
                                                                               root_heap_index,
                                                                               number_of_blocks_needed);

      if (page_index >= 0 && page_index > page_table->number_of_pages - 1)
          G_BREAKPOINT ();

      if (page_index >= 0)
        return &page_table->pages[page_index];
    }

  return NULL;
}

static char *
int_to_string (guint64  integer,
               char    *string,
               gsize    string_size)
{
  gsize i, j, bytes_used;
  char swap_byte;

  bytes_used = 0;
  if (integer == 0)
    {
      string[bytes_used++] = '0';
    }
  else while (integer != 0 && bytes_used < string_size)
    {
      string[bytes_used++] = (integer % 10) + '0';
      integer /= 10;
    }
  string[bytes_used] = '\0';

  j = bytes_used - 1;
  for (i = 0; i < j; i++, j--)
    {
      swap_byte = string[i];
      string[i] = string[j];
      string[j] = swap_byte;
    }
  return string + bytes_used;
}

gboolean
g_metrics_enabled (void)
{
  return metrics_config.metrics_enabled;
}

static gsize
g_metrics_allocation_get_payload_size (GMetricsAllocation *allocation)
{
  GMetricsAllocationHeader *header;
  header = &allocation->header_block.header;

  return (header->number_of_blocks * sizeof (GMetricsAllocationBlock)) - sizeof (allocation->header_block);
}

static GMetricsAllocationBlock *
get_first_block_in_page (GMetricsAllocationFileMap *file_map,
                         GMetricsAllocationPage    *page)
{
  GMetricsAllocationPageTable *page_table;
  gssize page_index;
  gsize blocks_per_page = PAGE_SIZE / sizeof (GMetricsAllocationBlock);

  page_table = &file_map->page_table;
  page_index = page - page_table->pages;

  g_assert (PAGE_SIZE % sizeof (GMetricsAllocationBlock) == 0);

  return &file_map->blocks[page_index * blocks_per_page];
}

static GMetricsAllocationBlock *
get_last_block_in_page (GMetricsAllocationFileMap *file_map,
                        GMetricsAllocationPage    *page)
{
  GMetricsAllocationPageTable *page_table;
  gssize page_index;
  gsize blocks_per_page = PAGE_SIZE / sizeof (GMetricsAllocationBlock);

  page_table = &file_map->page_table;
  page_index = page - page_table->pages;

  g_assert (PAGE_SIZE % sizeof (GMetricsAllocationBlock) == 0);

  return &file_map->blocks[((page_index + 1) * blocks_per_page) - 1];
}

static gboolean
block_is_in_bounds (GMetricsAllocationFileMap *file_map,
                    GMetricsAllocationPage    *page,
                    GMetricsAllocationBlock   *block)
{
  GMetricsAllocationBlock *first_block, *last_block;

  if (page != NULL)
    {
      first_block = get_first_block_in_page (file_map, page);
      last_block = get_last_block_in_page (file_map, page);
    }
  else
    {
      first_block = &file_map->blocks[0];
      last_block = &file_map->blocks[file_map->number_of_blocks - 1];
    }

  if (block < first_block)
    return FALSE;

  if (block > last_block)
    return FALSE;

  return TRUE;
}

static void
validate_block_is_in_bounds (GMetricsAllocationFileMap *file_map,
                             GMetricsAllocationPage    *page,
                             GMetricsAllocationBlock   *block)
{
  if (!metrics_config.validate_allocation_blocks)
    return;

  if (!block_is_in_bounds (file_map, page, block))
    G_BREAKPOINT ();
}

static void
g_metrics_allocation_file_map_validate_block (GMetricsAllocationFileMap *file_map,
                                              GMetricsAllocationPage    *page,
                                              GMetricsAllocationBlock   *block)
{
  GMetricsAllocation *allocation;
  GMetricsAllocationHeader *header;
  GMetricsAllocationPageTable *page_table;
  gsize expected_number_of_pages, number_of_pages;

  page_table = &file_map->page_table;

  if (!metrics_config.validate_allocation_blocks)
    return;

  if (!block)
    return;

  allocation = (GMetricsAllocation *) block;
  header = &allocation->header_block.header;

  if (header->is_allocated != 1 && header->is_allocated != 0)
    G_BREAKPOINT ();

  if (header->number_of_blocks == 0)
    G_BREAKPOINT ();

  validate_block_is_in_bounds (file_map, page, block);

  if (header->previous_block != NULL &&
      header->number_of_blocks > 32)
    G_BREAKPOINT ();

  if (header->previous_block != NULL)
    {
      GMetricsAllocationHeader *previous_header;

      if (header->previous_block < file_map->blocks)
        G_BREAKPOINT ();

      previous_header = (GMetricsAllocationHeader *) header->previous_block;

      if (previous_header->number_of_blocks == 0)
        G_BREAKPOINT ();

      if (previous_header->number_of_blocks > file_map->number_of_blocks)
        G_BREAKPOINT ();

      if (header->previous_block + previous_header->number_of_blocks != block)
        G_BREAKPOINT ();
    }

  if (block + header->number_of_blocks < file_map->blocks + file_map->number_of_blocks)
    {
      GMetricsAllocationBlock *next_block;
      GMetricsAllocationHeader *next_header;

      next_block = block + header->number_of_blocks;
      next_header = (GMetricsAllocationHeader *) next_block;

      if (block_is_in_bounds (file_map, page, next_block))
        {
          if (next_header->number_of_blocks == 0)
            G_BREAKPOINT ();

          if (next_header->number_of_blocks > file_map->number_of_blocks)
            G_BREAKPOINT ();

          if (next_header->previous_block != block)
            G_BREAKPOINT ();
        }
    }

  expected_number_of_pages = g_metrics_allocation_page_table_get_number_of_pages_for_blocks (page_table, header->number_of_blocks);
  number_of_pages = page->number_of_pages_in_run;

  if (expected_number_of_pages != number_of_pages)
    G_BREAKPOINT ();
}

static void
compute_allocation_map_file_path (char       *file_path,
                                  gsize       file_path_size,
                                  const char *name)
{
  char user_id[32] = "";
  char process_id[32] = "";

  strncat (file_path, metrics_config.allocation_map_dir, file_path_size - 1);
  strncat (file_path, "/", file_path_size - 1);
  strncat (file_path, "user-", file_path_size - 1);
  int_to_string (getuid (), user_id, sizeof (user_id) - 1);
  strncat (file_path, user_id, file_path_size - 1);
  strncat (file_path, "-for-pid-", file_path_size - 1);
  int_to_string (getpid (), process_id, sizeof (process_id) - 1);
  strncat (file_path, process_id, file_path_size - 1);
}

static void *
open_map_file (const char  *filename,
               gsize        size)
{
  int result;
  int map_fd = -1;
  int mmap_flags;
  void *map_address;

  map_fd = open (filename, O_RDWR | O_CREAT, 0644);

  if (map_fd < 0)
    goto fail;

  result = ftruncate (map_fd, size);

  if (metrics_config.use_map_files == 1)
    unlink (filename);

  if (result < 0)
    goto fail;

  if (metrics_config.use_map_files)
    mmap_flags = MAP_SHARED;
  else
    mmap_flags = MAP_PRIVATE;

  map_address = mmap (NULL, size, PROT_READ | PROT_WRITE, mmap_flags, map_fd, 0);

  if (map_address == MAP_FAILED)
    goto fail;

  close (map_fd);
  map_fd = -1;

  return map_address;

fail:
  if (map_fd >= 0)
    {
      close (map_fd);
      map_fd = -1;
    }
  return MAP_FAILED;
}

static gsize
calculate_blocks_needed_for_size (gsize size)
{
  GMetricsAllocation allocation;
  gsize aligned_size;
  static const gsize payload_block_size = sizeof (GMetricsAllocationBlock);

  aligned_size = sizeof (allocation.header_block) + round_to_multiple (size, payload_block_size);

  return aligned_size / payload_block_size;
}

static gboolean
g_metrics_allocation_file_map_open (GMetricsAllocationFileMap *file_map,
                                    gsize                      size)
{
  GMetricsAllocationBlockStore *block_store;
  GMetricsAllocationPageTable *page_table;
  gsize number_of_pages, size_of_pages, page_indices_heap_size;
  char filename[1024] = "";

  file_map->size = calculate_blocks_needed_for_size (size) * sizeof (GMetricsAllocationBlock);

  block_store = file_map->block_store;
  compute_allocation_map_file_path (filename, sizeof (filename) - 1, block_store->name);

  file_map->map_address = open_map_file (filename, file_map->size);

  if (file_map->map_address == MAP_FAILED)
    goto fail;

  page_table = &file_map->page_table;

  page_table->max_number_of_pages = size / PAGE_SIZE;

  number_of_pages = round_to_multiple (file_map->size, PAGE_SIZE) / PAGE_SIZE;

  size_of_pages = number_of_pages * sizeof (GMetricsAllocationPage);
  page_indices_heap_size = number_of_pages * sizeof (gsize);

  page_table->size = size_of_pages + page_indices_heap_size;

  strncat (filename, "-page-table", sizeof (filename) - 1);
  page_table->map_address = open_map_file (filename, page_table->size);

  if (page_table->map_address == MAP_FAILED)
    goto fail;

  page_table->number_of_pages = 0;

  page_table->pages = (GMetricsAllocationPage *) (page_table->map_address + page_table->size - size_of_pages);

  file_map->number_of_blocks = file_map->size / sizeof (GMetricsAllocationBlock);

  file_map->blocks[0].header.number_of_blocks = file_map->number_of_blocks;
  file_map->blocks[0].header.is_allocated = FALSE;
  file_map->blocks[0].header.previous_block = NULL;

  return TRUE;

fail:
  if (file_map->map_address != MAP_FAILED)
    {
      munmap (file_map->map_address, file_map->size);
      file_map->map_address = MAP_FAILED;
    }
  if (file_map->page_table.map_address != MAP_FAILED)
    {
      munmap (file_map->page_table.map_address, file_map->page_table.size);
      file_map->page_table.map_address = MAP_FAILED;
    }
  return FALSE;
}

static void
g_metrics_allocation_file_map_close (GMetricsAllocationFileMap *file_map)
{
  if (file_map->map_address != MAP_FAILED)
    {
      munmap (file_map->map_address, file_map->size);
      file_map->map_address = MAP_FAILED;
    }

  if (file_map->page_table.map_address != MAP_FAILED)
    {
      munmap (file_map->page_table.map_address, file_map->page_table.size);
      file_map->page_table.map_address = MAP_FAILED;
    }
}

static gboolean
g_metrics_allocation_file_map_is_open (GMetricsAllocationFileMap *file_map)
{
  return file_map->map_address != MAP_FAILED;
}

static gboolean
g_metrics_allocation_file_map_has_allocation (GMetricsAllocationFileMap *file_map,
                                              gpointer                   allocation)
{
  char *allocation_bytes = allocation;
  return allocation_bytes >= file_map->map_address && allocation_bytes < (file_map->map_address + file_map->size);
}

#if 0
static gsize
g_metrics_allocation_file_map_get_index_of_block (GMetricsAllocationFileMap *file_map,
                                                  GMetricsAllocationBlock   *block)
{
  return block - file_map->blocks;
}
#endif

static gsize
g_metrics_allocation_page_table_get_number_of_pages_for_blocks (GMetricsAllocationPageTable *page_table,
                                                                gsize                        number_of_blocks)
{
  gsize number_of_pages, blocks_per_page;

  blocks_per_page = g_metrics_allocation_page_table_get_number_of_blocks_per_page (page_table);

  number_of_pages = number_of_blocks / blocks_per_page;

  if ((number_of_blocks % blocks_per_page) != 0)
    number_of_pages++;

  return number_of_pages;
}

static void
g_metrics_allocation_file_map_shrink_allocation (GMetricsAllocationFileMap *file_map,
                                                 GMetricsAllocationPage    *pages,
                                                 GMetricsAllocation        *allocation,
                                                 gsize                      number_of_blocks)
{
  GMetricsAllocationPageTable *page_table;
  GMetricsAllocationPage *first_page_in_run;
  GMetricsAllocationBlockStore *block_store;
  GMetricsAllocationHeader *header;
  GMetricsAllocationBlock *first_block;
  gsize blocks_left;
  gsize old_number_of_pages, number_of_pages, number_of_extra_pages;

  page_table = &file_map->page_table;
  block_store = file_map->block_store;
  header = &allocation->header_block.header;
  first_block = (GMetricsAllocationBlock *) allocation;

  first_page_in_run = &pages[0];

  number_of_pages = g_metrics_allocation_page_table_get_number_of_pages_for_blocks (page_table, number_of_blocks);
  old_number_of_pages = first_page_in_run->number_of_pages_in_run;

  if (old_number_of_pages < number_of_pages)
    G_BREAKPOINT ();

  number_of_extra_pages = old_number_of_pages - number_of_pages;
  blocks_left = header->number_of_blocks - number_of_blocks;

  pages->number_of_pages_in_run = number_of_pages;
  header->number_of_blocks = number_of_blocks;

  if (number_of_blocks == 0)
    G_BREAKPOINT ();

  if (old_number_of_pages > 1 && number_of_extra_pages > 0)
    {
      GMetricsAllocationPage *pages_to_deallocate;
      gssize i;

      /* if an allocation spans more than one page, the auxillary pages aren't used
       * for other allocations, so we just deallocate the pages wholesale
       */

      for (i = number_of_pages; i < number_of_extra_pages; i++)
        {
          GMetricsAllocationPage *page;

          page = &pages[i];

          blocks_left -= page->number_of_allocated_blocks;
          block_store->total_bytes_allocated -= page->number_of_allocated_blocks;
        }

      pages_to_deallocate = &pages[number_of_pages];
      pages_to_deallocate->number_of_pages_in_run = number_of_extra_pages;

      g_metrics_allocation_page_table_deallocate_pages (page_table, file_map, pages_to_deallocate, number_of_extra_pages);
    }

  if (blocks_left)
    {
      GMetricsAllocationBlock *next_block, *block_after_next, *last_block;
      GMetricsAllocation *next_allocation;
      GMetricsAllocationHeader *next_header;

      g_metrics_allocation_page_table_decrease_page_allocation (page_table,
                                                                file_map,
                                                                pages,
                                                                blocks_left);

      next_block = first_block + number_of_blocks;

      validate_block_is_in_bounds (file_map, first_page_in_run, next_block);

      next_allocation = (GMetricsAllocation *) next_block;
      next_header = &next_allocation->header_block.header;

      if (blocks_left == 0)
        G_BREAKPOINT ();

      next_header->number_of_blocks = blocks_left;
      next_header->is_allocated = FALSE;
      next_header->previous_block = first_block;

      if (next_header->previous_block != NULL &&
          next_header->number_of_blocks > 32)
        G_BREAKPOINT ();

      last_block = get_last_block_in_page (file_map, first_page_in_run);

      if (next_block <= last_block)
        {
          if (first_page_in_run->run_of_free_blocks == NULL
              || first_page_in_run->run_of_free_blocks->header.number_of_blocks < next_header->number_of_blocks)
            first_page_in_run->run_of_free_blocks = next_block;

          if (next_header->number_of_blocks > (PAGE_SIZE / sizeof (GMetricsAllocationBlock)))
            G_BREAKPOINT ();
        }

      block_store->total_bytes_allocated -= next_header->number_of_blocks * sizeof (GMetricsAllocationBlock);

      block_after_next = next_block + next_header->number_of_blocks;

      if (block_after_next <= last_block)
        {
          GMetricsAllocation *allocation_after_next;
          GMetricsAllocationHeader *header_after_next;

          allocation_after_next = (GMetricsAllocation *) block_after_next;
          header_after_next = &allocation_after_next->header_block.header;
          header_after_next->previous_block = next_block;
        }
    }
}

static GMetricsAllocationPage *
get_pages_for_block (GMetricsAllocationBlockStore *block_store,
                     GMetricsAllocationBlock      *block)
{
  GMetricsAllocationFileMap *file_map;
  GMetricsAllocationPageTable *page_table;
  GMetricsAllocationPage *pages;
  gsize blocks_per_page;
  gsize index_of_block, index_of_page;

  file_map = &block_store->file_map;
  page_table = &file_map->page_table;

  blocks_per_page = g_metrics_allocation_page_table_get_number_of_blocks_per_page (page_table);
  index_of_block = block - file_map->blocks;
  index_of_page = index_of_block / blocks_per_page;

  pages = &page_table->pages[index_of_page];

  validate_block_is_in_bounds (file_map, pages, block);

  return pages;
}

static void
consolidate_consecutive_blocks (GMetricsAllocationFileMap *file_map,
                                GMetricsAllocationPage    *page,
                                GMetricsAllocationBlock   *block,
                                gsize                      blocks_needed)
{
  GMetricsAllocation *allocation = NULL;
  GMetricsAllocationHeader *header;
  GMetricsAllocationFileMapIter look_ahead_iter;
  GMetricsAllocationBlock *look_ahead_block;
  GMetricsAllocationBlock *next_block = NULL;
  GMetricsAllocationBlock *last_block_in_page;

  allocation = (GMetricsAllocation *) block;
  header = &allocation->header_block.header;

  if (header->number_of_blocks >= blocks_needed)
    return;

  last_block_in_page = get_last_block_in_page (file_map, page);

  g_metrics_allocation_file_map_iter_init_after_block (&look_ahead_iter, file_map, page, block);
  while (g_metrics_allocation_file_map_iter_next (&look_ahead_iter, &look_ahead_block))
    {
      GMetricsAllocation *look_ahead_allocation;
      GMetricsAllocationHeader *look_ahead_header;

      if (look_ahead_block > &file_map->blocks[file_map->number_of_blocks - 1])
        G_BREAKPOINT ();

      if (look_ahead_block > last_block_in_page)
        G_BREAKPOINT ();

      look_ahead_allocation = (GMetricsAllocation *) look_ahead_block;
      look_ahead_header = &look_ahead_allocation->header_block.header;

      if (look_ahead_block < block)
        break;

      if (look_ahead_header->is_allocated)
        break;

      if (page->run_of_free_blocks == look_ahead_block)
        page->run_of_free_blocks = NULL;

      header->number_of_blocks += look_ahead_header->number_of_blocks;

      if (metrics_config.validate_allocation_blocks)
        memset (look_ahead_block, 0xaa, MIN (look_ahead_header->number_of_blocks, 4) * sizeof (GMetricsAllocationBlock));

      if (header->number_of_blocks >= blocks_needed)
        break;

      g_metrics_allocation_file_map_iter_init_after_block (&look_ahead_iter, file_map, page, block);
    }

  if (block + header->number_of_blocks < &file_map->blocks[file_map->number_of_blocks])
    {
      next_block = block + header->number_of_blocks;

      if (next_block <= last_block_in_page)
        {
          next_block->header.previous_block = block;
        }
    }
}

static void
g_metrics_allocation_page_table_add_page_to_heap (GMetricsAllocationPageTable *page_table,
                                                  GMetricsAllocationPage      *page)
{
  gsize last_heap_index, new_heap_index;
  gssize page_index;

  page_index = page - page_table->pages;

  if (page->heap_index != -1)
    G_BREAKPOINT ();

  if (page_index >= page_table->number_of_pages)
    G_BREAKPOINT ();

  if (page_table->number_of_pages == 0)
    {
      new_heap_index = 0;
    }
  else
    {
      last_heap_index = page_table->page_indices_heap_size - 1;
      new_heap_index = last_heap_index + 1;
    }

  page_table->page_indices_heap_size++;

  if (page_table->page_indices_heap_size > page_table->number_of_pages)
    G_BREAKPOINT ();

  page_table->page_indices_heap[new_heap_index] = page_index;
  page->heap_index = new_heap_index;

  g_metrics_allocation_page_table_sort_page_with_ancestors (page_table, page->heap_index);
}

static void
g_metrics_allocation_page_table_remove_last_page_from_heap (GMetricsAllocationPageTable *page_table)
{
  GMetricsAllocationPage *page;
  gsize last_heap_index;
  gssize page_index;

  if (page_table->page_indices_heap_size == 0)
    G_BREAKPOINT ();

  last_heap_index = page_table->page_indices_heap_size - 1;

  page_index = page_table->page_indices_heap[last_heap_index];
  page = &page_table->pages[page_index];

  page_table->page_indices_heap_size--;
  page->heap_index = -1;
}

static void
g_metrics_allocation_page_table_remove_page_from_heap (GMetricsAllocationPageTable *page_table,
                                                       GMetricsAllocationPage      *page)
{
  gsize last_heap_index;
  gssize page_index;

  page_index = page - page_table->pages;

  if (page->heap_index == -1)
    G_BREAKPOINT ();

  if (page_index >= page_table->number_of_pages)
    G_BREAKPOINT ();

  if (page_table->page_indices_heap_size == 0)
    G_BREAKPOINT ();

  last_heap_index = page_table->page_indices_heap_size - 1;

  if (page_table->page_indices_heap_size > 1 &&
      page->heap_index != last_heap_index)
    {
      GMetricsAllocationPage *misfiled_page;
      gssize misfiled_page_index, misfiled_heap_index;

      misfiled_page_index = page_table->page_indices_heap[last_heap_index];
      misfiled_page = &page_table->pages[misfiled_page_index];

      /* Move the page to get removed to the end of the heap so the heap can be truncated.
       * To make room for it at the end, swap the page that's already there to the defunct page's
       * old position, then sort it back to the bottom, so all the intermediate pages slide up.
       */
      g_metrics_allocation_page_table_swap_pages_on_heap (page_table, page, misfiled_page);

      g_metrics_allocation_page_table_remove_last_page_from_heap (page_table);

      misfiled_heap_index = misfiled_page->heap_index;
      g_metrics_allocation_page_table_sort_page_with_children (page_table, misfiled_heap_index);
      g_metrics_allocation_page_table_sort_page_with_ancestors (page_table, misfiled_heap_index);

      if (page->heap_index != -1)
        G_BREAKPOINT ();
    }
  else
    {
      g_metrics_allocation_page_table_remove_last_page_from_heap (page_table);

      if (page->heap_index != -1)
        G_BREAKPOINT ();
    }
}

static void
g_metrics_allocation_page_table_truncate_empty_pages (GMetricsAllocationPageTable *page_table)
{
  while (page_table->number_of_pages > 0)
    {
      GMetricsAllocationPage *last_page_in_table = &page_table->pages[page_table->number_of_pages - 1];

      if (last_page_in_table->number_of_allocated_blocks != 0)
        break;

      if (last_page_in_table->heap_index != -1)
        g_metrics_allocation_page_table_remove_page_from_heap (page_table, last_page_in_table);

      page_table->number_of_pages--;
    }
}

static GMetricsAllocationPage *
g_metrics_allocation_page_table_deallocate_pages (GMetricsAllocationPageTable *page_table,
                                                  GMetricsAllocationFileMap   *file_map,
                                                  GMetricsAllocationPage      *pages,
                                                  gsize                        number_of_pages)
{
  GMetricsAllocationPage *first_page_in_run, *last_page_in_table;
  GMetricsAllocationBlock *block;
  gsize blocks_per_page;
  gssize i;

  first_page_in_run = &pages[0];
  blocks_per_page = g_metrics_allocation_page_table_get_number_of_blocks_per_page (page_table);

  if (first_page_in_run->heap_index != -1)
    g_metrics_allocation_page_table_remove_page_from_heap (page_table, first_page_in_run);

  if (number_of_pages > first_page_in_run->number_of_pages_in_run)
    G_BREAKPOINT ();

  block = get_first_block_in_page (file_map, first_page_in_run);

  block->header.number_of_blocks = blocks_per_page * number_of_pages;
  block->header.is_allocated = FALSE;
  block->header.previous_block = NULL;
  block->header.name[0] = 'X';

  if (metrics_config.validate_allocation_blocks)
    memset (block + 1, 0xbb, (blocks_per_page - 1) * sizeof (GMetricsAllocationBlock));

  first_page_in_run->run_of_free_blocks = block;

  for (i = 0; i < number_of_pages; i++)
    {
      GMetricsAllocationPage *page = &pages[i];

      page->number_of_allocated_blocks = 0;

      if (i != 0)
        {
          if (page->number_of_pages_in_run != -1)
            G_BREAKPOINT ();
        }
    }

  g_metrics_allocation_page_table_truncate_empty_pages (page_table);

  last_page_in_table = &page_table->pages[page_table->number_of_pages - 1];

  if (metrics_config.give_back_deallocated_pages)
    {
      int advice = MADV_NORMAL;
      void *address_of_page = block;

      if (metrics_config.give_back_deallocated_pages == 1)
        advice = MADV_REMOVE;
      else if (metrics_config.give_back_deallocated_pages == 2)
        advice = MADV_DONTNEED;

      madvise (address_of_page, PAGE_SIZE * number_of_pages, advice);
    }

  if (first_page_in_run <= last_page_in_table)
    g_metrics_allocation_page_table_add_page_to_heap (page_table, first_page_in_run);

  return pages;
}

static void
g_metrics_allocation_page_table_increase_page_allocation (GMetricsAllocationPageTable *page_table,
                                                          GMetricsAllocationFileMap   *file_map,
                                                          GMetricsAllocationPage      *pages,
                                                          gsize                        number_of_blocks)
{
  GMetricsAllocationPage *first_page_in_run;
  gsize blocks_per_page;
  gsize i;

  if (number_of_blocks == 0)
    return;

  blocks_per_page = g_metrics_allocation_page_table_get_number_of_blocks_per_page (page_table);

  first_page_in_run = &pages[0];

  if (first_page_in_run->heap_index != -1)
    g_metrics_allocation_page_table_remove_page_from_heap (page_table, first_page_in_run);

  for (i = 0; i < pages->number_of_pages_in_run && number_of_blocks > 0; i++)
    {
      GMetricsAllocationPage *page = &pages[i];
      gsize blocks_to_allocate;

      blocks_to_allocate = MIN (number_of_blocks, blocks_per_page);

      page->number_of_allocated_blocks += blocks_to_allocate;
      number_of_blocks -= blocks_to_allocate;
    }

  if (pages->number_of_pages_in_run == 1 &&
      first_page_in_run->number_of_allocated_blocks <= metrics_config.allocated_blocks_threshold)
    {
      g_metrics_allocation_page_table_add_page_to_heap (page_table, first_page_in_run);
    }
}

#if 0
static void
validate_page_allocation (GMetricsAllocationPageTable *page_table,
                          GMetricsAllocationFileMap   *file_map,
                          GMetricsAllocationPage      *page)
{
  GMetricsAllocationFileMapIter iter;
  GMetricsAllocationBlock *block;
  gsize blocks_per_page;
  gsize allocation = 0;

  if (page->number_of_allocated_blocks == 0)
    return;

  blocks_per_page = g_metrics_allocation_page_table_get_number_of_blocks_per_page (page_table);

  g_metrics_allocation_file_map_iter_init_at_block (&iter, file_map, page, NULL);
  while (g_metrics_allocation_file_map_iter_next (&iter, &block))
    {
      if (block->header.is_allocated)
        allocation += MIN (block->header.number_of_blocks, blocks_per_page);

      if (allocation == page->number_of_allocated_blocks)
        break;
    }

  if (allocation != page->number_of_allocated_blocks)
    G_BREAKPOINT ();
}

static void
validate_blocks_in_page (GMetricsAllocationPageTable *page_table,
                         GMetricsAllocationFileMap   *file_map,
                         GMetricsAllocationPage      *page)
{
  GMetricsAllocationFileMapIter iter;
  GMetricsAllocationBlock *block;

  g_metrics_allocation_file_map_iter_init_at_block (&iter, file_map, page, NULL);
  while (g_metrics_allocation_file_map_iter_next (&iter, &block))
    {
      g_metrics_allocation_file_map_validate_block (file_map, page, block);
    }

  validate_page_allocation (page_table, file_map, page);
}
#endif

static void
g_metrics_allocation_page_table_decrease_page_allocation (GMetricsAllocationPageTable *page_table,
                                                          GMetricsAllocationFileMap   *file_map,
                                                          GMetricsAllocationPage      *pages,
                                                          gsize                        number_of_blocks)
{
  GMetricsAllocationPage *first_page_in_run, *last_page_in_table;
  GMetricsAllocationPage *pages_to_deallocate = NULL;
  gsize i;

  if (number_of_blocks == 0)
    return;

  first_page_in_run = &pages[0];

  if (first_page_in_run->heap_index != -1)
    g_metrics_allocation_page_table_remove_page_from_heap (page_table, first_page_in_run);

  for (i = pages->number_of_pages_in_run - 1; i >= 0 && number_of_blocks > 0; i--)
    {
      GMetricsAllocationPage *page = &pages[i];
      gsize blocks_to_deallocate;

      blocks_to_deallocate = MIN (number_of_blocks, page->number_of_allocated_blocks);

      page->number_of_allocated_blocks -= blocks_to_deallocate;
      number_of_blocks -= blocks_to_deallocate;

      if (page->number_of_allocated_blocks == 0)
        pages_to_deallocate = page;
    }

  if (pages_to_deallocate != NULL)
    {
      gsize number_of_pages_to_keep_allocated, number_of_pages_to_deallocate;

      number_of_pages_to_keep_allocated = pages_to_deallocate - pages;
      number_of_pages_to_deallocate = pages->number_of_pages_in_run - number_of_pages_to_keep_allocated;

      pages->number_of_pages_in_run = number_of_pages_to_keep_allocated;
      pages_to_deallocate->number_of_pages_in_run = number_of_pages_to_deallocate;

      g_metrics_allocation_page_table_deallocate_pages (page_table,
                                                        file_map,
                                                        pages_to_deallocate,
                                                        pages_to_deallocate->number_of_pages_in_run);
    }

  last_page_in_table = &page_table->pages[page_table->number_of_pages - 1];
  if (pages->number_of_pages_in_run == 1 &&
      first_page_in_run <= last_page_in_table &&
      first_page_in_run->number_of_allocated_blocks <= metrics_config.allocated_blocks_threshold &&
      first_page_in_run->heap_index == -1)
    {
      g_metrics_allocation_page_table_add_page_to_heap (page_table, first_page_in_run);
    }
}

#if 0
static void
g_metrics_allocation_page_table_set_page_allocation (GMetricsAllocationPageTable *page_table,
                                                     GMetricsAllocationFileMap   *file_map,
                                                     GMetricsAllocationPage      *page,
                                                     gsize                        number_of_blocks)
{
  gsize old_number_of_blocks;

  old_number_of_blocks = page->number_of_allocated_blocks;

  if (old_number_of_blocks == number_of_blocks)
    return;

  if (number_of_blocks > old_number_of_blocks)
    g_metrics_allocation_page_table_increase_page_allocation (page_table, file_map, page, number_of_blocks - old_number_of_blocks);
  else
    g_metrics_allocation_page_table_decrease_page_allocation (page_table, file_map, page, old_number_of_blocks - number_of_blocks);
}
#endif

static GMetricsAllocationBlock *
find_run_of_free_blocks (GMetricsAllocationPageTable *page_table,
                         GMetricsAllocationFileMap   *file_map,
                         GMetricsAllocationPage      *page)
{
  GMetricsAllocationFileMapIter iter;
  GMetricsAllocationBlock *block, *biggest_block = NULL;
  GMetricsAllocationBlock *first_block_in_page, *last_block_in_page;
  gsize blocks_per_page;

  if (page->number_of_pages_in_run == -1)
    G_BREAKPOINT ();

  blocks_per_page = g_metrics_allocation_page_table_get_number_of_blocks_per_page (page_table);

  if (page->number_of_allocated_blocks == blocks_per_page)
    return NULL;

  first_block_in_page = get_first_block_in_page (file_map, page);
  last_block_in_page = get_last_block_in_page (file_map, page);

  g_metrics_allocation_file_map_iter_init_at_block (&iter, file_map, page, first_block_in_page);
  while (g_metrics_allocation_file_map_iter_next (&iter, &block))
    {
      GMetricsAllocation *allocation;
      GMetricsAllocationHeader *header;

      if (block > &file_map->blocks[file_map->number_of_blocks - 1])
        G_BREAKPOINT ();

      if (block > last_block_in_page)
        G_BREAKPOINT ();

      allocation = (GMetricsAllocation *) block;
      header = &allocation->header_block.header;

      if (block < first_block_in_page)
        break;

      if (header->is_allocated)
        continue;

      if (header->number_of_blocks == 0)
        G_BREAKPOINT ();

      consolidate_consecutive_blocks (file_map, page, block, last_block_in_page - block);

      if (biggest_block == NULL || biggest_block->header.number_of_blocks < header->number_of_blocks)
        biggest_block = block;
    }

  return biggest_block;
}

static void
g_metrics_allocation_file_map_claim_allocation (GMetricsAllocationFileMap *file_map,
                                                GMetricsAllocationPage    *pages,
                                                GMetricsAllocation        *allocation)
{
  GMetricsAllocationPageTable *page_table;
  GMetricsAllocationPage *first_page_in_run;
  GMetricsAllocationBlock *block;
  GMetricsAllocationBlockStore *block_store;
  GMetricsAllocationHeader *header;
  gsize blocks_per_page;

  page_table = &file_map->page_table;
  block_store = file_map->block_store;
  header = &allocation->header_block.header;

  header->is_allocated = TRUE;
  block_store->total_bytes_allocated += header->number_of_blocks * sizeof (GMetricsAllocationBlock);
  block_store->number_of_allocations++;

  g_metrics_allocation_page_table_increase_page_allocation (page_table, file_map, pages, header->number_of_blocks);

  first_page_in_run = &pages[0];
  block = (GMetricsAllocationBlock *) allocation;

  if (first_page_in_run->run_of_free_blocks == block)
    first_page_in_run->run_of_free_blocks = find_run_of_free_blocks (page_table, file_map, first_page_in_run);

  blocks_per_page = g_metrics_allocation_page_table_get_number_of_blocks_per_page (page_table);
  if (g_metrics_allocation_page_has_free_blocks (first_page_in_run) && first_page_in_run->number_of_allocated_blocks == blocks_per_page)
    G_BREAKPOINT ();
}

static gboolean
g_metrics_allocation_file_map_grow_allocation (GMetricsAllocationFileMap *file_map,
                                               GMetricsAllocationPage    *page,
                                               GMetricsAllocation        *allocation,
                                               gsize                      number_of_blocks)
{
  GMetricsAllocationHeader *header;
  GMetricsAllocationBlock *first_block;
  GMetricsAllocationBlockStore *block_store;
  GMetricsAllocationPageTable *page_table;
  gsize blocks_per_page, blocks_to_allocate;
  gsize old_size, old_number_of_blocks;

  block_store = file_map->block_store;
  page_table = &file_map->page_table;

  header = &allocation->header_block.header;
  first_block = (GMetricsAllocationBlock *) allocation;
  blocks_per_page = g_metrics_allocation_page_table_get_number_of_blocks_per_page (page_table);

  old_number_of_blocks = header->number_of_blocks;
  old_size = header->number_of_blocks * sizeof (GMetricsAllocationBlock);
  consolidate_consecutive_blocks (file_map, page, first_block, number_of_blocks);

  block_store->total_bytes_allocated += header->number_of_blocks * sizeof (GMetricsAllocationBlock);
  block_store->total_bytes_allocated -= old_size;

  blocks_to_allocate = header->number_of_blocks - old_number_of_blocks;

  g_metrics_allocation_page_table_increase_page_allocation (page_table, file_map, page, blocks_to_allocate);

  if (header->number_of_blocks > number_of_blocks)
    g_metrics_allocation_file_map_shrink_allocation (file_map, page, allocation, number_of_blocks);

  if (page->run_of_free_blocks == NULL)
    page->run_of_free_blocks = find_run_of_free_blocks (page_table, file_map, page);

  if (g_metrics_allocation_page_has_free_blocks (page) && page->number_of_allocated_blocks == blocks_per_page)
    G_BREAKPOINT ();

  g_metrics_allocation_file_map_validate_block (file_map, page, first_block);

  return header->number_of_blocks == number_of_blocks;
}

static void
g_metrics_allocation_file_map_release_allocation (GMetricsAllocationFileMap *file_map,
                                                  GMetricsAllocationPage    *pages,
                                                  GMetricsAllocation        *allocation)
{
  GMetricsAllocationBlockStore *block_store;
  GMetricsAllocationPageTable *page_table;
  GMetricsAllocationPage *first_page_in_run;
  GMetricsAllocationHeader *header;
  GMetricsAllocationBlock *block, *previous_block;
  gsize blocks_per_page;
  gsize number_of_blocks;

  block_store = file_map->block_store;
  page_table = &file_map->page_table;
  first_page_in_run = &pages[0];

  header = &allocation->header_block.header;
  block = (GMetricsAllocationBlock *) allocation;
  number_of_blocks = header->number_of_blocks;

  if (!header->is_allocated)
    G_BREAKPOINT ();

  if (block->header.previous_block != NULL)
    g_metrics_allocation_file_map_validate_block (file_map, pages, block->header.previous_block);

  g_metrics_allocation_file_map_validate_block (file_map, pages, block);

  header->is_allocated = FALSE;

  blocks_per_page = g_metrics_allocation_page_table_get_number_of_blocks_per_page (page_table);

  if (number_of_blocks > blocks_per_page)
    {
      if (block->header.previous_block)
        G_BREAKPOINT ();

      g_metrics_allocation_page_table_deallocate_pages (page_table, file_map, pages, first_page_in_run->number_of_pages_in_run);
    }
  else
    {
      if (first_page_in_run->run_of_free_blocks == NULL
          || first_page_in_run->run_of_free_blocks->header.number_of_blocks < number_of_blocks)
        first_page_in_run->run_of_free_blocks = block;

      g_metrics_allocation_page_table_decrease_page_allocation (page_table, file_map, pages, number_of_blocks);
    }

  block_store->total_bytes_allocated -= number_of_blocks * sizeof (GMetricsAllocationBlock);

  if (first_page_in_run->number_of_allocated_blocks > 0)
    {
      g_metrics_allocation_file_map_validate_block (file_map, pages, block);

      if (block->header.previous_block != NULL)
        {
          previous_block = (GMetricsAllocationBlock *) block->header.previous_block;

          if (previous_block->header.number_of_blocks + number_of_blocks > blocks_per_page)
            G_BREAKPOINT ();

          g_metrics_allocation_file_map_validate_block (file_map, pages, previous_block);

          if (!previous_block->header.is_allocated)
            {
              consolidate_consecutive_blocks (file_map,
                                              first_page_in_run,
                                              previous_block,
                                              previous_block->header.number_of_blocks + number_of_blocks);

              if (first_page_in_run->run_of_free_blocks == NULL
                  || first_page_in_run->run_of_free_blocks->header.number_of_blocks < previous_block->header.number_of_blocks)
                first_page_in_run->run_of_free_blocks = previous_block;
            }

          g_metrics_allocation_file_map_validate_block (file_map, pages, previous_block);
        }
    }

  validate_heap (page_table, 0, NULL);

  block_store->number_of_allocations--;
}

static gboolean
g_metrics_allocation_block_store_init (GMetricsAllocationBlockStore *block_store,
                                       const char                   *name,
                                       gsize                         size)
{
  gboolean file_map_mapped;

  strncpy (block_store->name, name, sizeof (block_store->name) - 1);

  block_store->file_map.block_store = block_store;
  block_store->file_map.map_address = MAP_FAILED;
  block_store->file_map.page_table.map_address = MAP_FAILED;

  file_map_mapped = g_metrics_allocation_file_map_open (&block_store->file_map, size);

  return file_map_mapped;
}

void
g_metrics_allocation_block_store_free (GMetricsAllocationBlockStore *block_store)
{
  G_LOCK (allocation_block_stores);
  g_metrics_allocation_file_map_close (&block_store->file_map);
  g_metrics_list_remove_item (&allocation_block_stores, block_store);
  g_metrics_stack_trace_free (block_store->stack_trace);
  g_metrics_allocation_block_store_deallocate (&store_for_allocation_block_stores,
                                               block_store);
  G_UNLOCK (allocation_block_stores);
}

GMetricsAllocationBlockStore *
g_metrics_allocation_block_store_new (const char *name,
                                      gsize       size)
{
  GMetricsAllocationBlockStore *block_store;
  gboolean initialized;
  char thread_name[32] = "thread-";
  pid_t current_thread_id;

  G_LOCK(allocation_block_stores);

  block_store = g_metrics_allocation_block_store_allocate_with_name (&store_for_allocation_block_stores,
                                                                     sizeof (GMetricsAllocationBlockStore),
                                                                     "GMetricsAllocationBlockStore");
  G_UNLOCK (allocation_block_stores);

  if (block_store == NULL)
    return NULL;

  current_thread_id = (pid_t) syscall (SYS_gettid);
  int_to_string (current_thread_id, thread_name + strlen (thread_name), sizeof (thread_name) - strlen (thread_name));

  if (name == NULL)
    name = thread_name;

  memset (block_store, 0, sizeof (GMetricsAllocationBlockStore));

  strncpy (block_store->thread_name, thread_name, sizeof (block_store->thread_name) - 1);

  initialized = g_metrics_allocation_block_store_init (block_store, name, size);

  if (!initialized)
    {
      G_BREAKPOINT ();
      return NULL;
    }

  G_LOCK(allocation_block_stores);
  if (metrics_allocation_block_store)
    g_metrics_list_add_item (&allocation_block_stores, block_store);
  G_UNLOCK (allocation_block_stores);

  return block_store;
}

static gboolean
g_metrics_allocation_block_store_has_allocation (GMetricsAllocationBlockStore *block_store,
                                                 gpointer                       allocation)
{
  return g_metrics_allocation_file_map_has_allocation (&block_store->file_map, allocation);
}

static void
initialize_store_for_allocation_block_stores (void)
{
  gboolean initialized;

  initialized = g_metrics_allocation_block_store_init (&store_for_allocation_block_stores,
                                                       "allocation-block-stores",
                                                       metrics_config.allocation_block_store_size);

  if (!initialized)
    {
      G_BREAKPOINT ();
      return;
    }
}

static void
allocate_metrics_block_store (void)
{
  GMetricsAllocationBlockStore *block_store;

  block_store = g_metrics_allocation_block_store_new ("metrics",
                                                      metrics_config.allocation_block_store_size);

  if (block_store == NULL)
    return;

  G_LOCK (allocation_block_stores);
  metrics_allocation_block_store = block_store;
  g_metrics_list_add_item (&allocation_block_stores, metrics_allocation_block_store);
  G_UNLOCK (allocation_block_stores);
}

static void
allocate_thread_default_block_store (void)
{
  GMetricsAllocationBlockStore *block_store;

  block_store = g_metrics_allocation_block_store_new (NULL, metrics_config.allocation_block_store_size);
  block_store->is_thread_default = TRUE;
  g_metrics_push_default_allocation_block_store (block_store);
}

static void
g_metrics_allocation_block_store_iter_init (GMetricsAllocationBlockStoreIter *iter)
{
  g_metrics_list_iter_init (&iter->list_iter, &allocation_block_stores);
}

static gboolean
g_metrics_allocation_block_store_iter_next (GMetricsAllocationBlockStoreIter  *iter,
                                            GMetricsAllocationBlockStore     **block_store)
{
  return g_metrics_list_iter_next (&iter->list_iter, block_store);
}

static void
write_allocation_list (GMetricsAllocationBlockStore *block_store)
{
  GMetricsAllocationFileMap *file_map;
  GMetricsAllocationPageTable *page_table;
  GMetricsAllocationFileMapIter iter;
  GMetricsAllocationBlock *block;
  int fd = -1;
  gsize page_index;

  file_map = &block_store->file_map;
  page_table = &file_map->page_table;

  page_index = 0;
  while (page_index < page_table->number_of_pages)
    {
      GMetricsAllocationPage *page;

      page = &page_table->pages[page_index];

      if (page->number_of_allocated_blocks == 0)
        {
          page_index++;
          continue;
        }

      if (fd == -1)
        {
          char filename[1024] = { 0 };
          strncat (filename, metrics_config.log_dir, sizeof (filename) - 1);
          strncat (filename, "/", sizeof (filename) - 1);
          strncat (filename, block_store->name, sizeof (filename) - 1);
          strncat (filename, "-allocations.list", sizeof (filename) - 1);
          fd = open (filename, O_CREAT | O_TRUNC | O_RDWR, 0644);
        }

#if 0
      if (page->heap_index != -1)
        write (fd, "[non-heap page] {\n", strlen ("[non-heap page] {\n"));
      else
        write (fd, "[heap page] {\n", strlen ("[heap page] {\n"));
#endif

      g_metrics_allocation_file_map_iter_init_at_block (&iter, file_map, page, NULL);
      while (g_metrics_allocation_file_map_iter_next (&iter, &block))
        {
          GMetricsAllocation *allocation;
          GMetricsAllocationHeader *header;

          allocation = (GMetricsAllocation *) block;
          header = (GMetricsAllocationHeader *) &allocation->header_block.header;

          if (!header->is_allocated)
            {
#if 0
              write (fd, "[freed] ", strlen ("[freed] "));
#endif
              continue;
            }

          if (header->name[0] != '\0')
            {
              write (fd, header->name, strlen (header->name));
              write (fd, "\n", 1);
            }
          else
            {
              write (fd, "(unknown)\n", strlen("(unknown)\n"));
            }
        }
#if 0
      write (fd, "}\n", 2);
#endif

      if (page->number_of_pages_in_run > 0)
        page_index += page->number_of_pages_in_run;
      else
        page_index++;

    }

    if (fd != -1)
    close (fd);
}

static void
on_write_allocation_list_timeout (void)
{
  G_LOCK (allocation_block_stores);

  G_LOCK (allocations);
  write_allocation_list (metrics_allocation_block_store);
  G_UNLOCK (allocations);

  G_UNLOCK (allocation_block_stores);
}
static void
on_allocation_block_stores_metrics_timeout (void)
{
  GMetricsAllocationBlockStoreIter iter;
  GMetricsAllocationBlockStore *block_store = NULL;

  if (!allocation_block_store_metrics_file) {
    return;
  }

  G_LOCK (allocation_block_stores);

  if (!g_metrics_requested ("allocation-block-stores")) {
    G_UNLOCK (allocation_block_stores);
    return;
  }

  g_metrics_file_start_record (allocation_block_store_metrics_file);
  g_metrics_allocation_block_store_iter_init (&iter);
  while (g_metrics_allocation_block_store_iter_next (&iter, &block_store))
    {
       const char *stack_trace = NULL;
       GMetricsAllocationFileMap *file_map;

       file_map = &block_store->file_map;

       if (!g_metrics_allocation_file_map_is_open (file_map))
         continue;

       if (block_store->stack_trace != NULL)
         stack_trace = g_metrics_stack_trace_get_output (block_store->stack_trace);

       g_metrics_file_add_row (allocation_block_store_metrics_file,
                               block_store->name,
                               block_store->thread_name,
                               block_store->number_of_allocations,
                               block_store->total_bytes_allocated,
                               stack_trace?: "");
    }
  g_metrics_file_end_record (allocation_block_store_metrics_file);
  G_UNLOCK (allocation_block_stores);
}

static gsize
get_int_from_environment (const char *variable,
                          gsize       default_value)
{
  const char *value;

  value = getenv (variable);

  if (value == NULL)
    return default_value;

  return strtol (value, NULL, 10);
}

static void
load_metrics_config_command (void)
{
  static char cmdline[1024] = { 0 };
  const char *requested_command;
  const char *requested_user, *current_user;
  int fd;

  fd = open ("/proc/self/cmdline", O_RDONLY);
  if (fd >= 0)
    {
      read (fd, cmdline, 1023);
      close (fd);
    }

  current_user = getenv ("USER");
  requested_user = getenv ("G_METRICS_USER");
  requested_command = getenv ("G_METRICS_COMMAND")? : "gnome-shell";

  if (g_strcmp0 (requested_command, "*") == 0)
    requested_command = cmdline;

  metrics_config.metrics_enabled = g_str_has_suffix (cmdline, requested_command) &&
                                   g_strcmp0 (current_user, requested_user) == 0;

  if (metrics_config.metrics_enabled)
    {
      int delay;

      delay = atoi (getenv ("G_METRICS_DELAY")? : "0");

      if (delay != 0)
        sleep (delay);
    }
}

static void
load_metrics_allocation_config (void)
{
  const char *allocation_map_dir;

  allocation_map_dir = getenv ("G_METRICS_ALLOCATION_MAP_DIR");

  if (allocation_map_dir == NULL)
    allocation_map_dir = "/var/tmp";

  strncpy (metrics_config.allocation_map_dir, allocation_map_dir, sizeof (metrics_config.allocation_map_dir));

  metrics_config.max_allocation_block_stores = get_int_from_environment ("G_METRICS_MAX_ALLOCATION_BLOCK_STORES", 256);
  metrics_config.allocation_block_store_size = get_int_from_environment ("G_METRICS_DEFAULT_ALLOCATION_BLOCK_STORE_SIZE", 10485760) * 1024L;
  metrics_config.dedicated_allocation_block_store_threshold = get_int_from_environment ("G_METRICS_DEDICATED_ALLOCATION_BLOCK_STORE_THRESHOLD", 8192);
  metrics_config.dedicated_allocation_block_store_threshold = MIN (metrics_config.dedicated_allocation_block_store_threshold, PAGE_SIZE);
  metrics_config.allocated_blocks_threshold = get_int_from_environment ("G_METRICS_ALLOCATED_BLOCKS_THRESHOLD", 28);

  metrics_config.override_system_malloc = get_int_from_environment ("G_METRICS_OVERRIDE_SYSTEM_MALLOC", 0);
  metrics_config.override_glib_malloc = get_int_from_environment ("G_METRICS_OVERRIDE_GLIB_MALLOC", 0);
  metrics_config.validate_allocation_blocks = get_int_from_environment ("G_METRICS_VALIDATE_ALLOCATION_BLOCKS", 0);
  metrics_config.validate_heap = get_int_from_environment ("G_METRICS_VALIDATE_HEAP", 0);
  metrics_config.use_map_files = get_int_from_environment ("G_METRICS_USE_MAP_FILES", 1);
  metrics_config.give_back_deallocated_pages = get_int_from_environment ("G_METRICS_GIVE_BACK_DEALLOCATED_PAGES", 1);
}

static void
load_metrics_logging_config (void)
{
  const char *log_dir;

  log_dir = getenv ("G_METRICS_LOG_DIR");

  if (log_dir != NULL)
    {
      strncpy (metrics_config.log_dir, log_dir, sizeof (metrics_config.log_dir) - 1);
    }
  else
    {
      const char *cache_dir;
      char process_id[32] = "";

      cache_dir = getenv ("XDG_CACHE_HOME");

      if (cache_dir != NULL)
        {
          strncat (metrics_config.log_dir, cache_dir, sizeof (metrics_config.log_dir) - 1);
        }
      else
        {
          strncat (metrics_config.log_dir, getenv ("HOME"), sizeof (metrics_config.log_dir) - 1);
          strncat (metrics_config.log_dir, "/.cache", sizeof (metrics_config.log_dir) - 1);
        }
      strncat (metrics_config.log_dir, "/metrics/", sizeof (metrics_config.log_dir) - 1);

       int_to_string (getpid (), process_id, sizeof (process_id));

      strncat (metrics_config.log_dir, process_id, sizeof (metrics_config.log_dir) - 1);
    }
}

static void
load_metrics_inclusions_config (void)
{
  const char *included_metrics;

  included_metrics = getenv ("G_METRICS_INCLUDE");

  if (included_metrics != NULL)
    strncpy (metrics_config.included_metrics,
             included_metrics,
             sizeof (metrics_config.included_metrics) - 1);
}

static void
load_metrics_exclusions_config (void)
{
  const char *skipped_metrics;

  skipped_metrics = getenv ("G_METRICS_SKIP");

  if (skipped_metrics != NULL)
    strncpy (metrics_config.skipped_metrics,
             skipped_metrics,
             sizeof (metrics_config.skipped_metrics) - 1);
  else
    strncpy (metrics_config.skipped_metrics,
             default_skipped_metrics,
             sizeof (metrics_config.skipped_metrics) - 1);
}

static void
load_metrics_collection_config (void)
{
  const char *collection_ignore_list, *collection_include_list;

  metrics_config.collection_interval = get_int_from_environment ("G_METRICS_COLLECTION_INTERVAL", 10);
  metrics_config.generations_to_settle = get_int_from_environment ("G_METRICS_COLLECTION_NUMBER_OF_PRELOAD_INVERVALS", 10);
  metrics_config.generations_to_reset_average_window = get_int_from_environment ("G_METRICS_COLLECTION_AVERAGE_WINDOW_THRESHOLD", 10);
  metrics_config.number_of_interesting_instances = get_int_from_environment ("G_METRICS_COLLECTION_INSTANCE_COUNT", 10);
  metrics_config.stack_trace_sample_interval = get_int_from_environment ("G_METRICS_STACK_TRACE_SAMPLE_INTERVAL", 1);

  collection_ignore_list = getenv ("G_METRICS_COLLECTION_INSTANCE_IGNORE_LIST");

  if (collection_ignore_list != NULL)
    strncpy (metrics_config.collection_ignore_list,
             collection_ignore_list,
             sizeof (metrics_config.collection_ignore_list) - 1);
  else
    strncpy (metrics_config.collection_ignore_list,
             default_collection_ignore_list,
             sizeof (metrics_config.collection_ignore_list) - 1);

  collection_include_list = getenv ("G_METRICS_COLLECTION_INSTANCE_INCLUDE_LIST");

  if (collection_include_list != NULL)
    strncpy (metrics_config.collection_include_list,
             collection_include_list,
             sizeof (metrics_config.collection_include_list) - 1);
  else
    strncpy (metrics_config.collection_include_list,
             default_collection_ignore_list,
             sizeof (metrics_config.collection_include_list) - 1);
}

static void
load_metrics_stack_trace_config (void)
{
  metrics_config.stack_trace_size = get_int_from_environment ("G_METRICS_STACK_TRACE_SIZE", 15);

  metrics_config.stack_trace_annotation_size = get_int_from_environment ("G_METRICS_STACK_TRACE_ANNOTATION_SIZE", 512);
}

static void
load_metrics_config (void)
{
  load_metrics_config_command ();

  if (!metrics_config.metrics_enabled)
    return;

  load_metrics_allocation_config ();
  load_metrics_logging_config ();
  load_metrics_inclusions_config ();
  load_metrics_exclusions_config ();
  load_metrics_collection_config ();
  load_metrics_stack_trace_config ();
}

void
g_metrics_init (void)
{
  static gboolean initialized = 0;

  if (initialized)
    return;

  load_metrics_config ();

  if (!g_metrics_enabled ())
    {
      initialized = TRUE;
      return;
    }

  initialize_store_for_allocation_block_stores ();
  allocate_metrics_block_store ();
  allocate_thread_default_block_store ();

  initialized = TRUE;

  G_LOCK (timeouts);
  if (timeout_handlers == NULL)
    timeout_handlers = g_metrics_list_new ();
  G_UNLOCK (timeouts);
}

static GMetricsAllocationBlock *
get_next_block (GMetricsAllocationFileMap *file_map,
                GMetricsAllocationPage    *page,
                GMetricsAllocationBlock   *block)
{
  GMetricsAllocation *allocation;
  GMetricsAllocationHeader *header;
  GMetricsAllocationBlock *first_block, *last_block, *next_block;

  if (page != NULL)
    {
      first_block = get_first_block_in_page (file_map, page);
      last_block = get_last_block_in_page (file_map, page);
    }
  else
    {
      first_block = &file_map->blocks[0];
      last_block = &file_map->blocks[file_map->number_of_blocks - 1];
    }

  allocation = (GMetricsAllocation *) block;
  header = (GMetricsAllocationHeader *) &allocation->header_block.header;

  if ((block + header->number_of_blocks) > last_block)
    next_block = first_block;
  else
    next_block = block + header->number_of_blocks;

  return next_block;
}

static void
g_metrics_allocation_file_map_iter_init_at_block (GMetricsAllocationFileMapIter *iter,
                                                  GMetricsAllocationFileMap     *file_map,
                                                  GMetricsAllocationPage        *page,
                                                  GMetricsAllocationBlock       *block)
{
  iter->file_map = file_map;
  iter->page = page;

  if (block != NULL)
    iter->starting_block = block;
  else if (page != NULL)
    iter->starting_block = get_first_block_in_page (file_map, page);
  else
    iter->starting_block = &file_map->blocks[0];

  validate_block_is_in_bounds (file_map, page, iter->starting_block);

  iter->previous_block = NULL;
  iter->items_examined = 0;
}

static void
g_metrics_allocation_file_map_iter_init_after_block (GMetricsAllocationFileMapIter *iter,
                                                     GMetricsAllocationFileMap     *file_map,
                                                     GMetricsAllocationPage        *page,
                                                     GMetricsAllocationBlock       *block)
{
  iter->file_map = file_map;
  iter->page = page;

  if (block != NULL)
    {
      validate_block_is_in_bounds (file_map, page, block);

      iter->starting_block = get_next_block (file_map, page, block);
    }
  else
    {
      iter->starting_block = &file_map->blocks[0];
    }

  iter->previous_block = NULL;
  iter->items_examined = 0;
}

static gboolean
g_metrics_allocation_file_map_iter_next (GMetricsAllocationFileMapIter  *iter,
                                         GMetricsAllocationBlock       **next_block)
{
  GMetricsAllocationFileMap *file_map;
  GMetricsAllocationBlock *block;
  GMetricsAllocationPage *page;

  file_map = iter->file_map;
  page = iter->page;

  if (iter->previous_block == NULL)
    block = iter->starting_block;
  else
    block = get_next_block (file_map, page, iter->previous_block);

  if (block->header.number_of_blocks == 0)
    {
      if (next_block)
        *next_block = NULL;
      return FALSE;
    }

  if (block == iter->starting_block && iter->items_examined > 1)
    {
      if (next_block)
        *next_block = NULL;
      return FALSE;
    }

  if (next_block)
    *next_block = block;

  iter->items_examined++;
  iter->previous_block = block;

  return TRUE;
}

static GMetricsAllocation *
get_allocation (GMetricsAllocationBlockStore *block_store,
                gsize                         number_of_blocks,
                const char                   *name)
{
  GMetricsAllocationFileMap *file_map;
  GMetricsAllocationPageTable *page_table;
  GMetricsAllocationPage *pages = NULL, *page = NULL;
  GMetricsAllocationBlock *block;
  GMetricsAllocation *allocation = NULL;
  GMetricsAllocationHeader *header;
  GMetricsAllocationPage *pages_to_deallocate = NULL;
  gssize number_of_extra_pages = -1;
  gsize number_of_pages;

  file_map = &block_store->file_map;
  page_table = &file_map->page_table;

  number_of_pages = g_metrics_allocation_page_table_get_number_of_pages_for_blocks (page_table, number_of_blocks);

  pages = g_metrics_allocation_page_table_find_pages_for_blocks (page_table, file_map, number_of_blocks);

  if (pages != NULL)
    {
      number_of_extra_pages = pages->number_of_pages_in_run - number_of_pages;
      if (number_of_extra_pages < 0)
        G_BREAKPOINT ();

      if (number_of_extra_pages > 0)
        {
          pages->number_of_pages_in_run = number_of_pages;
          pages_to_deallocate = pages + number_of_pages;
          pages_to_deallocate->number_of_pages_in_run = number_of_extra_pages;

          g_metrics_allocation_page_table_deallocate_pages (page_table,
                                                            file_map,
                                                            pages_to_deallocate,
                                                            number_of_extra_pages);
        }
    }
  else
    {
      pages = g_metrics_allocation_page_table_allocate_pages (page_table, number_of_pages);
    }

  if (pages == NULL)
    return NULL;

  page = &pages[0];

  if (page->number_of_allocated_blocks == 0)
    {
      gsize blocks_per_page;

      block = get_first_block_in_page (file_map, page);

      if (metrics_config.give_back_deallocated_pages)
        {
          void *address_of_page;

          address_of_page = block;

          madvise (address_of_page,
                   PAGE_SIZE * page->number_of_pages_in_run,
                   MADV_NORMAL);
        }

      blocks_per_page = g_metrics_allocation_page_table_get_number_of_blocks_per_page (page_table);

      block->header.number_of_blocks = MAX (number_of_blocks, blocks_per_page);
      block->header.is_allocated = FALSE;
      block->header.previous_block = NULL;

      page->run_of_free_blocks = block;
    }

  if (!g_metrics_allocation_page_has_free_blocks (page))
    G_BREAKPOINT ();

  block = (GMetricsAllocationBlock *) page->run_of_free_blocks;
  allocation = (GMetricsAllocation *) block;
  header = &block->header;

  if (header->number_of_blocks < number_of_blocks)
    G_BREAKPOINT ();

  if (header->is_allocated)
    G_BREAKPOINT ();

  g_metrics_allocation_file_map_claim_allocation (file_map, page, allocation);

  if (header->number_of_blocks > number_of_blocks)
    g_metrics_allocation_file_map_shrink_allocation (file_map, page, allocation, number_of_blocks);

  g_metrics_allocation_file_map_validate_block (file_map, page, block);
  validate_heap (page_table, 0, NULL);

  allocation = (GMetricsAllocation *) block;

  if (allocation != NULL)
    {
      GMetricsAllocationHeader *header;

      header = &allocation->header_block.header;

      if (header->number_of_blocks < number_of_blocks)
        G_BREAKPOINT ();

      if (name != NULL)
        {
          strncpy (header->name, name, sizeof (header->name) - 1);
          header->name[sizeof (header->name) - 1] = '\0';
        }
    }
  else
    {
      G_BREAKPOINT();
    }
  return allocation;
}

static GMetricsAllocationBlockStore *
g_metrics_get_thread_default_allocation_block_store (void)
{
  GMetricsAllocationBlockStore *block_store;

  if (!g_metrics_enabled ())
    return NULL;

  block_store = g_metrics_list_get_last_item (&block_store_stack);

  if (block_store == NULL)
    {
      allocate_thread_default_block_store ();
      block_store = g_metrics_list_get_last_item (&block_store_stack);

      if (block_store != NULL)
        block_store->stack_trace = g_metrics_stack_trace_new (4, 5, " -> ");
    }

  return block_store;
}

static GMetricsAllocationBlockStore *
g_metrics_get_allocation_block_store_for_address (gpointer allocation)
{
  GMetricsAllocationBlockStoreIter iter;
  GMetricsAllocationBlockStore *block_store = NULL;

  g_metrics_allocation_block_store_iter_init (&iter);
  while (g_metrics_allocation_block_store_iter_next (&iter, &block_store))
    {
      if (!g_metrics_allocation_block_store_has_allocation (block_store, allocation))
        continue;

      break;
    }

  return block_store;
}

void
g_metrics_push_default_allocation_block_store (GMetricsAllocationBlockStore *block_store)
{
  g_metrics_list_add_item (&block_store_stack, block_store);
}

void
g_metrics_pop_default_allocation_block_store (void)
{
  g_metrics_list_remove_last_item (&block_store_stack);
}

gpointer
g_metrics_allocation_block_store_allocate (GMetricsAllocationBlockStore *block_store,
                                           gsize                         size)
{
  return g_metrics_allocation_block_store_allocate_with_name (block_store, size, NULL);
}

gpointer
g_metrics_allocation_block_store_allocate_with_name (GMetricsAllocationBlockStore *block_store,
                                                     gsize                         size,
                                                     const char                   *name)
{
  GMetricsAllocation *allocation;
  gsize needed_blocks;

  if (!g_metrics_allocation_file_map_is_open (&block_store->file_map))
    return NULL;

  needed_blocks = calculate_blocks_needed_for_size (size);

  G_LOCK (allocations);
  allocation = get_allocation (block_store, needed_blocks, name);
  G_UNLOCK (allocations);

  if (allocation == NULL)
    return NULL;

  memset (allocation->payload_blocks, 0, size);
  return (gpointer) allocation->payload_blocks;
}

gpointer
g_metrics_allocation_block_store_reallocate (GMetricsAllocationBlockStore *block_store,
                                             gpointer                      payload,
                                             gsize                         size)
{
  GMetricsAllocationFileMap *file_map;
  GMetricsAllocationPageTable *page_table;
  GMetricsAllocationPage *pages;
  GMetricsAllocationBlock *payload_blocks;
  GMetricsAllocationBlock *first_block;
  GMetricsAllocation *allocation;
  GMetricsAllocationHeader *header;
  gssize number_of_allocated_blocks_in_pages;
  gsize old_size;
  gsize needed_blocks;
  gpointer new_payload;
  gboolean could_grow;

  g_metrics_init ();

  if (!g_metrics_enabled ())
    return __libc_realloc (payload, size);

  if (size == 0)
    {
      g_metrics_allocation_block_store_deallocate (block_store, payload);

      return NULL;
    }

  if (payload == NULL)
    return g_metrics_allocation_block_store_allocate_with_name (block_store, size, __func__);

  G_LOCK (allocations);
  file_map = &block_store->file_map;
  page_table = &file_map->page_table;

  payload_blocks = (GMetricsAllocationBlock *) payload;
  first_block = payload_blocks - 1;
  allocation = (GMetricsAllocation *) first_block;
  header = &allocation->header_block.header;
  needed_blocks = calculate_blocks_needed_for_size (size);
  pages = get_pages_for_block (block_store, first_block);
  old_size = g_metrics_allocation_get_payload_size (allocation);

  number_of_allocated_blocks_in_pages = get_number_of_allocated_blocks_for_pages (page_table, pages);

  if (number_of_allocated_blocks_in_pages < header->number_of_blocks)
      G_BREAKPOINT();

  if (needed_blocks == header->number_of_blocks)
    {
      G_UNLOCK (allocations);
      return payload;
    }

  if (needed_blocks < header->number_of_blocks)
    {
      g_metrics_allocation_file_map_shrink_allocation (file_map, pages, allocation, needed_blocks);
      G_UNLOCK (allocations);

      if (header->number_of_blocks != needed_blocks)
        G_BREAKPOINT ();

      return payload;
    }

  could_grow = g_metrics_allocation_file_map_grow_allocation (file_map, pages, allocation, needed_blocks);

  G_UNLOCK (allocations);

  if (could_grow)
    {
      if (header->number_of_blocks != needed_blocks)
        G_BREAKPOINT ();

      return payload;
    }

  new_payload = g_metrics_allocation_block_store_allocate_with_name (block_store, size, header->name);

  memcpy (new_payload, payload, old_size);

  g_metrics_allocation_block_store_deallocate (block_store, payload);

  payload_blocks = (GMetricsAllocationBlock *) new_payload;
  first_block = payload_blocks - 1;
  allocation = (GMetricsAllocation *) first_block;
  header = &allocation->header_block.header;

  if (header->number_of_blocks != needed_blocks)
    G_BREAKPOINT ();

  return new_payload;
}

gpointer
g_metrics_allocation_block_store_copy (GMetricsAllocationBlockStore *block_store,
                                       gconstpointer                 allocation,
                                       gsize                         size)
{
  return g_metrics_allocation_block_store_copy_with_name (block_store, allocation, size, __func__);
}

gpointer
g_metrics_allocation_block_store_copy_with_name (GMetricsAllocationBlockStore *block_store,
                                                 gconstpointer                 allocation,
                                                 gsize                         size,
                                                 const char                   *name)
{
  gpointer copy;

  copy = g_metrics_allocation_block_store_allocate_with_name (block_store, size, name);

  memcpy (copy, allocation, size);

  return copy;
}

__attribute__((visibility("default")))
void *
malloc (size_t size)
{
  if (!metrics_config.override_system_malloc)
    return __libc_malloc (size);

  return g_metrics_allocate (size);
}

__attribute__((visibility("default")))
void *
calloc (size_t nmemb, size_t size)
{
  if (!metrics_config.override_system_malloc)
    return __libc_calloc (nmemb, size);

  return g_metrics_allocate (size * nmemb);
}

__attribute__((visibility("default")))
void *realloc (void *ptr, size_t size)
{
  if (!metrics_config.override_system_malloc)
    return __libc_realloc (ptr, size);

  return g_metrics_reallocate (ptr, size);
}

__attribute__((visibility("default")))
void
free (void *ptr)
{
  g_metrics_free (ptr);
}

void
g_metrics_allocation_block_store_deallocate (GMetricsAllocationBlockStore *block_store,
                                             gpointer                      payload)
{
  GMetricsAllocationPage *pages;
  GMetricsAllocationBlock *payload_blocks;
  GMetricsAllocationBlock *first_block;
  GMetricsAllocation *allocation;

  if (!payload)
    return;

  G_LOCK (allocations);
  payload_blocks = (GMetricsAllocationBlock *) payload;
  first_block = payload_blocks - 1;
  pages = get_pages_for_block (block_store, first_block);

  allocation = (GMetricsAllocation *) first_block;

  g_metrics_allocation_file_map_release_allocation (&block_store->file_map, pages, allocation);
  G_UNLOCK (allocations);

  if (block_store->is_dedicated && block_store->number_of_allocations == 0)
    g_metrics_allocation_block_store_free (block_store);
}

gpointer
g_metrics_allocate (gsize size)
{
  GMetricsAllocationBlockStore *block_store = NULL;
  static gsize counter = 0;

  g_metrics_init ();

  if (!metrics_config.override_glib_malloc)
    goto fallback;

  block_store = g_metrics_get_thread_default_allocation_block_store ();
  if (block_store == NULL)
    goto fallback;

  if (!g_metrics_allocation_file_map_is_open (&block_store->file_map))
    goto fallback;

  if (size >= metrics_config.dedicated_allocation_block_store_threshold &&
      g_metrics_list_get_length (&allocation_block_stores) < metrics_config.max_allocation_block_stores)
    {
      GMetricsAllocationBlockStore *dedicated_block_store = NULL;
      char *name;

      asprintf (&name, "allocation-%ld-%ld", size, counter);
      counter++;

      dedicated_block_store = g_metrics_allocation_block_store_new (name, MAX (metrics_config.allocation_block_store_size / 10, size));
      g_metrics_free (name);

      if (dedicated_block_store != NULL)
        {
          dedicated_block_store->is_dedicated = TRUE;
          dedicated_block_store->stack_trace = g_metrics_stack_trace_new (4, 5, " -> ");

          return g_metrics_allocation_block_store_allocate (dedicated_block_store, size);
        }
    }

  return g_metrics_allocation_block_store_allocate (block_store, size);

fallback:
  return __libc_calloc (1, size);
}

gpointer
g_metrics_reallocate (gpointer allocation,
                      gsize    size)
{
  GMetricsAllocationBlockStore *block_store = NULL;

  g_metrics_init ();

  if (!metrics_config.override_glib_malloc)
    goto fallback;

  block_store = g_metrics_get_allocation_block_store_for_address (allocation);
  if (block_store == NULL)
    goto fallback;

  if (!g_metrics_allocation_file_map_is_open (&block_store->file_map))
    goto fallback;

  return g_metrics_allocation_block_store_reallocate (block_store, allocation, size);

fallback:
  return __libc_realloc (allocation, size);
}

gpointer
g_metrics_copy (gconstpointer allocation,
                gsize         size)
{
  GMetricsAllocationBlockStore *block_store = NULL;
  gpointer copy;

  g_metrics_init ();

  if (!metrics_config.override_glib_malloc)
    goto fallback;

  block_store = g_metrics_get_thread_default_allocation_block_store ();
  if (block_store == NULL)
    goto fallback;

  if (!g_metrics_allocation_file_map_is_open (&block_store->file_map))
    goto fallback;

  return g_metrics_allocation_block_store_copy_with_name (block_store, allocation, size, __func__);

fallback:
  copy = __libc_malloc (size);
  memcpy (copy, allocation, size);
  return copy;
}

void
g_metrics_free (gpointer allocation)
{
  GMetricsAllocationBlockStore *block_store = NULL;

  if (!allocation)
    return;

  block_store = g_metrics_get_allocation_block_store_for_address (allocation);

  if (block_store != NULL)
    {
      g_metrics_allocation_block_store_deallocate (block_store, allocation);
      return;
    }

  __libc_free (allocation);
}

static void
g_metrics_file_write (GMetricsFile *metrics_file,
                      const char   *data,
                      gsize         size)
{
  gchar *buf = (gchar *) data;
  gsize to_write = size;

  while (to_write > 0)
    {
      gssize count = gzwrite (metrics_file->gzipped_file, buf, to_write);
      if (count <= 0)
        {
          if (errno != EINTR)
            return;
        }
      else
        {
          to_write -= count;
          buf += count;
        }
    }
}

static void
on_sigusr1 (int signal_number)
{
  needs_flush = TRUE;
}

GMetricsFile *
g_metrics_file_new (const char *name,
                    const char *first_column_name,
                    const char *first_column_format,
                    ...)
{
  GMetricsFile *metrics_file;
  va_list args;
  const char *column_name, *column_format;
  UT_string *format_string = NULL;
  UT_string *header_string = NULL;
  char *filename = NULL;

  g_metrics_init ();

  utstring_new (format_string);
  utstring_new (header_string);
  utstring_printf (header_string, "generation,timestamp,%s", first_column_name);

  va_start (args, first_column_format);
  do
    {
      column_name = va_arg (args, const char *);

      if (column_name == NULL)
        break;

      column_format = va_arg (args, const char *);

      utstring_printf (header_string, ",%s", column_name);
      utstring_printf (format_string, ",%s", column_format);
    }
  while (column_name != NULL);
  va_end (args);

  utstring_printf (header_string, "\n");

  metrics_file = g_metrics_allocation_block_store_allocate_with_name (metrics_allocation_block_store,
                                                                      sizeof (GMetricsFile),
                                                                      "GMetricsFile");
  memset (metrics_file, 0, sizeof (GMetricsFile));

  asprintf (&metrics_file->static_format_string, "%%lu,%%lf,%s", first_column_format);
  metrics_file->variadic_format_string = strdup (utstring_body (format_string));

  asprintf (&filename,"%s/%s.csv.gz", metrics_config.log_dir, name);
  g_mkdir_with_parents (metrics_config.log_dir, 0755);

  metrics_file->gzipped_file = gzopen (filename, "wbe");
  g_metrics_file_write (metrics_file, utstring_body (header_string), utstring_len (header_string));
  utstring_free (format_string);
  utstring_free (header_string);
  free (filename);

  signal (SIGUSR1, on_sigusr1);

  return metrics_file;
}

void
g_metrics_file_start_record (GMetricsFile *metrics_file)
{
  metrics_file->now = ((long double) g_get_real_time ()) / (1.0 * G_USEC_PER_SEC);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
void
g_metrics_file_add_row_without_cast (GMetricsFile  *metrics_file,
                                     gconstpointer  first_column_value,
                                     ...)

{
  va_list args;
  gsize row_length = 0, buffer_left = 0, buffer_written = 0;
  char *row;
  gulong generation;

  generation = g_metrics_get_generation ();

  row_length += snprintf (NULL, 0, metrics_file->static_format_string, generation, metrics_file->now, first_column_value);

  va_start (args, first_column_value);
  row_length += vsnprintf (NULL, 0, metrics_file->variadic_format_string, args);
  va_end (args);

  row_length++;

  buffer_left = row_length + 1;
  row = g_metrics_allocation_block_store_allocate_with_name (metrics_allocation_block_store, buffer_left, __func__);

  buffer_written += snprintf (row, buffer_left, metrics_file->static_format_string, generation, metrics_file->now, first_column_value);
  buffer_left -= buffer_written;

  va_start (args, first_column_value);
  buffer_written += vsnprintf (row + buffer_written, buffer_left, metrics_file->variadic_format_string, args);
  buffer_left -= buffer_written;
  va_end (args);
  *(row + buffer_written) = '\n';

  g_metrics_file_write (metrics_file, row, row_length);
  g_metrics_allocation_block_store_deallocate (metrics_allocation_block_store, row);
}
#pragma GCC diagnostic pop

void
g_metrics_file_end_record (GMetricsFile *metrics_file)
{
  gulong generation;

  generation = g_metrics_get_generation ();

  if (needs_flush)
    {
      gzflush (metrics_file->gzipped_file, Z_FULL_FLUSH);
    }
  else if ((generation % 10) == 0)
    {
      gzflush (metrics_file->gzipped_file, Z_PARTIAL_FLUSH);
    }
}

void
g_metrics_file_free (GMetricsFile *metrics_file)
{
  gzclose (metrics_file->gzipped_file);
  free (metrics_file->static_format_string);
  free (metrics_file->variadic_format_string);
  g_metrics_allocation_block_store_deallocate (metrics_allocation_block_store, metrics_file);
}

struct _GMetricsTableEntry
{
  UT_hash_handle hh;
  char *name;
  gpointer record;
};

struct _GMetricsTable
{
  gsize record_size;
  GMetricsTableEntry *entries;
  gboolean is_sorted;
};

GMetricsTable *
g_metrics_table_new (gsize record_size)
{
  GMetricsTable *table;

  g_metrics_init ();

  table = g_metrics_allocation_block_store_allocate_with_name (metrics_allocation_block_store, sizeof (GMetricsTable), "GMetricsTable");
  table->record_size = record_size;

  return table;
}

void
g_metrics_table_set_record (GMetricsTable *metrics_table,
                            const char    *name,
                            gpointer       record)
{
  GMetricsTableEntry *entry = NULL;

  HASH_FIND_STR (metrics_table->entries, name, entry);

  if (entry == NULL)
    {
      entry = g_metrics_allocation_block_store_allocate_with_name (metrics_allocation_block_store, sizeof (GMetricsTableEntry), "GMetricsTableEntry");
      memset (entry, 0, sizeof (GMetricsTableEntry));

      entry->name = g_metrics_allocation_block_store_copy_with_name (metrics_allocation_block_store, name, strlen (name) + 1, "GMetricsTableEntry::name");
      entry->record = g_metrics_allocation_block_store_copy_with_name (metrics_allocation_block_store, record, metrics_table->record_size, "GMetricsTableEntry::record");

      HASH_ADD_KEYPTR (hh, metrics_table->entries, entry->name, strlen (entry->name), entry);
    }
  else
    {
      gpointer old_record;
      old_record = entry->record;
      entry->record = g_metrics_allocation_block_store_copy_with_name (metrics_allocation_block_store, record, metrics_table->record_size, "GMetricsTableEntry::record");
      g_metrics_allocation_block_store_deallocate (metrics_allocation_block_store, old_record);
    }
  metrics_table->is_sorted = FALSE;
}

gpointer
g_metrics_table_get_record (GMetricsTable *metrics_table,
                            const char    *name)
{
  GMetricsTableEntry *entry = NULL;

  HASH_FIND_STR (metrics_table->entries, name, entry);

  if (entry == NULL)
    return NULL;

  return entry->record;
}

void
g_metrics_table_remove_record (GMetricsTable *metrics_table,
                               const char    *name)
{
  GMetricsTableEntry *entry = NULL;

  HASH_FIND_STR (metrics_table->entries, name, entry);

  if (entry == NULL)
    return;

  HASH_DEL (metrics_table->entries, entry);
  g_metrics_allocation_block_store_deallocate (metrics_allocation_block_store, entry->name);
  g_metrics_allocation_block_store_deallocate (metrics_allocation_block_store, entry->record);
  g_metrics_allocation_block_store_deallocate (metrics_allocation_block_store, entry);

  metrics_table->is_sorted = FALSE;
}

void
g_metrics_table_clear (GMetricsTable *metrics_table)
{
  GMetricsTableEntry *entry = NULL, *next_entry = NULL;

  HASH_ITER (hh, metrics_table->entries, entry, next_entry)
    {
      HASH_DEL (metrics_table->entries, entry);
      g_metrics_allocation_block_store_deallocate (metrics_allocation_block_store, entry->name);
      g_metrics_allocation_block_store_deallocate (metrics_allocation_block_store, entry->record);
      g_metrics_allocation_block_store_deallocate (metrics_allocation_block_store, entry);
    }
  metrics_table->is_sorted = FALSE;
}

void
g_metrics_table_free (GMetricsTable *metrics_table)
{
  g_metrics_table_clear (metrics_table);
  g_metrics_allocation_block_store_deallocate (metrics_allocation_block_store, metrics_table);
}

void
g_metrics_table_iter_init (GMetricsTableIter *iter,
                           GMetricsTable     *table)
{
  iter->entry = table->entries;
}

static int
comparison_wrapper (GMetricsTableEntry *entry_1,
                    GMetricsTableEntry *entry_2,
                    GCompareFunc        comparison_function)
{
  if (comparison_function == NULL)
    return g_strcmp0 (entry_1->name, entry_2->name);

  return comparison_function (entry_1->record, entry_2->record);
}

void
g_metrics_table_sorted_iter_init (GMetricsTableIter *iter,
                                  GMetricsTable     *table,
                                  GCompareFunc       comparison_function)
{
  if (!table->is_sorted)
    {
      HASH_SRT_DATA (hh, table->entries, comparison_wrapper, comparison_function);
      table->is_sorted = TRUE;
    }

  iter->entry = table->entries;
}

gboolean
g_metrics_table_iter_next_without_cast (GMetricsTableIter  *iter,
                                        const char        **name,
                                        gpointer           *record)
{
  if (iter->entry == NULL)
    {
      if (name)
        *name = NULL;
      return FALSE;
    }

  if (name)
    *name = (const char *) iter->entry->name;

  if (iter->entry->record == NULL)
    G_BREAKPOINT ();

  if (record)
    *record = iter->entry->record;

  iter->entry = iter->entry->hh.next;
  return TRUE;
}

struct _GMetricsInstanceCounterEntry
{
  UT_hash_handle hh;
  char *name;
  gpointer record;
};

struct _GMetricsInstanceCounter
{
  GMetricsTable *tables[2];
  GMetricsTable *interesting_instances;
  ssize_t last_table_index;
  ssize_t current_table_index;
  size_t  generation;
};

static gint
g_metrics_instance_counter_metrics_sort (GMetricsInstanceCounterMetrics *a, GMetricsInstanceCounterMetrics *b)
{
  if (b->average_instance_change == a->average_instance_change)
    {
      if (b->total_memory_usage == a->total_memory_usage)
        {
          if (b->instance_count == a->instance_count)
            return 0;

          if (b->instance_count > a->instance_count)
            return 1;

          return -1;
        }

      if (b->total_memory_usage > a->total_memory_usage)
        return 1;

      return -1;
    }

  if (b->average_instance_change > a->average_instance_change)
    return 1;

  return -1;
}

GMetricsInstanceCounter *
g_metrics_instance_counter_new (void)
{
  GMetricsInstanceCounter *counter;

  g_metrics_init ();

  counter = g_metrics_allocation_block_store_allocate_with_name (metrics_allocation_block_store,
                                                                 sizeof (GMetricsInstanceCounter),
                                                                 "GMetricsInstanceCounter");
  memset (counter, 0, sizeof (GMetricsInstanceCounter));
  counter->last_table_index = -1;
  counter->interesting_instances = g_metrics_table_new (sizeof (gboolean));

  return counter;
}

void
g_metrics_instance_counter_start_record (GMetricsInstanceCounter *counter)
{
  GMetricsTable *table;

  counter->current_table_index = (counter->last_table_index + 1) % G_N_ELEMENTS (counter->tables);

  if (counter->tables[counter->current_table_index] == NULL)
    counter->tables[counter->current_table_index] = g_metrics_table_new (sizeof (GMetricsInstanceCounterMetrics));

  table = counter->tables[counter->current_table_index];
  g_metrics_table_clear (table);
}

void
g_metrics_instance_counter_end_record (GMetricsInstanceCounter *counter)
{
  GMetricsTable *old_table, *table;

  old_table = counter->tables[counter->last_table_index];
  table = counter->tables[counter->current_table_index];

  if (old_table != NULL)
    {
      GMetricsInstanceCounterMetrics *old_metrics = NULL;
      const char *name = NULL;
      GMetricsTableIter metrics_iter;

      g_metrics_table_iter_init (&metrics_iter, old_table);
      while (g_metrics_table_iter_next (&metrics_iter, &name, &old_metrics))
        {
            GMetricsInstanceCounterMetrics *metrics;

            metrics = g_metrics_table_get_record (table, name);
            if (metrics == NULL)
            {
                GMetricsInstanceCounterMetrics new_metrics = { { 0 }, 0 };
                new_metrics.instance_change = -old_metrics->instance_count;
                g_metrics_table_set_record (table, name, &new_metrics);
            }
        }
      g_metrics_table_clear (old_table);
    }

  counter->last_table_index = counter->current_table_index;
  counter->current_table_index = -1;
}

gboolean
g_metrics_instance_counter_instance_is_interesting (GMetricsInstanceCounter *counter,
                                                    const char              *name)
{
  GMetricsInstanceCounterIter iter;
  GMetricsInstanceCounterMetrics *metrics;
  const char *instance_name;
  size_t i = 0;

  if (strstr (name, metrics_config.collection_include_list) != NULL)
    return TRUE;

  g_metrics_instance_counter_iter_init (&iter, counter);
  while (g_metrics_instance_counter_iter_next (&iter, &instance_name, &metrics) &&
         i < metrics_config.number_of_interesting_instances)
    {
      if (g_strcmp0 (name, instance_name) == 0 &&
          strstr (name, metrics_config.collection_ignore_list) == NULL &&
          metrics->average_instance_change > 0)
        return TRUE;
      i++;
    }

  return FALSE;
}

void
g_metrics_instance_counter_add_instances (GMetricsInstanceCounter *counter,
                                          const char              *name,
                                          const char              *comment,
                                          size_t                   number_of_instances,
                                          size_t                   total_usage)
{
  GMetricsTable *old_table, *table;
  GMetricsInstanceCounterMetrics *metrics;
  size_t old_instance_count = 0, old_instance_watermark = 0;
  ssize_t old_average_instance_change = 0;
  size_t old_number_of_samples = 0;
  gulong generation;

  old_table = counter->tables[counter->last_table_index];
  table = counter->tables[counter->current_table_index];

  if (old_table != NULL)
    {
      GMetricsInstanceCounterMetrics *old_metrics = NULL;

      old_metrics = g_metrics_table_get_record (old_table, name);

      if (old_metrics != NULL)
        {
          old_average_instance_change = old_metrics->average_instance_change;
          old_instance_count = old_metrics->instance_count;
          old_instance_watermark = old_metrics->instance_watermark;
          old_number_of_samples = old_metrics->number_of_samples;
        }
    }

  metrics = g_metrics_table_get_record (table, name);
  if (metrics == NULL)
    {
      GMetricsInstanceCounterMetrics new_metrics = { { 0 }, 0 };
      g_metrics_table_set_record (table, name, &new_metrics);

      metrics = g_metrics_table_get_record (table, name);
      if (comment != NULL)
        strncpy (metrics->comment, comment, sizeof (metrics->comment) - 1);
    }

  metrics->instance_count += number_of_instances;
  metrics->instance_change = metrics->instance_count - old_instance_count;

  generation = g_metrics_get_generation ();
  if (generation > metrics_config.generations_to_settle &&
      metrics->instance_change != 0)
    {
    if (old_number_of_samples != 0)
      {
        metrics->average_instance_change = (old_average_instance_change * (ssize_t) old_number_of_samples) + (((ssize_t) metrics->instance_change) - old_average_instance_change);
        metrics->average_instance_change /= (ssize_t) old_number_of_samples;
        metrics->number_of_samples = old_number_of_samples + 1;
      }
    else
      {
        metrics->average_instance_change = metrics->instance_change;
        metrics->number_of_samples = 1;
      }
    }
  else
    {
      metrics->average_instance_change = old_average_instance_change;
      metrics->number_of_samples = old_number_of_samples;
    }

  metrics->instance_watermark = MAX (metrics->instance_watermark, metrics->instance_count);
  metrics->instance_watermark = MAX (metrics->instance_watermark, old_instance_watermark);
  metrics->total_memory_usage += total_usage;

  g_metrics_table_set_record (table, name, metrics);
}

void
g_metrics_instance_counter_add_instance (GMetricsInstanceCounter *counter,
                                         const char              *name,
                                         size_t                   memory_usage)
{
  g_metrics_instance_counter_add_instances (counter,
                                            name,
                                            NULL,
                                            1,
                                            memory_usage);
}

void
g_metrics_instance_counter_free (GMetricsInstanceCounter *counter)
{
  size_t i;

  for (i = 0; i < G_N_ELEMENTS (counter->tables); i++)
    g_metrics_table_free (counter->tables[i]);

  g_metrics_table_free (counter->interesting_instances);
  g_metrics_allocation_block_store_deallocate (metrics_allocation_block_store, counter);
}

void
g_metrics_instance_counter_iter_init (GMetricsInstanceCounterIter *iter,
                                      GMetricsInstanceCounter     *counter)
{
  GMetricsTable *old_table;

  iter->table_index = counter->last_table_index;

  old_table = counter->tables[iter->table_index];

  if (old_table == NULL)
    {
      memset (&iter->table_iter, 0, sizeof (iter->table_iter));
      return;
    }

  g_metrics_table_sorted_iter_init (&iter->table_iter, old_table, (GCompareFunc) g_metrics_instance_counter_metrics_sort);
}

gboolean
g_metrics_instance_counter_iter_next (GMetricsInstanceCounterIter    *iter,
                                      const char                    **name,
                                      GMetricsInstanceCounterMetrics **metrics)
{
  while (g_metrics_table_iter_next (&iter->table_iter, name, metrics))
    {
      if ((*metrics)->instance_change == 0)
        continue;

      return TRUE;
    }

  return FALSE;
}

gboolean
g_metrics_requested (const char *name)
{
  if (!g_metrics_enabled ())
    return FALSE;

  if (strstr (metrics_config.included_metrics, name) != NULL)
    return TRUE;

  if (strstr (metrics_config.skipped_metrics, name) != NULL)
    return FALSE;

  return TRUE;
}

void
g_metrics_start_timeout (GMetricsTimeoutFunc timeout_handler)
{
  G_LOCK (timeouts);

  if (timeout_fd < 0)
    {
      struct itimerspec timer_spec = { { 0 } };

      timer_spec.it_interval.tv_sec = metrics_config.collection_interval;
      timer_spec.it_value.tv_sec = metrics_config.collection_interval;

      timeout_fd = timerfd_create (CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
      if (timeout_fd >= 0)
        {
          int result;
          result = timerfd_settime (timeout_fd, 0, &timer_spec, NULL);

          if (result < 0)
            {
              close (timeout_fd);
              timeout_fd = -1;
            }
        }
    }

  g_metrics_list_add_item (timeout_handlers, timeout_handler);

  G_UNLOCK (timeouts);
}

static void
init_allocation_block_stores_metrics (void)
{
  static gboolean initialized;
  static gboolean needs_allocation_block_store_metrics;

  if (initialized)
    return;

  initialized = TRUE;
  needs_allocation_block_store_metrics = g_metrics_requested ("allocation-block-stores");

  if (needs_allocation_block_store_metrics)
    {
      allocation_block_store_metrics_file = g_metrics_file_new ("allocation-block-stores",
                                                                "name", "%s",
                                                                "thread name", "%s",
                                                                "number of allocations", "%ld",
                                                                "total size", "%ld",
                                                                "stack trace", "%s",
                                                                NULL);
      g_metrics_start_timeout (on_allocation_block_stores_metrics_timeout);
    }

  if (g_metrics_requested ("metrics-allocations"))
    g_metrics_start_timeout (on_write_allocation_list_timeout);
}

void
g_metrics_run_timeout_handlers (void)
{
  GMetricsListIter iter;
  GMetricsTimeoutFunc handler;

  guint64 number_of_expirations;

  read (timeout_fd, &number_of_expirations, sizeof (number_of_expirations));

  init_allocation_block_stores_metrics ();

  G_LOCK (timeouts);
  g_metrics_list_iter_init (&iter, timeout_handlers);
  while (g_metrics_list_iter_next (&iter, &handler))
    {
      if (handler != NULL)
        handler ();
    }
  g_metrics_generation++;
  G_UNLOCK (timeouts);

  needs_flush = FALSE;
}

gulong
g_metrics_get_generation (void)
{
  return g_metrics_generation;
}

struct _GMetricsListNode
{
  gpointer item;
  struct _GMetricsListNode *prev;
  struct _GMetricsListNode *next;
};

struct _GMetricsList
{
  struct _GMetricsListNode *first_node;
  gsize number_of_nodes;
};

GMetricsList *
g_metrics_list_new (void)
{
  GMetricsList *list;

  g_metrics_init ();

  list = g_metrics_allocation_block_store_allocate_with_name (metrics_allocation_block_store, sizeof (GMetricsList), "GMetricsList");
  memset (list, 0, sizeof (GMetricsList));

  return list;
}

void
g_metrics_list_add_item (GMetricsList *list,
                         gpointer      item)
{
  GMetricsListNode *node;

  node = g_metrics_allocation_block_store_allocate_with_name (metrics_allocation_block_store, sizeof (GMetricsListNode), "GMetricsListNode");
  memset (node, 0, sizeof (GMetricsListNode));

  node->item = item;

  DL_APPEND (list->first_node, node);
  list->number_of_nodes++;
}

void
g_metrics_list_remove_item (GMetricsList *list,
                            gpointer      item_to_remove)
{
  GMetricsListNode *node = NULL;

  DL_SEARCH_SCALAR (list->first_node, node, item, item_to_remove);

  if (node != NULL)
    {
      DL_DELETE (list->first_node, node);
      list->number_of_nodes--;

      g_metrics_allocation_block_store_deallocate (metrics_allocation_block_store, node);
    }
}

gpointer
g_metrics_list_get_last_item (GMetricsList *list)
{
  GMetricsListNode *last_node = NULL;

  if (list->number_of_nodes == 0)
    return NULL;

  last_node = list->first_node->prev;

  return last_node->item;
}

void
g_metrics_list_remove_last_item (GMetricsList *list)
{
  GMetricsListNode *last_node = NULL;

  if (list->number_of_nodes == 0)
    return;

  last_node = list->first_node->prev;

  DL_DELETE (list->first_node, last_node);
  list->number_of_nodes--;
}

gsize
g_metrics_list_get_length (GMetricsList *list)
{
  return list->number_of_nodes;
}

void
g_metrics_list_free (GMetricsList *list)
{
  GMetricsListNode *node, *next_node;

  DL_FOREACH_SAFE (list->first_node, node, next_node)
    {
      DL_DELETE (list->first_node, node);
      g_metrics_allocation_block_store_deallocate (metrics_allocation_block_store, node);
    }
  g_metrics_allocation_block_store_deallocate (metrics_allocation_block_store, list);
}

void
g_metrics_list_iter_init (GMetricsListIter *iter,
                          GMetricsList     *list)
{
  iter->node = list->first_node;

  if (iter->node != NULL)
    iter->next_node = iter->node->next;
}

gboolean
g_metrics_list_iter_next_without_cast (GMetricsListIter *iter,
                                       gpointer         *item)
{
  if (iter->node == NULL)
    {
      *item = NULL;
      return FALSE;
    }

  *item = iter->node->item;

  iter->node = iter->next_node;

  if (iter->node != NULL)
    iter->next_node = iter->node->next;
  return TRUE;
}

struct _GMetricsStackTrace
{
  gpointer *frames;
  int start_frame;
  int number_of_frames;
  char *delimiter;
  char *output;
  char *hash_key;
  char *annotation;
};

GMetricsStackTrace *
g_metrics_stack_trace_new (int start_frame,
                           int number_of_frames,
                           const char *delimiter)
{
  GMetricsStackTrace *stack_trace;
  int total_frames_needed;

  stack_trace = g_metrics_allocation_block_store_allocate_with_name (metrics_allocation_block_store, sizeof (GMetricsStackTrace), "GMetricsStackTrace");
  memset (stack_trace, 0, sizeof (GMetricsStackTrace));

  total_frames_needed = start_frame + number_of_frames;
  stack_trace->frames = g_metrics_allocation_block_store_allocate_with_name (metrics_allocation_block_store, sizeof (gpointer) * (total_frames_needed), "GMetricsStackTrace::frames");
  stack_trace->number_of_frames = backtrace (stack_trace->frames, total_frames_needed);
  stack_trace->start_frame = start_frame;
  stack_trace->delimiter = g_metrics_allocation_block_store_copy_with_name (metrics_allocation_block_store, delimiter, strlen (delimiter) + 1, "GMetricsStackTrace::delimiter");

  return stack_trace;
}

const char *
g_metrics_stack_trace_get_hash_key (GMetricsStackTrace *stack_trace)
{
  if (stack_trace->hash_key == NULL)
    {
      size_t i;
      int total_frames_needed;
      char *end;
      size_t key_size;
      size_t annotation_length = 0;

      total_frames_needed = stack_trace->number_of_frames - stack_trace->start_frame;
      if (stack_trace->annotation != NULL)
        annotation_length = strlen (stack_trace->annotation);
      key_size = strlen ("c00ldead0ldc0des") * total_frames_needed + annotation_length + 1;
      stack_trace->hash_key = g_metrics_allocation_block_store_allocate_with_name (metrics_allocation_block_store, key_size, "GMetricsStackTrace::hash_key");

      end = stack_trace->hash_key;
      for (i = stack_trace->start_frame; i < stack_trace->number_of_frames; i++)
        {
          size_t key_size_left;

          key_size_left = key_size - (end - stack_trace->hash_key);
          end = int_to_string ((uintptr_t)stack_trace->frames[i], end, key_size_left);
        }

      if (stack_trace->annotation != NULL)
        memcpy (end, stack_trace->annotation, annotation_length + 1);
    }

  return stack_trace->hash_key;
}

const char *
g_metrics_stack_trace_get_output (GMetricsStackTrace *stack_trace)
{
  if (stack_trace->output == NULL)
    {
      UT_string *output_string = NULL;
      char **symbols = NULL;
      int i;

      symbols = backtrace_symbols (stack_trace->frames, stack_trace->number_of_frames);

      if (symbols == NULL)
        return NULL;

      utstring_new (output_string);
      if (stack_trace->annotation != NULL)
        utstring_printf (output_string, "%s: ", stack_trace->annotation);
      for (i = stack_trace->start_frame; i < stack_trace->number_of_frames; i++)
        utstring_printf (output_string, "%s%s", symbols[i], stack_trace->delimiter);
      stack_trace->output = g_metrics_allocation_block_store_copy_with_name (metrics_allocation_block_store, utstring_body (output_string), utstring_len (output_string) + 1, "GMetricsStackTrace::output");
      utstring_free (output_string);
      g_metrics_free (symbols);
    }

  return stack_trace->output;
}

void
g_metrics_stack_trace_add_annotation (GMetricsStackTrace *stack_trace,
                                      const char         *annotation)
{
  stack_trace->annotation = g_metrics_allocation_block_store_copy_with_name (metrics_allocation_block_store, annotation, strlen (annotation) + 1, "GMetricsStackTrace::annotation");
}

void
g_metrics_stack_trace_free (GMetricsStackTrace *stack_trace)
{
  if (stack_trace == NULL)
    return;
  g_metrics_allocation_block_store_deallocate (metrics_allocation_block_store, stack_trace->annotation);
  g_metrics_allocation_block_store_deallocate (metrics_allocation_block_store, stack_trace->hash_key);
  g_metrics_allocation_block_store_deallocate (metrics_allocation_block_store, stack_trace->frames);
  g_metrics_allocation_block_store_deallocate (metrics_allocation_block_store, stack_trace->output);
  g_metrics_allocation_block_store_deallocate (metrics_allocation_block_store, stack_trace->delimiter);
  g_metrics_allocation_block_store_deallocate (metrics_allocation_block_store, stack_trace);
}

char *
g_metrics_stack_trace (void)
{
  GMetricsStackTrace *stack_trace;
  const char *output;
  char *copy = NULL;

  stack_trace = g_metrics_stack_trace_new (2, metrics_config.stack_trace_size, " -> ");
  output = g_metrics_stack_trace_get_output (stack_trace);

  if (output != NULL)
    copy = g_metrics_allocation_block_store_copy_with_name (metrics_allocation_block_store, output, strlen (output) + 1, __func__);

  return copy;
}

struct _GMetricsStackTraceSampler
{
  GMetricsTable *traces_table;
  GMetricsTable *instances_table;
};

typedef struct _GMetricsStackTraceSamplerInstanceEntry GMetricsStackTraceSamplerInstanceEntry;
struct _GMetricsStackTraceSamplerInstanceEntry
{
  gpointer instance;
  char *trace_hash_key;
};

static int
g_metrics_stack_trace_sample_sort (GMetricsStackTraceSample *a,
                                   GMetricsStackTraceSample *b)
{
  if (b->number_of_hits == a->number_of_hits)
    return g_strcmp0 (a->name, b->name);

  if (b->number_of_hits > a->number_of_hits)
    return 1;

  return -1;
}

void
g_metrics_stack_trace_sampler_iter_init (GMetricsStackTraceSamplerIter *iter,
                                         GMetricsStackTraceSampler     *sampler)
{
  g_metrics_table_sorted_iter_init (&iter->table_iter, sampler->traces_table, (GCompareFunc) g_metrics_stack_trace_sample_sort);
}

gboolean
g_metrics_stack_trace_sampler_iter_next (GMetricsStackTraceSamplerIter   *iter,
                                         GMetricsStackTraceSample       **sample)
{
  const char *key;

  if (!g_metrics_table_iter_next (&iter->table_iter, &key, sample))
    return FALSE;

  return TRUE;
}

void
g_metrics_set_stack_trace_annotation_handler (GMetricsStackTraceAnnotationHandler   handler,
                                              gpointer                              user_data)
{
  stack_trace_annotation_handler = handler;
  stack_trace_annotation_handler_user_data = user_data;
}

GMetricsStackTraceSampler *
g_metrics_stack_trace_sampler_new (void)
{
  GMetricsStackTraceSampler *sampler;

  sampler = g_metrics_allocation_block_store_allocate_with_name (metrics_allocation_block_store, sizeof (GMetricsStackTraceSampler), "GMetricsStackTraceSampler");
  memset (sampler, 0, sizeof (GMetricsStackTraceSampler));

  sampler->traces_table = g_metrics_table_new (sizeof (GMetricsStackTraceSample));
  sampler->instances_table = g_metrics_table_new (sizeof (GMetricsStackTraceSamplerInstanceEntry));

  return sampler;
}

static int
sort_of_random_integer (void)
{
  static unsigned int seed = 0;
  static unsigned int next_int = 0xbadc0de;

  if (seed == 0)
    {
      seed = (unsigned int) getpid ();

      next_int ^= seed;
    }

  next_int = next_int * 1103515245 + 12345;

  return (int) ((next_int / 65536) % 32768);
}

void
g_metrics_stack_trace_sampler_take_sample (GMetricsStackTraceSampler *sampler,
                                           const char                *name,
                                           gpointer                   instance)
{
  GMetricsStackTrace *stack_trace;
  GMetricsStackTraceSample *sample;
  GMetricsStackTraceSamplerInstanceEntry instance_entry = { 0 };
  const char *trace_key;
  char instance_name[64] = "";

  if ((sort_of_random_integer () % (metrics_config.stack_trace_sample_interval)) != 0)
    return;

  stack_trace = g_metrics_stack_trace_new (4, 5, " -> ");

  if (stack_trace_annotation_handler != NULL)
    {
      char annotation[metrics_config.stack_trace_annotation_size];
      gboolean annotated = FALSE;

      annotation[0] = '\0';
      annotated = stack_trace_annotation_handler(annotation, metrics_config.stack_trace_annotation_size - 1, stack_trace_annotation_handler_user_data);
      if (annotated)
        {
          annotation[metrics_config.stack_trace_annotation_size - 1] = '\0';
          g_metrics_stack_trace_add_annotation (stack_trace, annotation);
        }
    }

  trace_key = g_metrics_stack_trace_get_hash_key (stack_trace);

  sample = g_metrics_table_get_record (sampler->traces_table, trace_key);

  if (sample == NULL)
    {
      GMetricsStackTraceSample empty_sample = { { 0 }, 0 };

      g_metrics_table_set_record (sampler->traces_table, trace_key, &empty_sample);

      sample = g_metrics_table_get_record (sampler->traces_table, trace_key);
      sample->stack_trace = stack_trace;
      strncpy (sample->name, name, sizeof (sample->name) - 1);
      stack_trace = NULL;
    }

  sample->number_of_hits++;
  g_metrics_stack_trace_free (stack_trace);

  int_to_string ((uintptr_t) instance, instance_name, sizeof (instance_name));
  instance_entry.instance = instance;
  instance_entry.trace_hash_key = g_metrics_allocation_block_store_copy_with_name (metrics_allocation_block_store, trace_key, strlen (trace_key) + 1, "GMetricsStackTraceSamplerInstanceEntry::trace_hash_key");
  g_metrics_table_set_record (sampler->instances_table, instance_name, &instance_entry);
}

void
g_metrics_stack_trace_sampler_remove_sample (GMetricsStackTraceSampler *sampler,
                                             gpointer                   instance)
{
  GMetricsStackTraceSample *sample;
  GMetricsStackTraceSamplerInstanceEntry *instance_entry;
  char *trace_key;
  char instance_name[64] = "";

  int_to_string ((uintptr_t) instance, instance_name, sizeof (instance_name));
  instance_entry = g_metrics_table_get_record (sampler->instances_table, instance_name);

  if (instance_entry == NULL)
    return;

  trace_key = instance_entry->trace_hash_key;
  g_metrics_table_remove_record (sampler->instances_table, instance_name);

  sample = g_metrics_table_get_record (sampler->traces_table, trace_key);

  if (sample == NULL)
    {
      g_metrics_allocation_block_store_deallocate (metrics_allocation_block_store, trace_key);
      return;
    }

  sample->number_of_hits--;

  if (sample->number_of_hits == 0)
    {
      g_metrics_stack_trace_free (sample->stack_trace);
      g_metrics_table_remove_record (sampler->traces_table, trace_key);
    }

  g_metrics_allocation_block_store_deallocate (metrics_allocation_block_store, trace_key);
}

void
g_metrics_stack_trace_sampler_clear (GMetricsStackTraceSampler *sampler)
{
  GMetricsTableIter iter;
  const char *name;
  GMetricsStackTraceSample *sample;
  GMetricsStackTraceSamplerInstanceEntry *instance_entry;

  g_metrics_table_iter_init (&iter, sampler->traces_table);
  while (g_metrics_table_iter_next (&iter, &name, &sample))
    g_metrics_stack_trace_free (sample->stack_trace);
  g_metrics_table_clear (sampler->traces_table);

  g_metrics_table_iter_init (&iter, sampler->instances_table);
  while (g_metrics_table_iter_next (&iter, &name, &instance_entry))
    g_metrics_allocation_block_store_deallocate (metrics_allocation_block_store,
                                                 instance_entry->trace_hash_key);
  g_metrics_table_clear (sampler->instances_table);
}

void
g_metrics_stack_trace_sampler_free (GMetricsStackTraceSampler *sampler)
{
  if (sampler == NULL)
    return;

  g_metrics_stack_trace_sampler_clear (sampler);
  g_metrics_table_free (sampler->traces_table);
  g_metrics_table_free (sampler->instances_table);
  g_metrics_allocation_block_store_deallocate (metrics_allocation_block_store, sampler);
}

int
g_metrics_get_timeout_fd (void)
{
  return timeout_fd;
}

const char *
g_metrics_get_log_dir (void)
{
  return metrics_config.log_dir;
}
