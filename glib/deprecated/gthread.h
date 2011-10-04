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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#if defined(G_DISABLE_SINGLE_INCLUDES) && !defined (__GLIB_H_INSIDE__) && !defined (GLIB_COMPILATION)
#error "Only <glib.h> can be included directly."
#endif

#ifndef __G_DEPRECATED_THREAD_H__
#define __G_DEPRECATED_THREAD_H__

#include <glib/gthread.h>

G_BEGIN_DECLS

typedef enum
{
  G_THREAD_PRIORITY_LOW,
  G_THREAD_PRIORITY_NORMAL,
  G_THREAD_PRIORITY_HIGH,
  G_THREAD_PRIORITY_URGENT
} GThreadPriority;

struct  _GThread
{
  /*< private >*/
  GThreadFunc func;
  gpointer data;
  gboolean joinable;
  GThreadPriority priority;
};

typedef struct _GThreadFunctions GThreadFunctions;
struct _GThreadFunctions
{
  GMutex*  (*mutex_new)           (void);
  void     (*mutex_lock)          (GMutex               *mutex);
  gboolean (*mutex_trylock)       (GMutex               *mutex);
  void     (*mutex_unlock)        (GMutex               *mutex);
  void     (*mutex_free)          (GMutex               *mutex);
  GCond*   (*cond_new)            (void);
  void     (*cond_signal)         (GCond                *cond);
  void     (*cond_broadcast)      (GCond                *cond);
  void     (*cond_wait)           (GCond                *cond,
                                   GMutex               *mutex);
  gboolean (*cond_timed_wait)     (GCond                *cond,
                                   GMutex               *mutex,
                                   GTimeVal             *end_time);
  void      (*cond_free)          (GCond                *cond);
  GPrivate* (*private_new)        (GDestroyNotify        destructor);
  gpointer  (*private_get)        (GPrivate             *private_key);
  void      (*private_set)        (GPrivate             *private_key,
                                   gpointer              data);
  void      (*thread_create)      (GThreadFunc           func,
                                   gpointer              data,
                                   gulong                stack_size,
                                   gboolean              joinable,
                                   gboolean              bound,
                                   GThreadPriority       priority,
                                   gpointer              thread,
                                   GError              **error);
  void      (*thread_yield)       (void);
  void      (*thread_join)        (gpointer              thread);
  void      (*thread_exit)        (void);
  void      (*thread_set_priority)(gpointer              thread,
                                   GThreadPriority       priority);
  void      (*thread_self)        (gpointer              thread);
  gboolean  (*thread_equal)       (gpointer              thread1,
                                   gpointer              thread2);
};

GLIB_VAR GThreadFunctions       g_thread_functions_for_glib_use;
GLIB_VAR gboolean               g_thread_use_default_impl;

GLIB_VAR guint64   (*g_thread_gettime) (void);

/* internal function for fallback static mutex implementation */
GMutex* g_static_mutex_get_mutex_impl   (GMutex **mutex);

GThread* g_thread_create       (GThreadFunc            func,
                                gpointer               data,
                                gboolean               joinable,
                                GError               **error);
GThread* g_thread_create_full  (GThreadFunc            func,
                                gpointer               data,
                                gulong                 stack_size,
                                gboolean               joinable,
                                gboolean               bound,
                                GThreadPriority        priority,
                                GError               **error);

void g_thread_set_priority     (GThread         *thread,
                                GThreadPriority  priority);

void     g_thread_foreach      (GFunc         thread_func,
                                gpointer      user_data);

#ifdef G_OS_WIN32
typedef GMutex * GStaticMutex;
#define G_STATIC_MUTEX_INIT NULL
#define g_static_mutex_get_mutex g_static_mutex_get_mutex_impl
#else /* G_OS_WIN32 */
typedef struct {
  GMutex *unused;
  GMutex mutex;
} GStaticMutex;
#define G_STATIC_MUTEX_INIT { NULL, { NULL } }
#define g_static_mutex_get_mutex(s) (&(s)->mutex)
#endif /* G_OS_WIN32 */

#define g_static_mutex_lock(mutex) \
    g_mutex_lock (g_static_mutex_get_mutex (mutex))
#define g_static_mutex_trylock(mutex) \
    g_mutex_trylock (g_static_mutex_get_mutex (mutex))
#define g_static_mutex_unlock(mutex) \
    g_mutex_unlock (g_static_mutex_get_mutex (mutex))
void g_static_mutex_init (GStaticMutex *mutex);
void g_static_mutex_free (GStaticMutex *mutex);

typedef struct _GStaticRecMutex GStaticRecMutex;
struct _GStaticRecMutex
{
  /*< private >*/
  GStaticMutex mutex;
  guint depth;
  GSystemThread owner;
};

#define G_STATIC_REC_MUTEX_INIT { G_STATIC_MUTEX_INIT, 0, {{0, 0, 0, 0}} }
void     g_static_rec_mutex_init        (GStaticRecMutex *mutex);
void     g_static_rec_mutex_lock        (GStaticRecMutex *mutex);
gboolean g_static_rec_mutex_trylock     (GStaticRecMutex *mutex);
void     g_static_rec_mutex_unlock      (GStaticRecMutex *mutex);
void     g_static_rec_mutex_lock_full   (GStaticRecMutex *mutex,
                                         guint            depth);
guint    g_static_rec_mutex_unlock_full (GStaticRecMutex *mutex);
void     g_static_rec_mutex_free        (GStaticRecMutex *mutex);

typedef struct _GStaticRWLock GStaticRWLock;
struct _GStaticRWLock
{
  /*< private >*/
  GStaticMutex mutex;
  GCond *read_cond;
  GCond *write_cond;
  guint read_counter;
  gboolean have_writer;
  guint want_to_read;
  guint want_to_write;
};

#define G_STATIC_RW_LOCK_INIT { G_STATIC_MUTEX_INIT, NULL, NULL, 0, FALSE, 0, 0 }

void      g_static_rw_lock_init           (GStaticRWLock* lock);
void      g_static_rw_lock_reader_lock    (GStaticRWLock* lock);
gboolean  g_static_rw_lock_reader_trylock (GStaticRWLock* lock);
void      g_static_rw_lock_reader_unlock  (GStaticRWLock* lock);
void      g_static_rw_lock_writer_lock    (GStaticRWLock* lock);
gboolean  g_static_rw_lock_writer_trylock (GStaticRWLock* lock);
void      g_static_rw_lock_writer_unlock  (GStaticRWLock* lock);
void      g_static_rw_lock_free           (GStaticRWLock* lock);

GPrivate *      g_private_new             (GDestroyNotify  notify);

struct _GStaticPrivate
{
  /*< private >*/
  guint index;
};

#define G_STATIC_PRIVATE_INIT { 0 }
void     g_static_private_init           (GStaticPrivate   *private_key);
gpointer g_static_private_get            (GStaticPrivate   *private_key);
void     g_static_private_set            (GStaticPrivate   *private_key,
                                          gpointer          data,
                                          GDestroyNotify    notify);
void     g_static_private_free           (GStaticPrivate   *private_key);

gboolean g_once_init_enter_impl          (volatile gsize   *location);

void     g_thread_init   (gpointer vtable);

gboolean g_thread_get_initialized (void);

GLIB_VAR gboolean g_threads_got_initialized;

#if defined(G_THREADS_MANDATORY)
#define g_thread_supported()     1
#else
#define g_thread_supported()    (g_threads_got_initialized)
#endif

GMutex* g_static_mutex_get_mutex_impl   (GMutex **mutex);

GMutex *                g_mutex_new                                     (void);
void                    g_mutex_free                                    (GMutex         *mutex);
GCond *                 g_cond_new                                      (void);
void                    g_cond_free                                     (GCond          *cond);

G_END_DECLS

#endif /* __G_DEPRECATED_THREAD_H__ */
