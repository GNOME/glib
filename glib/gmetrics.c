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
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <locale.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <sys/types.h>

#define uthash_malloc(_s) g_metrics_allocate(_s)
#define uthash_free(_p,_s) g_metrics_free(_p)
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

#ifdef G_OS_UNIX
#include <unistd.h>
#endif

#include <zlib.h>

#define round_to_multiple(n, m) (((n) + (((m) - 1))) & ~((m) - 1))

struct _GMetricsFile
{
  gzFile gzipped_file;
  char *static_format_string;
  char *variadic_format_string;
  gdouble now;
  gulong generation;
};

typedef struct _GMetricsAllocationHeader GMetricsAllocationHeader;
struct _GMetricsAllocationHeader
{
  guint32 is_freed;
  gsize number_of_blocks;
};

typedef union _GMetricsAllocationBlock GMetricsAllocationBlock;
union _GMetricsAllocationBlock
{
  GMetricsAllocationHeader header;
  char minimum_alignment[32];
};

typedef struct _GMetricsAllocation GMetricsAllocation;
struct _GMetricsAllocation
{
  GMetricsAllocationBlock header_block;
  GMetricsAllocationBlock payload_blocks[0];
};

typedef struct _GMetricsAllocationBlocksIter GMetricsAllocationBlocksIter;
struct _GMetricsAllocationBlocksIter
{
  gsize index;
  gsize items_examined;
};

static volatile gboolean needs_flush = FALSE;
static int timeout_fd = -1;
GMetricsList *timeout_handlers;
G_LOCK_DEFINE_STATIC (timeouts);
static int metrics_map_fd;
static char *metrics_map = MAP_FAILED;
static gsize metrics_map_size;
static GMetricsAllocationBlock *allocation_blocks, *last_block_allocated;
static gsize number_of_allocation_blocks;
G_LOCK_DEFINE_STATIC (allocations);

gboolean
g_metrics_enabled (void)
{
  static char cmdline[1024] = { 0 };
  static gboolean has_command_line;
  const char *requested_command;
  if (!has_command_line)
    {
      int fd;
      has_command_line = TRUE;

      fd = open ("/proc/self/cmdline", O_RDONLY);
      if (fd >= 0)
        read (fd, cmdline, 1023);
    }

  requested_command = getenv ("G_METRICS_COMMAND")? : "gnome-shell";
  if (!g_str_has_suffix (cmdline, requested_command))
    return FALSE;

  return TRUE;
}

static void
initialize_metrics_map (void)
{
  char *filename;
  int result;

  asprintf (&filename,"/var/tmp/user-%d-for-pid-%d-metrics.map", getuid (), getpid ());

  metrics_map_size = strtol (getenv ("G_METRICS_MAP_SIZE")? : "10485760", NULL, 10) * 1024L;
  if (!metrics_map_size)
    metrics_map_size = 10485760L * 1024;

  unlink (filename);
  metrics_map_fd = open (filename, O_RDWR | O_CREAT, 0644);
  free (filename);

  if (metrics_map_fd < 0)
    return;

  result = ftruncate (metrics_map_fd, metrics_map_size);

  if (result < 0)
    goto fail;

  metrics_map = mmap (NULL, metrics_map_size, PROT_READ | PROT_WRITE, MAP_SHARED, metrics_map_fd, 0);

  if (metrics_map == MAP_FAILED)
    goto fail;

  number_of_allocation_blocks = metrics_map_size / sizeof (GMetricsAllocationBlock);

  allocation_blocks = (GMetricsAllocationBlock *) metrics_map;
  allocation_blocks[0].header.number_of_blocks = number_of_allocation_blocks;
  allocation_blocks[0].header.is_freed = TRUE;

  last_block_allocated = NULL;

  return;
fail:
  close (metrics_map_fd);
  metrics_map_fd = -1;
  metrics_map_size = 0;
}

static void
g_metrics_init (void)
{
  static gboolean initialized = 0;

  if (!g_metrics_enabled ())
    initialized = TRUE;

  if (initialized)
    return;

  initialize_metrics_map ();
  initialized = TRUE;

  G_LOCK (timeouts);
  if (timeout_handlers == NULL)
    timeout_handlers = g_metrics_list_new ();
  G_UNLOCK (timeouts);
}

static gsize
g_metrics_allocation_block_get_index (GMetricsAllocationBlock *block)
{
  return block - allocation_blocks;
}

static void
g_metrics_allocation_blocks_iter_init_after_block (GMetricsAllocationBlocksIter *iter,
                                                   GMetricsAllocationBlock      *block)
{
  if (block == NULL)
    iter->index = 0;
  else
    iter->index = g_metrics_allocation_block_get_index (block);

  iter->items_examined = 0;
}

static void
g_metrics_allocation_blocks_iter_init (GMetricsAllocationBlocksIter *iter)
{
  g_metrics_allocation_blocks_iter_init_after_block (iter, NULL);
}

static gboolean
g_metrics_allocation_blocks_iter_next (GMetricsAllocationBlocksIter  *iter,
                                       GMetricsAllocationBlock      **next_block)
{
  GMetricsAllocationBlock *block;
  GMetricsAllocation *allocation;
  GMetricsAllocationHeader *header;

  if (iter->items_examined >= number_of_allocation_blocks)
    return FALSE;

  block = &allocation_blocks[iter->index];

  if (next_block)
    *next_block = block;

  iter->items_examined++;

  allocation = (GMetricsAllocation *) block;
  header = (GMetricsAllocationHeader *) &allocation->header_block.header;

  iter->index += header->number_of_blocks;
  iter->index %= number_of_allocation_blocks;

  return TRUE;
}

static void
g_metrics_allocation_shrink (GMetricsAllocation *allocation,
                             gsize               number_of_blocks)
{
  GMetricsAllocationHeader *header;
  GMetricsAllocationBlock *first_block, *next_block;
  GMetricsAllocation *next_allocation;
  GMetricsAllocationHeader *next_header;
  gsize blocks_left;

  header = &allocation->header_block.header;
  first_block = (GMetricsAllocationBlock *) allocation;

  blocks_left = header->number_of_blocks - number_of_blocks;
  header->number_of_blocks = number_of_blocks;

  if (blocks_left > 0)
    {
      next_block = first_block + number_of_blocks;
      next_allocation = (GMetricsAllocation *) next_block;
      next_header = &next_allocation->header_block.header;

      next_header->number_of_blocks = blocks_left;
      next_header->is_freed = TRUE;
    }
}

#if 0
static gboolean
g_metrics_allocation_block_starts_allocation (GMetricsAllocationBlock *dubious_block)
{
  GMetricsAllocationBlocksIter iter;
  GMetricsAllocationBlock *block = NULL;

  g_metrics_allocation_blocks_iter_init (&iter);
  while (g_metrics_allocation_blocks_iter_next (&iter, &block))
    {
      if (block >= dubious_block)
        break;
    }

  return block == dubious_block;
r
#endif

static GMetricsAllocation *
get_allocation (gsize number_of_blocks)
{
  GMetricsAllocationBlocksIter iter;
  GMetricsAllocationBlock *block;

  g_metrics_allocation_blocks_iter_init_after_block (&iter, last_block_allocated);
  while (g_metrics_allocation_blocks_iter_next (&iter, &block))
    {
      GMetricsAllocation *allocation;
      GMetricsAllocationHeader *header;
      gsize free_blocks_ahead;
      GMetricsAllocationBlocksIter look_ahead_iter;
      GMetricsAllocationBlock *look_ahead_block;

      allocation = (GMetricsAllocation *) block;
      header = &allocation->header_block.header;

      if (!header->is_freed)
        continue;

      if (number_of_blocks == header->number_of_blocks)
        {
          header->is_freed = FALSE;
          last_block_allocated = block;
          return allocation;
        }

      if (number_of_blocks < header->number_of_blocks)
        {
          header->is_freed = FALSE;
          g_metrics_allocation_shrink (allocation, number_of_blocks);

          last_block_allocated = block;
          return allocation;
        }

      free_blocks_ahead = 0;
      g_metrics_allocation_blocks_iter_init_after_block (&look_ahead_iter, block);
      while (g_metrics_allocation_blocks_iter_next (&look_ahead_iter, &look_ahead_block))
        {
          GMetricsAllocation *look_ahead_allocation;
          GMetricsAllocationHeader *look_ahead_header;

          look_ahead_allocation = (GMetricsAllocation *) look_ahead_block;
          look_ahead_header = &look_ahead_allocation->header_block.header;

          if (!look_ahead_header->is_freed)
            break;

          free_blocks_ahead += look_ahead_header->number_of_blocks;

          if (number_of_blocks <= free_blocks_ahead)
            break;
        }

      if (number_of_blocks == free_blocks_ahead)
        {
          header->number_of_blocks = number_of_blocks;
          header->is_freed = FALSE;

          last_block_allocated = block;
          return allocation;
        }

      if (number_of_blocks < free_blocks_ahead)
        {
          header->number_of_blocks = free_blocks_ahead;
          header->is_freed = FALSE;
          g_metrics_allocation_shrink (allocation, number_of_blocks);

          last_block_allocated = block;
          return allocation;
        }
    }

  return NULL;
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

gpointer
g_metrics_allocate (gsize size)
{
  GMetricsAllocation *allocation;
  gsize needed_blocks;

  g_metrics_init ();

  if (!g_metrics_enabled ())
    return calloc (1, size);

  needed_blocks = calculate_blocks_needed_for_size (size);

  G_LOCK (allocations);
  allocation = get_allocation (needed_blocks);
  G_UNLOCK (allocations);

  if (allocation == NULL)
    G_BREAKPOINT ();

  memset (allocation->payload_blocks, 0, size);
  return (gpointer) allocation->payload_blocks;
}

static gboolean
g_metrics_allocation_grow (GMetricsAllocation *allocation,
                           gsize               number_of_blocks)
{
  GMetricsAllocationHeader *header;
  GMetricsAllocationBlock *first_block;
  gsize free_blocks_ahead;
  gsize block_index, look_ahead_index;

  header = &allocation->header_block.header;
  first_block = (GMetricsAllocationBlock *) allocation;

  block_index = g_metrics_allocation_block_get_index (first_block);
  look_ahead_index = block_index + header->number_of_blocks;

  free_blocks_ahead = header->number_of_blocks;
  while (look_ahead_index < number_of_allocation_blocks)
    {
      GMetricsAllocationBlock *next_block = &allocation_blocks[look_ahead_index];

      if (!next_block->header.is_freed)
        break;

      free_blocks_ahead += next_block->header.number_of_blocks;
      look_ahead_index += next_block->header.number_of_blocks;

      if (free_blocks_ahead >= number_of_blocks)
        break;
    }

  if (free_blocks_ahead < number_of_blocks)
    return FALSE;

  header->number_of_blocks = free_blocks_ahead;

  if (header->number_of_blocks > number_of_blocks)
    g_metrics_allocation_shrink (allocation, number_of_blocks);

  return TRUE;
}

static gsize
g_metrics_allocation_get_payload_size (GMetricsAllocation *allocation)
{
  GMetricsAllocationHeader *header;
  header = &allocation->header_block.header;

  return (header->number_of_blocks * sizeof (GMetricsAllocationBlock)) - sizeof (allocation->header_block);
}

gpointer
g_metrics_reallocate (gpointer payload,
                      gsize    size)
{
  GMetricsAllocationBlock *payload_blocks;
  GMetricsAllocationBlock *first_block;
  GMetricsAllocation *allocation;
  GMetricsAllocationHeader *header;
  gsize old_size;
  gsize needed_blocks;
  gpointer new_payload;
  gboolean could_grow;

  g_metrics_init ();

  if (!g_metrics_enabled ())
    return realloc (payload, size);

  if (size == 0)
    {
      g_metrics_free (payload);
      return NULL;
    }

  if (payload == NULL)
    return g_metrics_allocate (size);

  G_LOCK (allocations);
  payload_blocks = (GMetricsAllocationBlock *) payload;
  first_block = payload_blocks - offsetof (GMetricsAllocation, payload_blocks) / sizeof (GMetricsAllocationBlock);
  allocation = (GMetricsAllocation *) first_block;
  header = &allocation->header_block.header;
  needed_blocks = calculate_blocks_needed_for_size (size);

  if (needed_blocks == header->number_of_blocks)
    {
      G_UNLOCK (allocations);
      return payload;
    }

  if (needed_blocks < header->number_of_blocks)
    {
      g_metrics_allocation_shrink (allocation, needed_blocks);
      G_UNLOCK (allocations);
      return payload;
    }

  could_grow = g_metrics_allocation_grow (allocation, needed_blocks);

  if (could_grow &&
      last_block_allocated > first_block &&
      last_block_allocated < first_block + header->number_of_blocks)
    last_block_allocated = first_block;
  G_UNLOCK (allocations);

  if (could_grow)
    return payload;

  old_size = g_metrics_allocation_get_payload_size (allocation);

  new_payload = g_metrics_allocate (size);
  memcpy (new_payload, payload, old_size);
  g_metrics_free (payload);

  return new_payload;
}

gpointer
g_metrics_copy (gconstpointer allocation,
                gsize size)
{
  gpointer copy;

  copy = g_metrics_allocate (size);
  memcpy (copy, allocation, size);

  return copy;
}

extern void *__libc_free (void *ptr);

__attribute__((visibility("default")))
void
free (void *ptr)
{
    if (metrics_map_size > 0)
      {
        if (ptr >= (gpointer) metrics_map && ptr < (gpointer) (metrics_map + metrics_map_size))
        {
            g_metrics_free (ptr);
            return;
        }
      }

  __libc_free (ptr);
}

void
g_metrics_free (gpointer payload)
{
  GMetricsAllocationBlock *payload_blocks;
  GMetricsAllocationBlock *first_block;
  GMetricsAllocation *allocation;
  GMetricsAllocationHeader *header;

  if (!payload)
    return;

  if (payload < (gpointer) metrics_map || payload > (gpointer) (metrics_map + metrics_map_size))
    {
      free (payload);
      return;
    }

  payload_blocks = (GMetricsAllocationBlock *) payload;
  first_block = payload_blocks - offsetof (GMetricsAllocation, payload_blocks) / sizeof (GMetricsAllocationBlock);
  allocation = (GMetricsAllocation *) first_block;
  header = &allocation->header_block.header;

  if (header->is_freed)
    G_BREAKPOINT ();

  header->is_freed = TRUE;
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
  char *dir = NULL;
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

  metrics_file = g_metrics_allocate (sizeof (GMetricsFile));
  asprintf (&metrics_file->static_format_string,"%%lu,%%lf,%s",
                                                        first_column_format);
  metrics_file->variadic_format_string = strdup (utstring_body (format_string));

  asprintf (&dir,"%s/logs/%u", g_get_user_cache_dir (), getpid ());
  asprintf (&filename,"%s/%s.csv.gz", dir, name);
  g_mkdir_with_parents (dir, 0755);

  metrics_file->gzipped_file = gzopen (filename, "wbe");
  g_metrics_file_write (metrics_file, utstring_body (header_string), utstring_len (header_string));
  utstring_free (format_string);
  utstring_free (header_string);
  free (dir);
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

  row_length += snprintf (NULL, 0, metrics_file->static_format_string, metrics_file->generation, metrics_file->now, first_column_value);

  va_start (args, first_column_value);
  row_length += vsnprintf (NULL, 0, metrics_file->variadic_format_string, args);
  va_end (args);

  row_length++;

  buffer_left = row_length + 1;
  row = g_metrics_allocate (buffer_left);

  buffer_written += snprintf (row, buffer_left, metrics_file->static_format_string, metrics_file->generation, metrics_file->now, first_column_value);
  buffer_left -= buffer_written;

  va_start (args, first_column_value);
  buffer_written += vsnprintf (row + buffer_written, buffer_left, metrics_file->variadic_format_string, args);
  buffer_left -= buffer_written;
  va_end (args);
  *(row + buffer_written) = '\n';

  g_metrics_file_write (metrics_file, row, row_length);
  g_metrics_free (row);
}
#pragma GCC diagnostic pop

void
g_metrics_file_end_record (GMetricsFile *metrics_file)
{
  metrics_file->generation++;

  if (needs_flush)
    {
      gzflush (metrics_file->gzipped_file, Z_FULL_FLUSH);
    }
  else if ((metrics_file->generation % 10) == 0)
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
  g_metrics_free (metrics_file);
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
};

GMetricsTable *
g_metrics_table_new (gsize record_size)
{
  GMetricsTable *table;

  g_metrics_init ();

  table = g_metrics_allocate (sizeof (GMetricsTable));
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
      entry = g_metrics_allocate (sizeof (GMetricsTableEntry));
      entry->name = g_metrics_copy (name, strlen (name) + 1);
      entry->record = g_metrics_copy (record, metrics_table->record_size);

      HASH_ADD_KEYPTR (hh, metrics_table->entries, entry->name, strlen (entry->name), entry);
    }
  else
    {
      gpointer old_record;
      old_record = entry->record;
      entry->record = g_metrics_copy (record, metrics_table->record_size);
      g_metrics_free (old_record);
    }
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
  g_metrics_free (entry->name);
  g_metrics_free (entry->record);
  g_metrics_free (entry);
}

void
g_metrics_table_clear (GMetricsTable *metrics_table)
{
  GMetricsTableEntry *entry = NULL, *next_entry = NULL;

  HASH_ITER (hh, metrics_table->entries, entry, next_entry)
    {
      HASH_DEL (metrics_table->entries, entry);
      g_metrics_free (entry->name);
      g_metrics_free (entry->record);
      g_metrics_free (entry);
    }
}

void
g_metrics_table_free (GMetricsTable *metrics_table)
{
  g_metrics_table_clear (metrics_table);
  g_metrics_free (metrics_table);
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
  return comparison_function (entry_1->record, entry_2->record);
}

void
g_metrics_table_sorted_iter_init (GMetricsTableIter *iter,
                                  GMetricsTable     *table,
                                  GCompareFunc       comparison_function)
{
  HASH_SRT_DATA (hh, table->entries, comparison_wrapper, comparison_function);
  iter->entry = table->entries;
}

gboolean
g_metrics_table_iter_next_without_cast (GMetricsTableIter  *iter,
                                        const char        **name,
                                        gpointer           *record)
{
  if (iter->entry == NULL)
    return FALSE;

  if (name)
    *name = (const char *) iter->entry->name;

  if (iter->entry->record == NULL)
    G_BREAKPOINT ();

  if (record)
    *record = iter->entry->record;

  iter->entry = iter->entry->hh.next;
  return TRUE;
}

gboolean
g_metrics_requested (const char *name)
{
  const char *skipped_metrics;

  if (!g_metrics_enabled ())
    return FALSE;

  skipped_metrics = getenv ("G_METRICS_SKIP");

  if (skipped_metrics != NULL && strstr (skipped_metrics, name) != NULL)
    return FALSE;

  return TRUE;
}

void
g_metrics_start_timeout (GMetricsTimeoutFunc timeout_handler)
{
  guint timeout_interval;

  G_LOCK (timeouts);

  if (timeout_fd < 0)
    {
      struct itimerspec timer_spec = { { 0 } };

      timeout_interval = atoi (getenv ("G_METRICS_INTERVAL")?: "10");

      timer_spec.it_interval.tv_sec = timeout_interval;
      timer_spec.it_value.tv_sec = timeout_interval;


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

void
g_metrics_run_timeout_handlers (void)
{
  GMetricsListIter iter;
  GMetricsTimeoutFunc handler;

  guint64 number_of_expirations;

  read (timeout_fd, &number_of_expirations, sizeof (number_of_expirations));

  G_LOCK (timeouts);
  g_metrics_list_iter_init (&iter, timeout_handlers);
  while (g_metrics_list_iter_next (&iter, &handler))
    {
      if (handler != NULL)
        handler ();
    }
  G_UNLOCK (timeouts);

  needs_flush = FALSE;
}

typedef struct _GMetricsListNode GMetricsListNode;
struct _GMetricsListNode
{
  gpointer item;
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

  list = g_metrics_allocate (sizeof (GMetricsList));

  return list;
}

void
g_metrics_list_add_item (GMetricsList *list,
                         gpointer      item)
{
  GMetricsListNode *node;

  node = g_metrics_allocate (sizeof (GMetricsListNode));
  node->item = item;

  LL_APPEND (list->first_node, node);
  list->number_of_nodes++;
}

void
g_metrics_list_remove_item (GMetricsList *list,
                            gpointer      item_to_remove)
{
  GMetricsListNode *node = NULL;

  LL_SEARCH_SCALAR (list->first_node, node, item, item_to_remove);

  if (node != NULL)
    {
      LL_DELETE (list->first_node, node);
      list->number_of_nodes--;
    }
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

  LL_FOREACH_SAFE (list->first_node, node, next_node)
    {
      LL_DELETE (list->first_node, node);
      g_metrics_free (node);
    }
  g_metrics_free (list);
}

void
g_metrics_list_iter_init (GMetricsListIter *iter,
                          GMetricsList     *list)
{
  iter->node = list->first_node;
}

gboolean
g_metrics_list_iter_next_without_cast (GMetricsListIter *iter,
                                       gpointer         *item)
{
  if (iter->node == NULL)
    return FALSE;

  *item = iter->node->item;

  iter->node = iter->node->next;
  return TRUE;
}

char *
g_metrics_stack_trace (void)
{
  static gsize trace_size = 0;
  static gsize sample_interval = 0, sample_index = 0;
  int number_of_frames, i;
  gpointer *frames;
  char **symbols;
  UT_string *output_string = NULL;
  char *output;

  if (trace_size == 0)
    trace_size = atoi (getenv ("G_METRICS_STACK_TRACE_SIZE")? : "3");

  if (trace_size == 0)
    return NULL;

  if (sample_interval == 0)
    sample_interval = atoi (getenv ("G_METRICS_STACK_TRACE_SAMPLE_INTERVAL")? : "1000");

  if ((sample_index % sample_interval) != 0)
    {
      sample_index++;
      return NULL;
    }

  sample_index++;

  frames = g_metrics_allocate (sizeof (gpointer) * (trace_size + 2));
  number_of_frames = backtrace (frames, trace_size + 2);

  if (number_of_frames <= 0)
    return NULL;

  symbols = backtrace_symbols (frames, number_of_frames);

  utstring_new (output_string);
  for (i = 2; i < number_of_frames; i++)
    utstring_printf (output_string, "%s -> ", symbols[i]);

  output = g_metrics_copy (utstring_body (output_string), utstring_len (output_string) + 1);
  utstring_free (output_string);

  free (symbols);
  g_metrics_free (frames);

  return output;
}

int
g_metrics_get_timeout_fd (void)
{
  return timeout_fd;
}
