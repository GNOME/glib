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

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "gsimpleasyncresult.h"
#include "gioscheduler.h"
#include <gio/gioerror.h>
#include "glibintl.h"

static void g_simple_async_result_async_result_iface_init (GAsyncResultIface       *iface);

struct _GSimpleAsyncResult
{
  GObject parent_instance;

  GObject *source_object;
  GAsyncReadyCallback callback;
  gpointer user_data;
  GError *error;
  gboolean failed;
  gboolean handle_cancellation;

  gpointer source_tag;

  union {
    gpointer v_pointer;
    gboolean v_boolean;
    gssize   v_ssize;
  } op_res;

  GDestroyNotify destroy_op_res;
};

struct _GSimpleAsyncResultClass
{
  GObjectClass parent_class;
};


G_DEFINE_TYPE_WITH_CODE (GSimpleAsyncResult, g_simple_async_result, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_RESULT,
						g_simple_async_result_async_result_iface_init))

static void
g_simple_async_result_finalize (GObject *object)
{
  GSimpleAsyncResult *simple;

  simple = G_SIMPLE_ASYNC_RESULT (object);

  if (simple->source_object)
    g_object_unref (simple->source_object);

  if (simple->destroy_op_res)
    simple->destroy_op_res (simple->op_res.v_pointer);

  if (simple->error)
    g_error_free (simple->error);
  
  if (G_OBJECT_CLASS (g_simple_async_result_parent_class)->finalize)
    (*G_OBJECT_CLASS (g_simple_async_result_parent_class)->finalize) (object);
}

static void
g_simple_async_result_class_init (GSimpleAsyncResultClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  
  gobject_class->finalize = g_simple_async_result_finalize;
}

static void
g_simple_async_result_init (GSimpleAsyncResult *simple)
{
  simple->handle_cancellation = TRUE;
}

/**
 * g_simple_async_result_new:
 * @source_object:
 * @callback:
 * @user_data:
 * @source_tag:
 * 
 * Returns: #GSimpleAsyncResult
 **/
GSimpleAsyncResult *
g_simple_async_result_new (GObject *source_object,
			   GAsyncReadyCallback callback,
			   gpointer user_data,
			   gpointer source_tag)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (G_IS_OBJECT (source_object), NULL);

  simple = g_object_new (G_TYPE_SIMPLE_ASYNC_RESULT, NULL);
  simple->callback = callback;
  simple->source_object = g_object_ref (source_object);
  simple->user_data = user_data;
  simple->source_tag = source_tag;
  
  return simple;
}

/**
 * g_simple_async_result_new_from_error:
 * @source_object:
 * @callback:
 * @user_data:
 * @error: a #GError location to store the error occuring, or %NULL to 
 * ignore.
 * Returns: #GSimpleAsyncResult
 **/
GSimpleAsyncResult *
g_simple_async_result_new_from_error (GObject *source_object,
				      GAsyncReadyCallback callback,
				      gpointer user_data,
				      GError *error)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (G_IS_OBJECT (source_object), NULL);

  simple = g_simple_async_result_new (source_object,
				      callback,
				      user_data, NULL);
  g_simple_async_result_set_from_error (simple, error);

  return simple;
}

/**
 * g_simple_async_result_new_error:
 * @source_object:
 * @callback:
 * @user_data:
 * @domain:
 * @code:
 * @format:
 * @...
 * 
 * Returns: #GSimpleAsyncResult.
 **/
GSimpleAsyncResult *
g_simple_async_result_new_error (GObject *source_object,
				 GAsyncReadyCallback callback,
				 gpointer user_data,
				 GQuark         domain,
				 gint           code,
				 const char    *format,
				 ...)
{
  GSimpleAsyncResult *simple;
  va_list args;
  
  g_return_val_if_fail (G_IS_OBJECT (source_object), NULL);
  g_return_val_if_fail (domain != 0, NULL);
  g_return_val_if_fail (format != NULL, NULL);

  simple = g_simple_async_result_new (source_object,
				      callback,
				      user_data, NULL);

  va_start (args, format);
  g_simple_async_result_set_error_va (simple, domain, code, format, args);
  va_end (args);
  
  return simple;
}


static gpointer
g_simple_async_result_get_user_data (GAsyncResult *res)
{
  return G_SIMPLE_ASYNC_RESULT (res)->user_data;
}

static GObject *
g_simple_async_result_get_source_object (GAsyncResult *res)
{
  if (G_SIMPLE_ASYNC_RESULT (res)->source_object)
    return g_object_ref (G_SIMPLE_ASYNC_RESULT (res)->source_object);
  return NULL;
}

static void
g_simple_async_result_async_result_iface_init (GAsyncResultIface *iface)
{
  iface->get_user_data = g_simple_async_result_get_user_data;
  iface->get_source_object = g_simple_async_result_get_source_object;
}

/**
 * g_simple_async_result_set_handle_cancellation:
 * @simple:
 * @handle_cancellation:
 * 
 **/
void
g_simple_async_result_set_handle_cancellation (GSimpleAsyncResult *simple,
					       gboolean handle_cancellation)
{
  g_return_if_fail (G_IS_SIMPLE_ASYNC_RESULT (simple));
  simple->handle_cancellation = handle_cancellation;
}

/**
 * g_simple_async_result_get_source_tag:
 * @simple:
 * 
 * Returns: 
 **/
gpointer
g_simple_async_result_get_source_tag (GSimpleAsyncResult *simple)
{
  g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (simple), NULL);
  return simple->source_tag;
}

/**
 * g_simple_async_result_result_propagate_error:
 * @simple:
 * @dest:
 * 
 * Returns: 
 **/
gboolean
g_simple_async_result_propagate_error (GSimpleAsyncResult *simple,
				       GError **dest)
{
  g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (simple), FALSE);

  if (simple->failed)
    {
      g_propagate_error (dest, simple->error);
      simple->error = NULL;
      return TRUE;
    }
  return FALSE;
}

/**
 * g_simple_async_result_set_op_res_gpointer:
 * @simple:
 * @op_res:
 * @destroy_op_res:
 * 
 **/
void
g_simple_async_result_set_op_res_gpointer (GSimpleAsyncResult      *simple,
                                           gpointer                 op_res,
                                           GDestroyNotify           destroy_op_res)
{
  g_return_if_fail (G_IS_SIMPLE_ASYNC_RESULT (simple));

  simple->op_res.v_pointer = op_res;
  simple->destroy_op_res = destroy_op_res;
}

/**
 * g_simple_async_result_get_op_res_gpointer:
 * @simple:
 * 
 * Returns: gpointer.
 **/
gpointer
g_simple_async_result_get_op_res_gpointer (GSimpleAsyncResult      *simple)
{
  g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (simple), NULL);
  return simple->op_res.v_pointer;
}

/**
 * g_simple_async_result_set_op_res_gssize:
 * @simple:
 * @op_res:
 * 
 **/
void
g_simple_async_result_set_op_res_gssize   (GSimpleAsyncResult      *simple,
                                           gssize                   op_res)
{
  g_return_if_fail (G_IS_SIMPLE_ASYNC_RESULT (simple));
  simple->op_res.v_ssize = op_res;
}

/**
 * g_simple_async_result_get_op_res_gssize:
 * @simple:
 * 
 * Returns:
 **/
gssize
g_simple_async_result_get_op_res_gssize   (GSimpleAsyncResult      *simple)
{
  g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (simple), 0);
  return simple->op_res.v_ssize;
}

/**
 * g_simple_async_result_set_op_res_gboolean:
 * @simple:
 * @op_res:
 *  
 **/
void
g_simple_async_result_set_op_res_gboolean (GSimpleAsyncResult      *simple,
                                           gboolean                 op_res)
{
  g_return_if_fail (G_IS_SIMPLE_ASYNC_RESULT (simple));
  simple->op_res.v_boolean = !!op_res;
}

/**
 * g_simple_async_result_get_op_res_gboolean:
 * @simple:
 * 
 * Returns a #gboolean. 
 **/
gboolean
g_simple_async_result_get_op_res_gboolean (GSimpleAsyncResult      *simple)
{
  g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (simple), FALSE);
  return simple->op_res.v_boolean;
}

/**
 * g_simple_async_result_set_from_error:
 * @simple:
 * @error: #GError.
 * 
 * Sets the result from given @error.
 * 
 **/
void
g_simple_async_result_set_from_error (GSimpleAsyncResult *simple,
				      GError *error)
{
  g_return_if_fail (G_IS_SIMPLE_ASYNC_RESULT (simple));
  g_return_if_fail (error != NULL);

  simple->error = g_error_copy (error);
  simple->failed = TRUE;
}

static GError* 
_g_error_new_valist (GQuark         domain,
                    gint           code,
                    const char    *format,
                    va_list        args)
{
  GError *error;
  char *message;

  message = g_strdup_vprintf (format, args);

  error = g_error_new_literal (domain, code, message);
  g_free (message);
  
  return error;
}

/**
 * g_simple_async_result_set_error_va:
 * @simple:
 * @domain:
 * @code:
 * @format:
 * @args: va_list of arguments. 
 * 
 * Sets error va_list, suitable for language bindings.
 * 
 **/
void
g_simple_async_result_set_error_va (GSimpleAsyncResult *simple,
				    GQuark         domain,
				    gint           code,
				    const char    *format,
				    va_list        args)
{
  g_return_if_fail (G_IS_SIMPLE_ASYNC_RESULT (simple));
  g_return_if_fail (domain != 0);
  g_return_if_fail (format != NULL);

  simple->error = _g_error_new_valist (domain, code, format, args);
  simple->failed = TRUE;
}

/**
 * g_simple_async_result_set_error:
 * @simple:
 * @domain:
 * @code:
 * @format:
 * @...
 * 
 **/
void
g_simple_async_result_set_error (GSimpleAsyncResult *simple,
				 GQuark         domain,
				 gint           code,
				 const char    *format,
				 ...)
{
  va_list args;

  g_return_if_fail (G_IS_SIMPLE_ASYNC_RESULT (simple));
  g_return_if_fail (domain != 0);
  g_return_if_fail (format != NULL);

  va_start (args, format);
  g_simple_async_result_set_error_va (simple, domain, code, format, args);
  va_end (args);
}

/**
 * g_simple_async_result_complete:
 * @simple:
 * 
 **/
void
g_simple_async_result_complete (GSimpleAsyncResult *simple)
{
  g_return_if_fail (G_IS_SIMPLE_ASYNC_RESULT (simple));

  if (simple->callback)
    simple->callback (simple->source_object,
		      G_ASYNC_RESULT (simple),
		      simple->user_data);
}

static gboolean
complete_in_idle_cb (gpointer data)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (data);

  g_simple_async_result_complete (simple);

  return FALSE;
}

/**
 * g_simple_async_result_complete_in_idle:
 * @simple:
 *  
 **/
void
g_simple_async_result_complete_in_idle (GSimpleAsyncResult *simple)
{
  GSource *source;
  guint id;
  
  g_return_if_fail (G_IS_SIMPLE_ASYNC_RESULT (simple));
  
  g_object_ref (simple);
  
  source = g_idle_source_new ();
  g_source_set_priority (source, G_PRIORITY_DEFAULT);
  g_source_set_callback (source, complete_in_idle_cb, simple, g_object_unref);

  id = g_source_attach (source, NULL);
  g_source_unref (source);
}

typedef struct {
  GSimpleAsyncResult *simple;
  GSimpleAsyncThreadFunc func;
} RunInThreadData;

static void
run_in_thread (GIOJob *job,
	       GCancellable *c,
	       gpointer _data)
{
  RunInThreadData *data = _data;
  GSimpleAsyncResult *simple = data->simple;

  if (simple->handle_cancellation &&
      g_cancellable_is_cancelled (c))
    {
       g_simple_async_result_set_error (simple,
					G_IO_ERROR,
					G_IO_ERROR_CANCELLED,
                                       _("Operation was cancelled"));
    }
  else
    {
      data->func (simple,
		  simple->source_object,
		  c);
    }

  g_simple_async_result_complete_in_idle (data->simple);
  g_object_unref (data->simple);
  g_free (data);
}

/**
 * g_simple_async_result_run_in_thread:
 * @simple:
 * @func:
 * @io_priority: the io priority of the request.
 * @cancellable: optional #GCancellable object, %NULL to ignore. 
 **/
void
g_simple_async_result_run_in_thread (GSimpleAsyncResult *simple,
				     GSimpleAsyncThreadFunc func,
				     int io_priority, 
				     GCancellable *cancellable)
{
  RunInThreadData *data;
  
  g_return_if_fail (G_IS_SIMPLE_ASYNC_RESULT (simple));
  g_return_if_fail (func != NULL);

  data = g_new (RunInThreadData, 1);
  data->func = func;
  data->simple = g_object_ref (simple);
  g_schedule_io_job (run_in_thread, data, NULL, io_priority, cancellable);
}

/**
 * g_simple_async_report_error_in_idle:
 * @object:
 * @callback:
 * @user_data:
 * @domain:
 * @code:
 * @format:
 * @...
 * 
 **/
void
g_simple_async_report_error_in_idle (GObject *object,
				     GAsyncReadyCallback callback,
				     gpointer user_data,
				     GQuark         domain,
				     gint           code,
				     const char    *format,
				     ...)
{
  GSimpleAsyncResult *simple;
  va_list args;
  
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (domain != 0);
  g_return_if_fail (format != NULL);

  simple = g_simple_async_result_new (object,
				      callback,
				      user_data, NULL);

  va_start (args, format);
  g_simple_async_result_set_error_va (simple, domain, code, format, args);
  va_end (args);
  g_simple_async_result_complete_in_idle (simple);
  g_object_unref (simple);
}
