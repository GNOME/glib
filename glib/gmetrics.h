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
gboolean g_metrics_enabled (void);

GLIB_AVAILABLE_IN_ALL
gboolean g_metrics_requested (const char *name);

typedef void (*GMetricsTimeoutFunc) (void);

GLIB_AVAILABLE_IN_ALL
int g_metrics_get_timeout_fd (void);

GLIB_AVAILABLE_IN_ALL
void g_metrics_run_timeout_handlers (void);

GLIB_AVAILABLE_IN_ALL
void g_metrics_start_timeout (GMetricsTimeoutFunc  timeout_handler);

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
void g_metrics_list_free (GMetricsList *list);

typedef struct _GMetricsListIter GMetricsListIter;
typedef struct _GMetricsListNode GMetricsListNode;
struct _GMetricsListIter
{
  /*< private >*/
  GMetricsListNode *node;
};

GLIB_AVAILABLE_IN_ALL
void g_metrics_list_iter_init (GMetricsListIter *iter,
                               GMetricsList     *list);
GLIB_AVAILABLE_IN_ALL
gboolean g_metrics_list_iter_next_without_cast (GMetricsListIter *iter,
                                                gpointer         *item);
#define g_metrics_list_iter_next(_iter,_item) g_metrics_list_iter_next_without_cast ((_iter), (gpointer *) (_item))

GLIB_AVAILABLE_IN_ALL
char *g_metrics_stack_trace (void);

GLIB_AVAILABLE_IN_ALL
gpointer g_metrics_allocate (gsize size);

GLIB_AVAILABLE_IN_ALL
gpointer g_metrics_reallocate (gpointer allocation, gsize size);

GLIB_AVAILABLE_IN_ALL
gpointer g_metrics_copy (gconstpointer allocation,
                         gsize         size);
GLIB_AVAILABLE_IN_ALL
void g_metrics_free (gpointer allocation);

G_END_DECLS

#endif /* __G_METRICS_H__ */
