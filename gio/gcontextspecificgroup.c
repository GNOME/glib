/*
 * Copyright © 2015 Canonical Limited
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gcontextspecificgroup.h"

#include <glib-object.h>
#include "glib-private.h"

typedef struct
{
  GSource   source;

  GMutex    lock;
  gpointer  instance;
  GQueue    pending;
} GContextSpecificSource;

static gboolean
g_context_specific_source_dispatch (GSource     *source,
                                    GSourceFunc  callback,
                                    gpointer     user_data)
{
  GContextSpecificSource *css = (GContextSpecificSource *) source;
  guint signal_id;

  g_mutex_lock (&css->lock);

  g_assert (!g_queue_is_empty (&css->pending));
  signal_id = GPOINTER_TO_UINT (g_queue_pop_head (&css->pending));

  if (g_queue_is_empty (&css->pending))
    g_source_set_ready_time (source, -1);

  g_mutex_unlock (&css->lock);

  g_signal_emit (css->instance, signal_id, 0);

  return TRUE;
}

static void
g_context_specific_source_finalize (GSource *source)
{
  GContextSpecificSource *css = (GContextSpecificSource *) source;

  g_mutex_clear (&css->lock);
  g_queue_clear (&css->pending);
}

static GContextSpecificSource *
g_context_specific_source_new (const gchar *name,
                               gpointer     instance)
{
  static GSourceFuncs source_funcs = {
    NULL,
    NULL,
    g_context_specific_source_dispatch,
    g_context_specific_source_finalize
  };
  GContextSpecificSource *css;
  GSource *source;

  source = g_source_new (&source_funcs, sizeof (GContextSpecificSource));
  css = (GContextSpecificSource *) source;

  g_source_set_name (source, name);

  g_mutex_init (&css->lock);
  g_queue_init (&css->pending);
  css->instance = instance;

  return css;
}

typedef struct
{
  GCallback callback;
  GMutex    mutex;
  GCond     cond;
} Closure;

static gboolean
g_context_specific_group_invoke_closure (gpointer user_data)
{
  Closure *closure = user_data;

  (* closure->callback) ();

  g_mutex_lock (&closure->mutex);

  closure->callback = NULL;

  g_cond_broadcast (&closure->cond);

  g_mutex_unlock (&closure->mutex);

  return FALSE;
}

/* this is not the most elegant way to deal with this, but it's probably
 * the best.  there are only two other things we could do, really:
 *
 *  - run the start function (but not the stop function) from the user's
 *    thread under some sort of lock.  we don't run the stop function
 *    from the user's thread to avoid the destroy-while-emitting problem
 *
 *  - have some check-and-compare functionality similar to what
 *    gsettings does where we send an artificial event in case we notice
 *    a change during the potential race period (using stat, for
 *    example)
 */
static void
g_context_specific_group_wait_for_callback (GCallback callback)
{
  Closure closure;

  g_mutex_init (&closure.mutex);
  g_cond_init (&closure.cond);

  closure.callback = callback;

  g_main_context_invoke (GLIB_PRIVATE_CALL(g_get_worker_context) (),
                         g_context_specific_group_invoke_closure, &closure);

  g_mutex_lock (&closure.mutex);
  while (closure.callback)
    g_cond_wait (&closure.cond, &closure.mutex);
  g_mutex_unlock (&closure.mutex);

  g_mutex_clear (&closure.mutex);
  g_cond_clear (&closure.cond);
}

gpointer
g_context_specific_group_get (GContextSpecificGroup *group,
                              GType                  type,
                              goffset                context_offset,
                              GCallback              start_func)
{
  GContextSpecificSource *css;
  GMainContext *context;

  context = g_main_context_get_thread_default ();
  if (!context)
    context = g_main_context_default ();

  g_mutex_lock (&group->lock);

  if (!group->table)
    group->table = g_hash_table_new (NULL, NULL);

  /* start only if there are no others */
  if (start_func && g_hash_table_size (group->table) == 0)
    g_context_specific_group_wait_for_callback (start_func);

  css = g_hash_table_lookup (group->table, context);

  if (!css)
    {
      gpointer instance;

      instance = g_object_new (type, NULL);
      css = g_context_specific_source_new (g_type_name (type), instance);
      G_STRUCT_MEMBER (GMainContext *, instance, context_offset) = g_main_context_ref (context);
      g_source_attach ((GSource *) css, context);

      g_hash_table_insert (group->table, context, css);
    }
  else
    g_object_ref (css->instance);

  g_mutex_unlock (&group->lock);

  return css->instance;
}

void
g_context_specific_group_remove (GContextSpecificGroup *group,
                                 GMainContext          *context,
                                 gpointer               instance,
                                 GCallback              stop_func)
{
  GContextSpecificSource *css;

  if (!context)
    {
      g_critical ("Removing %s with NULL context.  This object was probably directly constructed from a "
                  "dynamic language.  This is not a valid use of the API.", G_OBJECT_TYPE_NAME (instance));
      return;
    }

  g_mutex_lock (&group->lock);
  css = g_hash_table_lookup (group->table, context);
  g_hash_table_remove (group->table, context);
  g_assert (css);

  /* stop only if we were the last one */
  if (stop_func && g_hash_table_size (group->table) == 0)
    g_context_specific_group_wait_for_callback (stop_func);

  g_mutex_unlock (&group->lock);

  g_assert (css->instance == instance);

  g_source_unref ((GSource *) css);
  g_main_context_unref (context);
}

void
g_context_specific_group_emit (GContextSpecificGroup *group,
                               guint                  signal_id)
{
  g_mutex_lock (&group->lock);

  if (group->table)
    {
      GHashTableIter iter;
      gpointer value;
      gpointer ptr;

      ptr = GUINT_TO_POINTER (signal_id);

      g_hash_table_iter_init (&iter, group->table);
      while (g_hash_table_iter_next (&iter, NULL, &value))
        {
          GContextSpecificSource *css = value;

          g_mutex_lock (&css->lock);

          g_queue_remove (&css->pending, ptr);
          g_queue_push_tail (&css->pending, ptr);

          g_source_set_ready_time ((GSource *) css, 0);

          g_mutex_unlock (&css->lock);
        }
    }

  g_mutex_unlock (&group->lock);
}
