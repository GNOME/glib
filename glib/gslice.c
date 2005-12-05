/* GLIB sliced memory - fast concurrent memory chunk allocator
 * Copyright (C) 2005 Tim Janik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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
 */
/* MT safe */
#define _XOPEN_SOURCE 600       /* posix_memalign() */
#include <stdlib.h>             /* posix_memalign() */
#include <assert.h>             /* assert() for nomessage phase */
#include <string.h>
#include <errno.h>
#include "config.h"
#include "gmem.h"               /* gslice.h */
#include "gthreadinit.h"
#include "galias.h"
#include "glib.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>             /* sysconf() */
#endif
#ifdef G_OS_WIN32
#include <windows.h>
#endif

/* the GSlice allocator is split up into 4 layers, roughly modelled after the slab
 * allocator and magazine extensions as outlined in:
 * + [Bonwick94] Jeff Bonwick, The slab allocator: An object-caching kernel
 *   memory allocator. USENIX 1994, http://citeseer.ist.psu.edu/bonwick94slab.html
 * + [Bonwick01] Bonwick and Jonathan Adams, Magazines and vmem: Extending the
 *   slab allocator to many cpu's and arbitrary resources.
 *   USENIX 2001, http://citeseer.ist.psu.edu/bonwick01magazines.html
 * the layers are:
 * - the thread magazines. for each (aligned) chunk size, a magazine (a list)
 *   of recently freed and soon to be allocated chunks is maintained per thread.
 *   this way, most alloc/free requests can be quickly satisfied from per-thread
 *   free lists which only require one g_private_get() call to retrive the
 *   thread handle.
 * - the magazine cache. allocating and freeing chunks to/from threads only
 *   occours at magazine sizes from a global depot of magazines. the depot
 *   maintaines a 15 second working set of allocated magazines, so full
 *   magazines are not allocated and released too often.
 *   the chunk size dependent magazine sizes automatically adapt (within limits,
 *   see [3]) to lock contention to properly scale performance across a variety
 *   of SMP systems.
 * - the slab allocator. this allocator allocates slabs (blocks of memory) close
 *   to the system page size or multiples thereof which have to be page aligned.
 *   the blocks are divided into smaller chunks which are used to satisfy
 *   allocations from the upper layers. the space provided by the reminder of
 *   the chunk size division is used for cache colorization (random distribution
 *   of chunk addresses) to improve processor cache utilization. multiple slabs
 *   with the same chunk size are kept in a partially sorted ring to allow O(1)
 *   freeing and allocation of chunks (as long as the allocation of an entirely
 *   new slab can be avoided).
 * - the page allocator. on most modern systems, posix_memalign(3) or
 *   memalign(3) should be available, so this is used to allocate blocks with
 *   system page size based alignments and sizes or multiples thereof.
 *   if no memalign variant is provided, valloc() is used instead and
 *   block sizes are limited to the system page size (no multiples thereof).
 *   as a fallback, on system without even valloc(), a malloc(3)-based page
 *   allocator with alloc-only behaviour is used.
 *
 * NOTES:
 * [1] some systems memalign(3) implementations may rely on boundary tagging for
 *     the handed out memory chunks. to avoid excessive page-wise fragmentation,
 *     we reserve 2 * sizeof (void*) per block size for the systems memalign(3),
 *     specified in NATIVE_MALLOC_PADDING.
 * [2] using the slab allocator alone already provides for a fast and efficient
 *     allocator, it doesn't properly scale beyond single-threaded uses though.
 *     also, the slab allocator implements eager free(3)-ing, i.e. does not
 *     provide any form of caching or working set maintenance. so if used alone,
 *     it's vulnerable to trashing for sequences of balanced (alloc, free) pairs
 *     at certain thresholds.
 * [3] magazine sizes are bound by an implementation specific minimum size and
 *     a chunk size specific maximum to limit magazine storage sizes to roughly
 *     16KB.
 * [4] allocating ca. 8 chunks per block/page keeps a good balance between
 *     external and internal fragmentation (<= 12.5%) [Bonwick94]
 */

/* --- macros and constants --- */
#define LARGEALIGNMENT          (256)
#define P2ALIGNMENT             (2 * sizeof (gsize))                            /* fits 2 pointers (assumed to be 2 * GLIB_SIZEOF_SIZE_T below) */
#define ALIGN(size, base)       ((base) * (gsize) (((size) + (base) - 1) / (base)))
#define NATIVE_MALLOC_PADDING   P2ALIGNMENT                                     /* per-page padding left for native malloc(3) see [1] */
#define SLAB_INFO_SIZE          P2ALIGN (sizeof (SlabInfo) + NATIVE_MALLOC_PADDING)
#define MAX_MAGAZINE_SIZE       (256)                                           /* see [3] and allocator_get_magazine_threshold() for this */
#define MIN_MAGAZINE_SIZE       (4)
#define MAX_STAMP_COUNTER       (7)                                             /* distributes the load of gettimeofday() */
#define MAX_SLAB_CHUNK_SIZE(al) (((al)->max_page_size - SLAB_INFO_SIZE) / 8)    /* we want at last 8 chunks per page, see [4] */
#define MAX_SLAB_INDEX(al)      (SLAB_INDEX (al, MAX_SLAB_CHUNK_SIZE (al)) + 1)
#define SLAB_INDEX(al, asize)   ((asize) / P2ALIGNMENT - 1)                     /* asize must be P2ALIGNMENT aligned */
#define SLAB_CHUNK_SIZE(al, ix) (((ix) + 1) * P2ALIGNMENT)
#define SLAB_PAGE_SIZE(al,csz)  (ALIGN (8 * (csz) + SLAB_INFO_SIZE, (al)->min_page_size))

/* optimized version of ALIGN (size, P2ALIGNMENT) */
#if     GLIB_SIZEOF_SIZE_T * 2 == 8  /* P2ALIGNMENT */
#define P2ALIGN(size)   (((size) + 0x7) & ~(gsize) 0x7)
#elif   GLIB_SIZEOF_SIZE_T * 2 == 16 /* P2ALIGNMENT */
#define P2ALIGN(size)   (((size) + 0xf) & ~(gsize) 0xf)
#else
#define P2ALIGN(size)   ALIGN (size, P2ALIGNMENT)
#endif

/* --- structures --- */
typedef struct _ChunkLink      ChunkLink;
typedef struct _SlabInfo       SlabInfo;
typedef struct _CachedMagazine CachedMagazine;
struct _ChunkLink {
  ChunkLink *next;
  ChunkLink *data;
};
struct _SlabInfo {
  ChunkLink *chunks;
  guint n_allocated;
  SlabInfo *next, *prev;
};
typedef struct {
  ChunkLink *chunks;
  gsize      count;                     /* approximative chunks list length */
} Magazine;
typedef struct {
  Magazine   *magazine1;                /* array of MAX_SLAB_INDEX (allocator) */
  Magazine   *magazine2;                /* array of MAX_SLAB_INDEX (allocator) */
} ThreadMemory;
typedef struct {
  gboolean always_malloc;
  gboolean bypass_magazines;
  gboolean always_free;
  gsize    working_set_msecs;
} SliceConfig;
typedef struct {
  /* const after initialization */
  gsize         min_page_size, max_page_size;
  SliceConfig   config;
  guint         max_slab_chunk_size_for_magazine_cache;
  /* magazine cache */
  GMutex       *magazine_mutex;
  ChunkLink   **magazines;                /* array of MAX_SLAB_INDEX (allocator) */
  guint        *contention_counters;      /* array of MAX_SLAB_INDEX (allocator) */
  gint          mutex_counter;
  guint         stamp_counter;
  guint         last_stamp;
  /* slab allocator */
  GMutex       *slab_mutex;
  SlabInfo    **slab_stack;                /* array of MAX_SLAB_INDEX (allocator) */
  guint        color_accu;
} Allocator;

/* --- prototypes --- */
static gpointer     slab_allocator_alloc_chunk       (gsize      chunk_size);
static void         slab_allocator_free_chunk        (gsize      chunk_size,
                                                      gpointer   mem);
static void         private_thread_memory_cleanup    (gpointer   data);
static gpointer     allocator_memalign               (gsize      alignment,
                                                      gsize      memsize);
static void         allocator_memfree                (gsize      memsize,
                                                      gpointer   mem);
static inline void  magazine_cache_update_stamp      (void);
static inline gsize allocator_get_magazine_threshold (Allocator *allocator,
                                                      guint      ix);

/* --- variables --- */
static GPrivate        *private_thread_memory = NULL;
static gsize            sys_page_size = 0;
static Allocator        allocator[1] = { { 0, }, };
static SliceConfig      slice_config = {
  FALSE,        /* always_malloc */
  FALSE,        /* bypass_magazines */
  FALSE,        /* always_free */
  15 * 1000,    /* working_set_msecs */
};

/* --- auxillary funcitons --- */
void
g_slice_set_config (GSliceConfig ckey,
                    gint64       value)
{
  g_return_if_fail (sys_page_size == 0);
  switch (ckey)
    {
    case G_SLICE_CONFIG_ALWAYS_MALLOC:
      slice_config.always_malloc = value != 0;
      break;
    case G_SLICE_CONFIG_BYPASS_MAGAZINES:
      slice_config.bypass_magazines = value != 0;
      break;
    case G_SLICE_CONFIG_ALWAYS_FREE:
      slice_config.always_free = value != 0;
      break;
    case G_SLICE_CONFIG_WORKING_SET_MSECS:
      slice_config.working_set_msecs = value;
      break;
    default: ;
    }
}

gint64
g_slice_get_config (GSliceConfig ckey)
{
  switch (ckey)
    {
    case G_SLICE_CONFIG_ALWAYS_MALLOC:
      return slice_config.always_malloc;
    case G_SLICE_CONFIG_BYPASS_MAGAZINES:
      return slice_config.bypass_magazines;
    case G_SLICE_CONFIG_ALWAYS_FREE:
      return slice_config.always_free;
    case G_SLICE_CONFIG_WORKING_SET_MSECS:
      return slice_config.working_set_msecs;
    case G_SLICE_CONFIG_CHUNK_SIZES:
      return MAX_SLAB_INDEX (allocator);
    default:
      return 0;
    }
}

gint64*
g_slice_get_config_state (GSliceConfig ckey,
                          gint64       address,
                          guint       *n_values)
{
  guint i = 0;
  g_return_val_if_fail (n_values != NULL, NULL);
  *n_values = 0;
  switch (ckey)
    {
      gint64 array[64];
    case G_SLICE_CONFIG_CONTENTION_COUNTER:
      array[i++] = SLAB_CHUNK_SIZE (allocator, address);
      array[i++] = allocator->contention_counters[address];
      array[i++] = allocator_get_magazine_threshold (allocator, address);
      *n_values = i;
      return g_memdup (array, sizeof (array[0]) * *n_values);
    default:
      return NULL;
    }
}

static void
g_slice_init_nomessage (void)
{
#ifdef G_OS_WIN32
  SYSTEM_INFO system_info;
#endif
  /* we may not use g_error() or friends here */
  assert (sys_page_size == 0);

#ifdef G_OS_WIN32
  GetSystemInfo (&system_info);
  sys_page_size = system_info.dwPageSize;
#else
  sys_page_size = sysconf (_SC_PAGESIZE); /* = sysconf (_SC_PAGE_SIZE); = getpagesize(); */
#endif
  assert (sys_page_size >= 2 * LARGEALIGNMENT);
  allocator->config = slice_config;
  allocator->min_page_size = sys_page_size;
#if HAVE_POSIX_MEMALIGN || HAVE_MEMALIGN
  /* allow allocation of pages up to 8KB (with 8KB alignment).
   * this is useful because many medium to large sized structures
   * fit less than 8 times (see [4]) into 4KB pages.
   */
  allocator->min_page_size = MAX (allocator->min_page_size, 4096);
  allocator->max_page_size = MAX (allocator->min_page_size, 8192);
#else
  /* we can only align to system page size */
  allocator->max_page_size = sys_page_size;
#endif
  allocator->magazine_mutex = NULL;     /* _g_slice_thread_init_nomessage() */
  allocator->magazines = g_new0 (ChunkLink*, MAX_SLAB_INDEX (allocator));
  allocator->contention_counters = g_new0 (guint, MAX_SLAB_INDEX (allocator));
  allocator->mutex_counter = 0;
  allocator->stamp_counter = MAX_STAMP_COUNTER; /* force initial update */
  allocator->last_stamp = 0;
  allocator->slab_mutex = NULL;         /* _g_slice_thread_init_nomessage() */
  allocator->slab_stack = g_new0 (SlabInfo*, MAX_SLAB_INDEX (allocator));
  allocator->color_accu = 0;
  magazine_cache_update_stamp();
  /* values cached for performance reasons */
  allocator->max_slab_chunk_size_for_magazine_cache = MAX_SLAB_CHUNK_SIZE (allocator);
  if (allocator->config.always_malloc || allocator->config.bypass_magazines)
    allocator->max_slab_chunk_size_for_magazine_cache = 0;      /* non-optimized cases */
}

static inline guint
allocator_categorize (gsize aligned_chunk_size)
{
  /* speed up the likely path */
  if (G_LIKELY (aligned_chunk_size && aligned_chunk_size <= allocator->max_slab_chunk_size_for_magazine_cache))
    return 1;           /* use magazine cache */

  /* the above will fail (max_slab_chunk_size_for_magazine_cache == 0) if the
   * allocator is still uninitialized, or if we are not configured to use the
   * magazine cache.
   */
  if (!sys_page_size)
    g_slice_init_nomessage ();
  if (!allocator->config.always_malloc &&
      aligned_chunk_size &&
      aligned_chunk_size <= MAX_SLAB_CHUNK_SIZE (allocator))
    {
      if (allocator->config.bypass_magazines)
        return 2;       /* use slab allocator, see [2] */
      return 1;         /* use magazine cache */
    }
  return 0;             /* use malloc() */
}

void
_g_slice_thread_init_nomessage (void)
{
  /* we may not use g_error() or friends here */
  if (!sys_page_size)
    g_slice_init_nomessage();
  private_thread_memory = g_private_new (private_thread_memory_cleanup);
  allocator->magazine_mutex = g_mutex_new();
  allocator->slab_mutex = g_mutex_new();
}

static inline void
g_mutex_lock_a (GMutex *mutex,
                guint  *contention_counter)
{
  gboolean contention = FALSE;
  if (!g_mutex_trylock (mutex))
    {
      g_mutex_lock (mutex);
      contention = TRUE;
    }
  if (contention)
    {
      allocator->mutex_counter++;
      if (allocator->mutex_counter >= 1)        /* quickly adapt to contention */
        {
          allocator->mutex_counter = 0;
          *contention_counter = MIN (*contention_counter + 1, MAX_MAGAZINE_SIZE);
        }
    }
  else /* !contention */
    {
      allocator->mutex_counter--;
      if (allocator->mutex_counter < -11)       /* moderately recover magazine sizes */
        {
          allocator->mutex_counter = 0;
          *contention_counter = MAX (*contention_counter, 1) - 1;
        }
    }
}

static inline ThreadMemory*
thread_memory_from_self (void)
{
  ThreadMemory *tmem = g_private_get (private_thread_memory);
  if (G_UNLIKELY (!tmem))
    {
      const guint n_magazines = MAX_SLAB_INDEX (allocator);
      tmem = g_malloc0 (sizeof (ThreadMemory) + sizeof (Magazine) * 2 * n_magazines);
      tmem->magazine1 = (Magazine*) (tmem + 1);
      tmem->magazine2 = &tmem->magazine1[n_magazines];
      g_private_set (private_thread_memory, tmem);
    }
  return tmem;
}

static inline ChunkLink*
magazine_chain_pop_head (ChunkLink **magazine_chunks)
{
  /* magazine chains are linked via ChunkLink->next.
   * each ChunkLink->data of the toplevel chain may point to a subchain,
   * linked via ChunkLink->next. ChunkLink->data of the subchains just
   * contains uninitialized junk.
   */
  ChunkLink *chunk = (*magazine_chunks)->data;
  if (G_UNLIKELY (chunk))
    {
      /* allocating from freed list */
      (*magazine_chunks)->data = chunk->next;
    }
  else
    {
      chunk = *magazine_chunks;
      *magazine_chunks = chunk->next;
    }
  return chunk;
}

#if 0 /* useful for debugging */
static guint
magazine_count (ChunkLink *head)
{
  guint count = 0;
  if (!head)
    return 0;
  while (head)
    {
      ChunkLink *child = head->data;
      count += 1;
      for (child = head->data; child; child = child->next)
        count += 1;
      head = head->next;
    }
  return count;
}
#endif

static inline gsize
allocator_get_magazine_threshold (Allocator *allocator,
                                  guint      ix)
{
  /* the magazine size calculated here has a lower bound of MIN_MAGAZINE_SIZE,
   * which is required by the implementation. also, for moderately sized chunks
   * (say >= 64 bytes), magazine sizes shouldn't be much smaller then the number
   * of chunks available per page/2 to avoid excessive traffic in the magazine
   * cache for small to medium sized structures.
   * the upper bound of the magazine size is effectively provided by
   * MAX_MAGAZINE_SIZE. for larger chunks, this number is scaled down so that
   * the content of a single magazine doesn't exceed ca. 16KB.
   */
  gsize chunk_size = SLAB_CHUNK_SIZE (allocator, ix);
  guint threshold = MAX (MIN_MAGAZINE_SIZE, allocator->max_page_size / MAX (5 * chunk_size, 5 * 32));
  guint contention_counter = allocator->contention_counters[ix];
  if (G_UNLIKELY (contention_counter))  /* single CPU bias */
    {
      /* adapt contention counter thresholds to chunk sizes */
      contention_counter = contention_counter * 64 / chunk_size;
      threshold = MAX (threshold, contention_counter);
    }
  return threshold;
}

/* --- magazine cache --- */
static inline void
magazine_cache_update_stamp (void)
{
  if (allocator->stamp_counter >= MAX_STAMP_COUNTER)
    {
      GTimeVal tv;
      g_get_current_time (&tv);
      allocator->last_stamp = tv.tv_sec * 1000 + tv.tv_usec / 1000; /* milli seconds */
      allocator->stamp_counter = 0;
    }
  else
    allocator->stamp_counter++;
}

static inline ChunkLink*
magazine_chain_prepare_fields (ChunkLink *magazine_chunks)
{
  ChunkLink *chunk1;
  ChunkLink *chunk2;
  ChunkLink *chunk3;
  ChunkLink *chunk4;
  g_assert (MIN_MAGAZINE_SIZE >= 4);
  /* ensure a magazine with at least 4 unused data pointers */
  chunk1 = magazine_chain_pop_head (&magazine_chunks);
  chunk2 = magazine_chain_pop_head (&magazine_chunks);
  chunk3 = magazine_chain_pop_head (&magazine_chunks);
  chunk4 = magazine_chain_pop_head (&magazine_chunks);
  chunk4->next = magazine_chunks;
  chunk3->next = chunk4;
  chunk2->next = chunk3;
  chunk1->next = chunk2;
  return chunk1;
}

/* access the first 3 fields of a specially prepared magazine chain */
#define magazine_chain_prev(mc)         ((mc)->data)
#define magazine_chain_stamp(mc)        ((mc)->next->data)
#define magazine_chain_uint_stamp(mc)   GPOINTER_TO_UINT ((mc)->next->data)
#define magazine_chain_next(mc)         ((mc)->next->next->data)
#define magazine_chain_count(mc)        ((mc)->next->next->next->data)

static void
magazine_cache_trim (Allocator *allocator,
                     guint      ix,
                     guint      stamp)
{
  /* g_mutex_lock (allocator->mutex); done by caller */
  /* trim magazine cache from tail */
  ChunkLink *current = magazine_chain_prev (allocator->magazines[ix]);
  ChunkLink *trash = NULL;
  while (allocator->config.always_free ||
         ABS (stamp - magazine_chain_uint_stamp (current)) > allocator->config.working_set_msecs)
    {
      /* unlink */
      ChunkLink *prev = magazine_chain_prev (current);
      ChunkLink *next = magazine_chain_next (current);
      magazine_chain_next (prev) = next;
      magazine_chain_prev (next) = prev;
      /* clear special fields, put on trash stack */
      magazine_chain_next (current) = NULL;
      magazine_chain_count (current) = NULL;
      magazine_chain_stamp (current) = NULL;
      magazine_chain_prev (current) = trash;
      trash = current;
      /* fixup list head if required */
      if (current == allocator->magazines[ix])
        {
          allocator->magazines[ix] = NULL;
          break;
        }
      current = prev;
    }
  g_mutex_unlock (allocator->magazine_mutex);
  /* free trash */
  if (trash)
    {
      const gsize chunk_size = SLAB_CHUNK_SIZE (allocator, ix);
      g_mutex_lock (allocator->slab_mutex);
      while (trash)
        {
          current = trash;
          trash = magazine_chain_prev (current);
          magazine_chain_prev (current) = NULL; /* clear special field */
          while (current)
            {
              ChunkLink *chunk = magazine_chain_pop_head (&current);
              slab_allocator_free_chunk (chunk_size, chunk);
            }
        }
      g_mutex_unlock (allocator->slab_mutex);
    }
}

static void
magazine_cache_push_magazine (guint      ix,
                              ChunkLink *magazine_chunks,
                              gsize      count) /* must be >= MIN_MAGAZINE_SIZE */
{
  ChunkLink *current = magazine_chain_prepare_fields (magazine_chunks);
  ChunkLink *next, *prev;
  g_mutex_lock (allocator->magazine_mutex);
  /* add magazine at head */
  next = allocator->magazines[ix];
  if (next)
    prev = magazine_chain_prev (next);
  else
    next = prev = current;
  magazine_chain_next (prev) = current;
  magazine_chain_prev (next) = current;
  magazine_chain_prev (current) = prev;
  magazine_chain_next (current) = next;
  magazine_chain_count (current) = (gpointer) count;
  /* stamp magazine */
  magazine_cache_update_stamp();
  magazine_chain_stamp (current) = GUINT_TO_POINTER (allocator->last_stamp);
  allocator->magazines[ix] = current;
  /* free old magazines beyond a certain threshold */
  magazine_cache_trim (allocator, ix, allocator->last_stamp);
  /* g_mutex_unlock (allocator->mutex); was done by magazine_cache_trim() */
}

static ChunkLink*
magazine_cache_pop_magazine (guint  ix,
                             gsize *countp)
{
  g_mutex_lock_a (allocator->magazine_mutex, &allocator->contention_counters[ix]);
  if (!allocator->magazines[ix])
    {
      guint magazine_threshold = allocator_get_magazine_threshold (allocator, ix);
      gsize i, chunk_size = SLAB_CHUNK_SIZE (allocator, ix);
      ChunkLink *current = NULL;
      g_mutex_unlock (allocator->magazine_mutex);
      g_mutex_lock (allocator->slab_mutex);
      for (i = 0; i < magazine_threshold; i++)
        {
          ChunkLink *chunk = slab_allocator_alloc_chunk (chunk_size);
          chunk->data = NULL;
          chunk->next = current;
          current = chunk;
        }
      g_mutex_unlock (allocator->slab_mutex);
      *countp = i;
      return current;
    }
  else
    {
      ChunkLink *current = allocator->magazines[ix];
      ChunkLink *prev = magazine_chain_prev (current);
      ChunkLink *next = magazine_chain_next (current);
      /* unlink */
      magazine_chain_next (prev) = next;
      magazine_chain_prev (next) = prev;
      allocator->magazines[ix] = next == current ? NULL : next;
      g_mutex_unlock (allocator->magazine_mutex);
      /* clear special fields and hand out */
      *countp = (gsize) magazine_chain_count (current);
      magazine_chain_prev (current) = NULL;
      magazine_chain_next (current) = NULL;
      magazine_chain_count (current) = NULL;
      magazine_chain_stamp (current) = NULL;
      return current;
    }
}

/* --- thread magazines --- */
static void
private_thread_memory_cleanup (gpointer data)
{
  ThreadMemory *tmem = data;
  const guint n_magazines = MAX_SLAB_INDEX (allocator);
  guint ix;
  for (ix = 0; ix < n_magazines; ix++)
    {
      Magazine *mags[2];
      guint j;
      mags[0] = &tmem->magazine1[ix];
      mags[1] = &tmem->magazine2[ix];
      for (j = 0; j < 2; j++)
        {
          Magazine *mag = mags[j];
          if (mag->count >= MIN_MAGAZINE_SIZE)
            magazine_cache_push_magazine (ix, mag->chunks, mag->count);
          else
            {
              const gsize chunk_size = SLAB_CHUNK_SIZE (allocator, ix);
              g_mutex_lock (allocator->slab_mutex);
              while (mag->chunks)
                {
                  ChunkLink *chunk = magazine_chain_pop_head (&mag->chunks);
                  slab_allocator_free_chunk (chunk_size, chunk);
                }
              g_mutex_unlock (allocator->slab_mutex);
            }
        }
    }
  g_free (tmem);
}

static void
thread_memory_magazine1_reload (ThreadMemory *tmem,
                                guint         ix)
{
  Magazine *mag = &tmem->magazine1[ix];
  g_assert (mag->chunks == NULL); /* ensure that we may reset mag->count */
  mag->count = 0;
  mag->chunks = magazine_cache_pop_magazine (ix, &mag->count);
}

static void
thread_memory_magazine2_unload (ThreadMemory *tmem,
                                guint         ix)
{
  Magazine *mag = &tmem->magazine2[ix];
  magazine_cache_push_magazine (ix, mag->chunks, mag->count);
  mag->chunks = NULL;
  mag->count = 0;
}

static inline void
thread_memory_swap_magazines (ThreadMemory *tmem,
                              guint         ix)
{
  Magazine xmag = tmem->magazine1[ix];
  tmem->magazine1[ix] = tmem->magazine2[ix];
  tmem->magazine2[ix] = xmag;
}

static inline gboolean
thread_memory_magazine1_is_empty (ThreadMemory *tmem,
                                  guint         ix)
{
  return tmem->magazine1[ix].chunks == NULL;
}

static inline gboolean
thread_memory_magazine2_is_full (ThreadMemory *tmem,
                                 guint         ix)
{
  return tmem->magazine2[ix].count >= allocator_get_magazine_threshold (allocator, ix);
}

static inline gpointer
thread_memory_magazine1_alloc (ThreadMemory *tmem,
                               guint         ix)
{
  Magazine *mag = &tmem->magazine1[ix];
  ChunkLink *chunk = magazine_chain_pop_head (&mag->chunks);
  if (G_LIKELY (mag->count > 0))
    mag->count--;
  return chunk;
}

static inline void
thread_memory_magazine2_free (ThreadMemory *tmem,
                              guint         ix,
                              gpointer      mem)
{
  Magazine *mag = &tmem->magazine2[ix];
  ChunkLink *chunk = mem;
  chunk->data = NULL;
  chunk->next = mag->chunks;
  mag->chunks = chunk;
  mag->count++;
}

/* --- API functions --- */
gpointer
g_slice_alloc (gsize mem_size)
{
  gsize chunk_size;
  gpointer mem;
  guint acat;
  chunk_size = P2ALIGN (mem_size);
  acat = allocator_categorize (chunk_size);
  if (G_LIKELY (acat == 1))     /* allocate through magazine layer */
    {
      ThreadMemory *tmem = thread_memory_from_self();
      guint ix = SLAB_INDEX (allocator, chunk_size);
      if (G_UNLIKELY (thread_memory_magazine1_is_empty (tmem, ix)))
        {
          thread_memory_swap_magazines (tmem, ix);
          if (G_UNLIKELY (thread_memory_magazine1_is_empty (tmem, ix)))
            thread_memory_magazine1_reload (tmem, ix);
        }
      mem = thread_memory_magazine1_alloc (tmem, ix);
    }
  else if (acat == 2)           /* allocate through slab allocator */
    {
      g_mutex_lock (allocator->slab_mutex);
      mem = slab_allocator_alloc_chunk (chunk_size);
      g_mutex_unlock (allocator->slab_mutex);
    }
  else                          /* delegate to system malloc */
    mem = g_malloc (mem_size);
  return mem;
}

gpointer
g_slice_alloc0 (gsize mem_size)
{
  gpointer mem = g_slice_alloc (mem_size);
  if (mem)
    memset (mem, 0, mem_size);
  return mem;
}

void
g_slice_free1 (gsize    mem_size,
               gpointer mem_block)
{
  gsize chunk_size = P2ALIGN (mem_size);
  guint acat = allocator_categorize (chunk_size);
  if (G_UNLIKELY (!mem_block))
    /* pass */;
  else if (G_LIKELY (acat == 1))        /* allocate through magazine layer */
    {
      ThreadMemory *tmem = thread_memory_from_self();
      guint ix = SLAB_INDEX (allocator, chunk_size);
      if (G_UNLIKELY (thread_memory_magazine2_is_full (tmem, ix)))
        {
          thread_memory_swap_magazines (tmem, ix);
          if (G_UNLIKELY (thread_memory_magazine2_is_full (tmem, ix)))
            thread_memory_magazine2_unload (tmem, ix);
        }
      thread_memory_magazine2_free (tmem, ix, mem_block);
    }
  else if (acat == 2)                   /* allocate through slab allocator */
    {
      g_mutex_lock (allocator->slab_mutex);
      slab_allocator_free_chunk (chunk_size, mem_block);
      g_mutex_unlock (allocator->slab_mutex);
    }
  else                                  /* delegate to system malloc */
    g_free (mem_block);
}

void
g_slice_free_chain_with_offset (gsize    mem_size,
                                gpointer mem_chain,
                                gsize    next_offset)
{
  gpointer slice = mem_chain;
  /* while the thread magazines and the magazine cache are implemented so that
   * they can easily be extended to allow for free lists containing more free
   * lists for the first level nodes, which would allow O(1) freeing in this
   * function, the benefit of such an extension is questionable, because:
   * - the magazine size counts will become mere lower bounds which confuses
   *   the code adapting to lock contention;
   * - freeing a single node to the thread magazines is very fast, so this
   *   O(list_length) operation is multiplied by a fairly small factor;
   * - memory usage histograms on larger applications seem to indicate that
   *   the amount of released multi node lists is negligible in comparison
   *   to single node releases.
   * - the major performance bottle neck, namely g_private_get() or
   *   g_mutex_lock()/g_mutex_unlock() has already been moved out of the
   *   inner loop for freeing chained slices.
   */
  gsize chunk_size = P2ALIGN (mem_size);
  guint acat = allocator_categorize (chunk_size);
  if (G_LIKELY (acat == 1))             /* allocate through magazine layer */
    {
      ThreadMemory *tmem = thread_memory_from_self();
      guint ix = SLAB_INDEX (allocator, chunk_size);
      while (slice)
        {
          guint8 *current = slice;
          slice = *(gpointer*) (current + next_offset);
          if (G_UNLIKELY (thread_memory_magazine2_is_full (tmem, ix)))
            {
              thread_memory_swap_magazines (tmem, ix);
              if (G_UNLIKELY (thread_memory_magazine2_is_full (tmem, ix)))
                thread_memory_magazine2_unload (tmem, ix);
            }
          thread_memory_magazine2_free (tmem, ix, current);
        }
    }
  else if (acat == 2)                   /* allocate through slab allocator */
    {
      g_mutex_lock (allocator->slab_mutex);
      while (slice)
        {
          guint8 *current = slice;
          slice = *(gpointer*) (current + next_offset);
          slab_allocator_free_chunk (chunk_size, current);
        }
      g_mutex_unlock (allocator->slab_mutex);
    }
  else                                  /* delegate to system malloc */
    while (slice)
      {
        guint8 *current = slice;
        slice = *(gpointer*) (current + next_offset);
        g_free (current);
      }
}

/* --- single page allocator --- */
static void
allocator_slab_stack_push (Allocator *allocator,
                           guint      ix,
                           SlabInfo  *sinfo)
{
  /* insert slab at slab ring head */
  if (!allocator->slab_stack[ix])
    {
      sinfo->next = sinfo;
      sinfo->prev = sinfo;
    }
  else
    {
      SlabInfo *next = allocator->slab_stack[ix], *prev = next->prev;
      next->prev = sinfo;
      prev->next = sinfo;
      sinfo->next = next;
      sinfo->prev = prev;
    }
  allocator->slab_stack[ix] = sinfo;
}

static void
allocator_add_slab (Allocator *allocator,
                    guint      ix,
                    gsize      chunk_size)
{
  ChunkLink *chunk;
  SlabInfo *sinfo;
  gsize addr, padding, n_chunks, color = 0;
  gsize page_size = SLAB_PAGE_SIZE (allocator, chunk_size);
  /* allocate 1 page for the chunks and the slab */
  gpointer aligned_memory = allocator_memalign (page_size, page_size - NATIVE_MALLOC_PADDING);
  guint8 *mem = aligned_memory;
  guint i;
  if (!mem)
    g_error ("%s: failed to allocate %lu bytes: %s", "GSlicedMemory", (gulong) (page_size - NATIVE_MALLOC_PADDING), g_strerror (errno));
  /* mask page adress */
  addr = ((gsize) mem / page_size) * page_size;
  /* assert alignment */
  g_assert (aligned_memory == (gpointer) addr);
  /* basic slab info setup */
  sinfo = (SlabInfo*) (mem + page_size - SLAB_INFO_SIZE);
  sinfo->n_allocated = 0;
  sinfo->chunks = NULL;
  /* figure cache colorization */
  n_chunks = ((guint8*) sinfo - mem) / chunk_size;
  padding = ((guint8*) sinfo - mem) - n_chunks * chunk_size;
  if (padding)
    {
      color = (allocator->color_accu * P2ALIGNMENT) % padding;
      allocator->color_accu += 1;       /* alternatively: + 0x7fffffff */
    }
  /* add chunks to free list */
  chunk = (ChunkLink*) (mem + color);
  sinfo->chunks = chunk;
  for (i = 0; i < n_chunks - 1; i++)
    {
      chunk->next = (ChunkLink*) ((guint8*) chunk + chunk_size);
      chunk = chunk->next;
    }
  chunk->next = NULL;   /* last chunk */
  /* add slab to slab ring */
  allocator_slab_stack_push (allocator, ix, sinfo);
}

static gpointer
slab_allocator_alloc_chunk (gsize chunk_size)
{
  ChunkLink *chunk;
  guint ix = SLAB_INDEX (allocator, chunk_size);
  /* ensure non-empty slab */
  if (!allocator->slab_stack[ix] || !allocator->slab_stack[ix]->chunks)
    allocator_add_slab (allocator, ix, chunk_size);
  /* allocate chunk */
  chunk = allocator->slab_stack[ix]->chunks;
  allocator->slab_stack[ix]->chunks = chunk->next;
  allocator->slab_stack[ix]->n_allocated++;
  /* rotate empty slabs */
  if (!allocator->slab_stack[ix]->chunks)
    allocator->slab_stack[ix] = allocator->slab_stack[ix]->next;
  return chunk;
}

static void
slab_allocator_free_chunk (gsize    chunk_size,
                           gpointer mem)
{
  ChunkLink *chunk;
  gboolean was_empty;
  guint ix = SLAB_INDEX (allocator, chunk_size);
  gsize page_size = SLAB_PAGE_SIZE (allocator, chunk_size);
  gsize addr = ((gsize) mem / page_size) * page_size;
  /* mask page adress */
  guint8 *page = (guint8*) addr;
  SlabInfo *sinfo = (SlabInfo*) (page + page_size - SLAB_INFO_SIZE);
  /* assert valid chunk count */
  g_assert (sinfo->n_allocated > 0);
  /* add chunk to free list */
  was_empty = sinfo->chunks == NULL;
  chunk = (ChunkLink*) mem;
  chunk->next = sinfo->chunks;
  sinfo->chunks = chunk;
  sinfo->n_allocated--;
  /* keep slab ring partially sorted, empty slabs at end */
  if (was_empty)
    {
      /* unlink slab */
      SlabInfo *next = sinfo->next, *prev = sinfo->prev;
      next->prev = prev;
      prev->next = next;
      if (allocator->slab_stack[ix] == sinfo)
        allocator->slab_stack[ix] = next == sinfo ? NULL : next;
      /* insert slab at head */
      allocator_slab_stack_push (allocator, ix, sinfo);
    }
  /* eagerly free complete unused slabs */
  if (!sinfo->n_allocated)
    {
      /* unlink slab */
      SlabInfo *next = sinfo->next, *prev = sinfo->prev;
      next->prev = prev;
      prev->next = next;
      if (allocator->slab_stack[ix] == sinfo)
        allocator->slab_stack[ix] = next == sinfo ? NULL : next;
      /* free slab */
      allocator_memfree (page_size, page);
    }
}

/* --- memalign implementation --- */
#include <malloc.h>             /* memalign() */

/* from config.h:
 * define HAVE_POSIX_MEMALIGN     1     // if free(posix_memalign(3)) works, <stdlib.h>
 * define HAVE_MEMALIGN           1     // if free(memalign(3)) works, <malloc.h>
 * define HAVE_VALLOC             1     // if free(valloc(3)) works, <stdlib.h> or <malloc.h>
 * if none is provided, we implement malloc(3)-based alloc-only page alignment
 */

#if !(HAVE_POSIX_MEMALIGN || HAVE_MEMALIGN || HAVE_VALLOC)
static GTrashStack *compat_valloc_trash = NULL;
#endif

static gpointer
allocator_memalign (gsize alignment,
                    gsize memsize)
{
  gpointer aligned_memory = NULL;
  gint err = ENOMEM;
#if     HAVE_POSIX_MEMALIGN
  err = posix_memalign (&aligned_memory, alignment, memsize);
#elif   HAVE_MEMALIGN
  errno = 0;
  aligned_memory = memalign (alignment, memsize);
  err = errno;
#elif   HAVE_VALLOC
  errno = 0;
  aligned_memory = valloc (memsize);
  err = errno;
#else
  /* simplistic non-freeing page allocator */
  g_assert (alignment == sys_page_size);
  g_assert (memsize <= sys_page_size);
  if (!compat_valloc_trash)
    {
      const guint n_pages = 16;
      guint8 *mem = malloc (n_pages * sys_page_size);
      err = errno;
      if (mem)
        {
          gint i = n_pages;
          guint8 *amem = (guint8*) ALIGN ((gsize) mem, sys_page_size);
          if (amem != mem)
            i--;        /* mem wasn't page aligned */
          while (--i >= 0)
            g_trash_stack_push (&compat_valloc_trash, amem + i * sys_page_size);
        }
    }
  aligned_memory = g_trash_stack_pop (&compat_valloc_trash);
#endif
  if (!aligned_memory)
    errno = err;
  return aligned_memory;
}

static void
allocator_memfree (gsize    memsize,
                   gpointer mem)
{
#if     HAVE_POSIX_MEMALIGN || HAVE_MEMALIGN || HAVE_VALLOC
  free (mem);
#else
  g_assert (memsize <= sys_page_size);
  g_trash_stack_push (&compat_valloc_trash, mem);
#endif
}

#define __G_SLICE_C__
#include "galiasdef.c"
