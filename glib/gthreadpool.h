/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */

#if !defined (__GLIB_H_INSIDE__) && !defined (GLIB_COMPILATION)
#error "Only <glib.h> can be included directly."
#endif

#ifndef __G_THREADPOOL_H__
#define __G_THREADPOOL_H__

#include <glib/gthread.h>

G_BEGIN_DECLS

typedef struct _GThreadPool GThreadPool;

/* Thread Pools
 */

struct _GThreadPool
{
  GFunc func;
  gpointer user_data;
  gboolean exclusive;
};

GThreadPool *   g_thread_pool_new               (GFunc            func,
                                                 gpointer         user_data,
                                                 gint             max_threads,
                                                 gboolean         exclusive,
                                                 GError         **error);
void            g_thread_pool_free              (GThreadPool     *pool,
                                                 gboolean         immediate,
                                                 gboolean         wait_);
gboolean        g_thread_pool_push              (GThreadPool     *pool,
                                                 gpointer         data,
                                                 GError         **error);
guint           g_thread_pool_unprocessed       (GThreadPool     *pool);
void            g_thread_pool_set_sort_function (GThreadPool      *pool,
                                                 GCompareDataFunc  func,
                                                 gpointer          user_data);
gboolean        g_thread_pool_set_max_threads   (GThreadPool     *pool,
                                                 gint             max_threads,
                                                 GError         **error);
gint            g_thread_pool_get_max_threads   (GThreadPool     *pool);
guint           g_thread_pool_get_num_threads   (GThreadPool     *pool);

void            g_thread_pool_set_max_unused_threads (gint  max_threads);
gint            g_thread_pool_get_max_unused_threads (void);
guint           g_thread_pool_get_num_unused_threads (void);
void            g_thread_pool_stop_unused_threads    (void);
void            g_thread_pool_set_max_idle_time      (guint interval);
guint           g_thread_pool_get_max_idle_time      (void);

G_END_DECLS

#endif /* __G_THREADPOOL_H__ */
