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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#ifndef __G_METRICS_H__
#define __G_METRICS_H__

#if !defined (__GLIB_H_INSIDE__) && !defined (GLIB_COMPILATION)
#error "Only <glib.h> can be included directly."
#endif

#include <stdarg.h>
#include <glib/gtypes.h>
#include <glib/gmacros.h>
#include <glib/gmain.h>

G_BEGIN_DECLS

GLIB_AVAILABLE_IN_ALL
void g_metrics_init (void);

GLIB_AVAILABLE_IN_ALL
gboolean g_metrics_enabled (void);

GLIB_AVAILABLE_IN_ALL
gboolean g_metrics_requested (const char *name);

typedef void (*GMetricsTimeoutFunc) (void);

GLIB_AVAILABLE_IN_ALL
int g_metrics_get_timeout_fd (void);

GLIB_AVAILABLE_IN_ALL
void g_metrics_run_timeout_handlers (void);

GLIB_AVAILABLE_IN_ALL
gulong g_metrics_get_generation (void);

GLIB_AVAILABLE_IN_ALL
void g_metrics_start_timeout (GMetricsTimeoutFunc  timeout_handler);

GLIB_AVAILABLE_IN_ALL
const char *g_metrics_get_log_dir (void);

typedef struct _GMetricsFile GMetricsFile;
GLIB_AVAILABLE_IN_ALL
GMetricsFile *g_metrics_file_new (const char *name,
                                  const char *first_column_name,
                                  const char *first_column_format,
                                  ...);
GLIB_AVAILABLE_IN_ALL
void g_metrics_file_start_record (GMetricsFile *metrics_file);
GLIB_AVAILABLE_IN_ALL
void g_metrics_file_add_row_without_cast (GMetricsFile *metrics_file,
                                          gconstpointer first_column_value,
                                          ...);
#define g_metrics_file_add_row(_file,_first,_rest...) g_metrics_file_add_row_without_cast ((_file), (gpointer) (_first), _rest)
GLIB_AVAILABLE_IN_ALL
void g_metrics_file_end_record (GMetricsFile *metrics_file);
GLIB_AVAILABLE_IN_ALL
void g_metrics_file_free (GMetricsFile *metrics_file);

typedef struct _GMetricsTable GMetricsTable;
GLIB_AVAILABLE_IN_ALL
GMetricsTable *g_metrics_table_new (gsize record_size);
GLIB_AVAILABLE_IN_ALL
void g_metrics_table_set_record (GMetricsTable *metrics_table,
                                 const char    *name,
                                 gpointer       record);
GLIB_AVAILABLE_IN_ALL
gpointer g_metrics_table_get_record (GMetricsTable *metrics_table,
                                     const char    *name);
GLIB_AVAILABLE_IN_ALL
void g_metrics_table_remove_record (GMetricsTable *metrics_table,
                                    const char    *name);
GLIB_AVAILABLE_IN_ALL
void g_metrics_table_clear (GMetricsTable *metrics_table);

GLIB_AVAILABLE_IN_ALL
void g_metrics_table_free (GMetricsTable *metrics_table);

typedef struct _GMetricsTableIter GMetricsTableIter;

typedef struct _GMetricsTableEntry GMetricsTableEntry;
struct _GMetricsTableIter
{
  /*< private >*/
  GMetricsTableEntry *entry;
};
GLIB_AVAILABLE_IN_ALL
void g_metrics_table_iter_init (GMetricsTableIter *iter,
                                GMetricsTable     *table);
GLIB_AVAILABLE_IN_ALL
void g_metrics_table_sorted_iter_init (GMetricsTableIter *iter,
                                       GMetricsTable     *table,
                                       GCompareFunc       comparison_function);
GLIB_AVAILABLE_IN_ALL
gboolean g_metrics_table_iter_next_without_cast (GMetricsTableIter  *iter,
                                                 const char        **name,
                                                 gpointer           *record);
#define g_metrics_table_iter_next(_iter,_name,_record) g_metrics_table_iter_next_without_cast ((_iter), (_name), (gpointer *) (_record))

struct _GMetricsInstanceCounterMetrics
{
  char    comment[64];
  gsize  total_memory_usage;
  gsize  instance_count;
  gssize instance_change;
  gsize  instance_watermark;
  gssize average_instance_change;
  gsize  number_of_samples;
};
typedef struct _GMetricsInstanceCounterMetrics GMetricsInstanceCounterMetrics;

typedef struct _GMetricsInstanceCounter GMetricsInstanceCounter;

GLIB_AVAILABLE_IN_ALL
GMetricsInstanceCounter *g_metrics_instance_counter_new (void);

GLIB_AVAILABLE_IN_ALL
void g_metrics_instance_counter_start_record (GMetricsInstanceCounter *counter);
GLIB_AVAILABLE_IN_ALL
void g_metrics_instance_counter_add_instance (GMetricsInstanceCounter *counter,
                                              const char              *name,
                                              gsize                   memory_usage);

GLIB_AVAILABLE_IN_ALL
void g_metrics_instance_counter_add_instances (GMetricsInstanceCounter *counter,
                                               const char              *name,
                                               const char              *comment,
                                               gsize                   number_of_instances,
                                               gsize                   total_usage);
GLIB_AVAILABLE_IN_ALL
void g_metrics_instance_counter_end_record (GMetricsInstanceCounter *counter);

GLIB_AVAILABLE_IN_ALL
gboolean g_metrics_instance_counter_instance_is_interesting (GMetricsInstanceCounter *counter,
                                                             const char              *name);

GLIB_AVAILABLE_IN_ALL
void g_metrics_instance_counter_free (GMetricsInstanceCounter *counter);

typedef struct _GMetricsInstanceCounterIter GMetricsInstanceCounterIter;

typedef struct _GMetricsInstanceCounterEntry GMetricsInstanceCounterEntry;
struct _GMetricsInstanceCounterIter
{
  /*< private >*/
  gssize table_index;
  GMetricsTableIter table_iter;
};

GLIB_AVAILABLE_IN_ALL
void g_metrics_instance_counter_iter_init (GMetricsInstanceCounterIter *iter,
                                           GMetricsInstanceCounter     *table);
GLIB_AVAILABLE_IN_ALL
gboolean g_metrics_instance_counter_iter_next (GMetricsInstanceCounterIter  *iter,
                                               const char                **name,
                                               GMetricsInstanceCounterMetrics **metrics);

typedef struct _GMetricsList GMetricsList;
GLIB_AVAILABLE_IN_ALL
GMetricsList *g_metrics_list_new (void);
GLIB_AVAILABLE_IN_ALL
void g_metrics_list_add_item (GMetricsList *list,
                              gpointer      item);
GLIB_AVAILABLE_IN_ALL
void g_metrics_list_remove_item (GMetricsList *list,
                                 gpointer      item);
GLIB_AVAILABLE_IN_ALL
gsize g_metrics_list_get_length (GMetricsList *list);

GLIB_AVAILABLE_IN_ALL
gpointer g_metrics_list_get_last_item (GMetricsList *list);

GLIB_AVAILABLE_IN_ALL
void g_metrics_list_remove_last_item (GMetricsList *list);

GLIB_AVAILABLE_IN_ALL
void g_metrics_list_free (GMetricsList *list);

typedef struct _GMetricsListIter GMetricsListIter;
typedef struct _GMetricsListNode GMetricsListNode;
struct _GMetricsListIter
{
  /*< private >*/
  GMetricsListNode *node;
  GMetricsListNode *next_node;
};

GLIB_AVAILABLE_IN_ALL
void g_metrics_list_iter_init (GMetricsListIter *iter,
                               GMetricsList     *list);
GLIB_AVAILABLE_IN_ALL
gboolean g_metrics_list_iter_next_without_cast (GMetricsListIter *iter,
                                                gpointer         *item);
#define g_metrics_list_iter_next(_iter,_item) g_metrics_list_iter_next_without_cast ((_iter), (gpointer *) (_item))

typedef struct _GMetricsStackTrace GMetricsStackTrace;

GLIB_AVAILABLE_IN_ALL
GMetricsStackTrace *g_metrics_stack_trace_new (int start_frame,
                                               int number_of_frames,
                                               const char *delimiter);
GLIB_AVAILABLE_IN_ALL
const char *g_metrics_stack_trace_get_hash_key (GMetricsStackTrace *stack_trace);

GLIB_AVAILABLE_IN_ALL
const char *g_metrics_stack_trace_get_output (GMetricsStackTrace *stack_trace);

GLIB_AVAILABLE_IN_ALL
void g_metrics_stack_trace_add_annotation (GMetricsStackTrace *stack_trace,
                                           const char         *annotation);

GLIB_AVAILABLE_IN_ALL
void g_metrics_stack_trace_free (GMetricsStackTrace *stack_trace);

GLIB_AVAILABLE_IN_ALL
char *g_metrics_stack_trace (void);

typedef struct _GMetricsStackTraceSampler GMetricsStackTraceSampler;
typedef struct _GMetricsStackTraceSample GMetricsStackTraceSample;
typedef struct _GMetricsStackTraceSamplerIter GMetricsStackTraceSamplerIter;

struct _GMetricsStackTraceSamplerIter
{
  /*< private >*/
  GMetricsTableIter table_iter;
};

struct _GMetricsStackTraceSample
{
  char name[64];
  GMetricsStackTrace *stack_trace;
  gsize number_of_hits;
};

GLIB_AVAILABLE_IN_ALL
void g_metrics_stack_trace_sampler_iter_init (GMetricsStackTraceSamplerIter *iter,
                                              GMetricsStackTraceSampler     *sampler);
GLIB_AVAILABLE_IN_ALL
gboolean g_metrics_stack_trace_sampler_iter_next (GMetricsStackTraceSamplerIter   *iter,
                                                  GMetricsStackTraceSample       **sample);

typedef gboolean (* GMetricsStackTraceAnnotationHandler) (char *annotation, gsize annotation_size, gpointer user_data);

GLIB_AVAILABLE_IN_ALL
void g_metrics_set_stack_trace_annotation_handler (GMetricsStackTraceAnnotationHandler   handler,
                                                   gpointer                          user_data);
GLIB_AVAILABLE_IN_ALL
GMetricsStackTraceSampler *g_metrics_stack_trace_sampler_new (void);

GLIB_AVAILABLE_IN_ALL
void g_metrics_stack_trace_sampler_take_sample (GMetricsStackTraceSampler *sample,
                                                const char                *name,
                                                gpointer                   instance);
GLIB_AVAILABLE_IN_ALL
void g_metrics_stack_trace_sampler_remove_sample (GMetricsStackTraceSampler *sampler,
                                                  gpointer                   instance);
GLIB_AVAILABLE_IN_ALL
void g_metrics_stack_trace_sampler_clear (GMetricsStackTraceSampler *sampler);
GLIB_AVAILABLE_IN_ALL
void g_metrics_stack_trace_sampler_free (GMetricsStackTraceSampler *sampler);

typedef struct _GMetricsAllocationBlockStore GMetricsAllocationBlockStore;
GLIB_AVAILABLE_IN_ALL
GMetricsAllocationBlockStore *g_metrics_allocation_block_store_new (const char *name,
                                                                    gsize       size);
GLIB_AVAILABLE_IN_ALL
void g_metrics_allocation_block_store_free (GMetricsAllocationBlockStore *block_store);

GLIB_AVAILABLE_IN_ALL
gpointer g_metrics_allocation_block_store_allocate (GMetricsAllocationBlockStore *block_store,
                                                    gsize                        size);
GLIB_AVAILABLE_IN_ALL
gpointer g_metrics_allocation_block_store_allocate_with_name (GMetricsAllocationBlockStore *block_store,
                                                              gsize                         size,
                                                              const char                   *name);
GLIB_AVAILABLE_IN_ALL
gpointer g_metrics_allocation_block_store_reallocate (GMetricsAllocationBlockStore *block_store,
                                                      gpointer                     allocation,
                                                      gsize                        size);
GLIB_AVAILABLE_IN_ALL
gpointer g_metrics_allocation_block_store_copy (GMetricsAllocationBlockStore *block_store,
                                                gconstpointer                allocation,
                                                gsize                        size);
GLIB_AVAILABLE_IN_ALL
gpointer g_metrics_allocation_block_store_copy_with_name (GMetricsAllocationBlockStore *block_store,
                                                          gconstpointer                 allocation,
                                                          gsize                         size,
                                                          const char                   *name);
GLIB_AVAILABLE_IN_ALL
void g_metrics_allocation_block_store_deallocate (GMetricsAllocationBlockStore *block_store,
                                                  gpointer                     allocation);

GLIB_AVAILABLE_IN_ALL
void g_metrics_push_default_allocation_block_store (GMetricsAllocationBlockStore *block_store);
GLIB_AVAILABLE_IN_ALL
void g_metrics_pop_default_allocation_block_store (void);

GLIB_AVAILABLE_IN_ALL
gpointer g_metrics_allocate (gsize size);
GLIB_AVAILABLE_IN_ALL
gpointer g_metrics_reallocate (gpointer allocation,
                               gsize    size);
GLIB_AVAILABLE_IN_ALL
gpointer g_metrics_copy (gconstpointer allocation,
                         gsize         size);
GLIB_AVAILABLE_IN_ALL
void g_metrics_free (gpointer allocation);
G_END_DECLS

#endif /* __G_METRICS_H__ */
