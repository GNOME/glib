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

gpointer
g_context_specific_group_get (GContextSpecificGroup *group,
                              GType                  type,
                              goffset                context_offset,
                              GSourceFunc            start_func)
{
  GContextSpecificSource *css;
  GMainContext *context;

  context = g_main_context_get_thread_default ();
  if (!context)
    context = g_main_context_default ();

  if (!g_main_context_ensure_is_owner (context))
    g_critical ("Object %s is being set up in the wrong thread (or you forgot "
                "to set the thread-default main context).", g_type_name (type));

  g_mutex_lock (&group->lock);

  if (!group->table)
    group->table = g_hash_table_new (NULL, NULL);

  /* already started */
  if (start_func && g_hash_table_size (group->table) > 0)
    start_func = 0;

  css = g_hash_table_lookup (group->table, context);

  g_mutex_unlock (&group->lock);

  if (!css)
    {
      gpointer instance;

      instance = g_object_new (type, NULL);
      css = g_context_specific_source_new (g_type_name (type), instance);
      G_STRUCT_MEMBER (GMainContext *, instance, context_offset) = context;
      g_source_attach ((GSource *) css, context);

      g_mutex_lock (&group->lock);
      /* We know that nobody else could have added an item for 'context'
       * since we are the owner and there can only be one owner.
       */
      g_hash_table_insert (group->table, context, css);
      g_mutex_unlock (&group->lock);
    }

  if (start_func)
    g_main_context_invoke (GLIB_PRIVATE_CALL(g_get_worker_context) (), start_func, NULL);

  return css->instance;
}

void
g_context_specific_group_remove (GContextSpecificGroup *group,
                                 GMainContext          *context,
                                 gpointer               instance,
                                 GSourceFunc            stop_func)
{
  GContextSpecificSource *css;

  if (!context)
    {
      g_critical ("Removing %s with NULL context.  This object was probably directly constructed from a "
                  "dynamic language.  This is not a valid use of the API.", G_OBJECT_TYPE_NAME (instance));
      return;
    }

  if (!g_main_context_ensure_is_owner (context))
    g_critical ("Object %s is being torn down in the wrong thread.", G_OBJECT_TYPE_NAME (instance));

  g_mutex_lock (&group->lock);
  css = g_hash_table_lookup (group->table, context);
  g_hash_table_remove (group->table, context);
  g_assert (css);

  /* don't stop if there are others */
  if (stop_func && g_hash_table_size (group->table) > 0)
    stop_func = NULL;

  g_mutex_unlock (&group->lock);

  g_assert (css->instance == instance);

  g_source_unref ((GSource *) css);

  if (stop_func)
    g_main_context_invoke (GLIB_PRIVATE_CALL(g_get_worker_context) (), stop_func, NULL);
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
