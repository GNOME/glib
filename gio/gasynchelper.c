/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include "config.h"

#include "gasynchelper.h"


/*< private >
 * SECTION:gasynchelper
 * @short_description: Asynchronous Helper Functions
 * @include: gio/gio.h
 * @see_also: #GAsyncResult
 * 
 * Provides helper functions for asynchronous operations.
 *
 **/

/*************************************************************************
 *             fd source                                                 *
 ************************************************************************/

typedef struct 
{
  GSource source;
  GPollFD pollfd;
} FDSource;

static gboolean 
fd_source_prepare (GSource *source,
		   gint    *timeout)
{
  *timeout = -1;
  return FALSE;
}

static gboolean 
fd_source_check (GSource *source)
{
  FDSource *fd_source = (FDSource *)source;

  return fd_source->pollfd.revents != 0;
}

static gboolean
fd_source_dispatch (GSource     *source,
		    GSourceFunc  callback,
		    gpointer     user_data)

{
  GFDSourceFunc func = (GFDSourceFunc)callback;
  FDSource *fd_source = (FDSource *)source;

  g_warn_if_fail (func != NULL);

  return (*func) (fd_source->pollfd.fd, fd_source->pollfd.revents, user_data);
}

static void 
fd_source_finalize (GSource *source)
{
}

static gboolean
fd_source_closure_callback (int           fd,
			    GIOCondition  condition,
			    gpointer      data)
{
  GClosure *closure = data;

  GValue params[2] = { G_VALUE_INIT, G_VALUE_INIT };
  GValue result_value = G_VALUE_INIT;
  gboolean result;

  g_value_init (&result_value, G_TYPE_BOOLEAN);

  g_value_init (&params[0], G_TYPE_INT);
  g_value_set_int (&params[0], fd);

  g_value_init (&params[1], G_TYPE_IO_CONDITION);
  g_value_set_flags (&params[1], condition);

  g_closure_invoke (closure, &result_value, 2, params, NULL);

  result = g_value_get_boolean (&result_value);
  g_value_unset (&result_value);
  g_value_unset (&params[0]);
  g_value_unset (&params[1]);

  return result;
}

static void
fd_source_closure_marshal (GClosure     *closure,
			   GValue       *return_value,
			   guint         n_param_values,
			   const GValue *param_values,
			   gpointer      invocation_hint,
			   gpointer      marshal_data)
{
  GFDSourceFunc callback;
  GCClosure *cc = (GCClosure*) closure;
  gboolean v_return;

  g_return_if_fail (return_value != NULL);
  g_return_if_fail (n_param_values == 0);

  callback = (GFDSourceFunc) (marshal_data ? marshal_data : cc->callback);

  v_return = callback (g_value_get_int (param_values),
                       g_value_get_flags (param_values + 1),
		       closure->data);

  g_value_set_boolean (return_value, v_return);
}

static GSourceFuncs fd_source_funcs = {
  fd_source_prepare,
  fd_source_check,
  fd_source_dispatch,
  fd_source_finalize,
  (GSourceFunc)fd_source_closure_callback,
  (GSourceDummyMarshal)fd_source_closure_marshal,
};

GSource *
_g_fd_source_new (int           fd,
		  gushort       events,
		  GCancellable *cancellable)
{
  GSource *source;
  FDSource *fd_source;

  source = g_source_new (&fd_source_funcs, sizeof (FDSource));
  fd_source = (FDSource *)source;

  fd_source->pollfd.fd = fd;
  fd_source->pollfd.events = events;
  g_source_add_poll (source, &fd_source->pollfd);

  if (cancellable)
    {
      GSource *cancellable_source = g_cancellable_source_new (cancellable);

      g_source_set_dummy_callback (cancellable_source);
      g_source_add_child_source (source, cancellable_source);
      g_source_unref (cancellable_source);
    }

  return source;
}
