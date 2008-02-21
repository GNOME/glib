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

#include "gfileicon.h"
#include "gsimpleasyncresult.h"

#include "gioalias.h"

/**
 * SECTION:gfileicon
 * @short_description: Icons pointing to an image file
 * @include: gio/gio.h
 * @see_also: #GIcon, #GLoadableIcon
 * 
 * #GFileIcon specifies an icon by pointing to an image file
 * to be used as icon.
 * 
 **/

static void g_file_icon_icon_iface_init          (GIconIface          *iface);
static void g_file_icon_loadable_icon_iface_init (GLoadableIconIface  *iface);
static void g_file_icon_load_async               (GLoadableIcon       *icon,
						  int                  size,
						  GCancellable        *cancellable,
						  GAsyncReadyCallback  callback,
						  gpointer             user_data);

struct _GFileIcon
{
  GObject parent_instance;

  GFile *file;
};

struct _GFileIconClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE_WITH_CODE (GFileIcon, g_file_icon, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (G_TYPE_ICON,
						g_file_icon_icon_iface_init);
			 G_IMPLEMENT_INTERFACE (G_TYPE_LOADABLE_ICON,
						g_file_icon_loadable_icon_iface_init);
			 )
  
static void
g_file_icon_finalize (GObject *object)
{
  GFileIcon *icon;

  icon = G_FILE_ICON (object);

  g_object_unref (icon->file);
  
  if (G_OBJECT_CLASS (g_file_icon_parent_class)->finalize)
    (*G_OBJECT_CLASS (g_file_icon_parent_class)->finalize) (object);
}

static void
g_file_icon_class_init (GFileIconClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  
  gobject_class->finalize = g_file_icon_finalize;
}

static void
g_file_icon_init (GFileIcon *file)
{
}

/**
 * g_file_icon_new:
 * @file: a #GFile.
 * 
 * Creates a new icon for a file.
 * 
 * Returns: a #GIcon for the given @file, or %NULL on error.
 **/
GIcon *
g_file_icon_new (GFile *file)
{
  GFileIcon *icon;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  icon = g_object_new (G_TYPE_FILE_ICON, NULL);
  icon->file = g_object_ref (file);
  
  return G_ICON (icon);
}

/**
 * g_file_icon_get_file:
 * @icon: a #GIcon.
 * 
 * Gets the #GFile associated with the given @icon.
 * 
 * Returns: a #GFile, or %NULL.
 **/
GFile *
g_file_icon_get_file (GFileIcon *icon)
{
  g_return_val_if_fail (G_IS_FILE_ICON (icon), NULL);

  return icon->file;
}

static guint
g_file_icon_hash (GIcon *icon)
{
  GFileIcon *file_icon = G_FILE_ICON (icon);

  return g_file_hash (file_icon->file);
}

static gboolean
g_file_icon_equal (GIcon *icon1,
		   GIcon *icon2)
{
  GFileIcon *file1 = G_FILE_ICON (icon1);
  GFileIcon *file2 = G_FILE_ICON (icon2);
  
  return g_file_equal (file1->file, file2->file);
}


static void
g_file_icon_icon_iface_init (GIconIface *iface)
{
  iface->hash = g_file_icon_hash;
  iface->equal = g_file_icon_equal;
}


static GInputStream *
g_file_icon_load (GLoadableIcon  *icon,
		  int            size,
		  char          **type,
		  GCancellable   *cancellable,
		  GError        **error)
{
  GFileInputStream *stream;
  GFileIcon *file_icon = G_FILE_ICON (icon);

  stream = g_file_read (file_icon->file,
			cancellable,
			error);
  
  return G_INPUT_STREAM (stream);
}

typedef struct {
  GLoadableIcon *icon;
  GAsyncReadyCallback callback;
  gpointer user_data;
} LoadData;

static void
load_data_free (LoadData *data)
{
  g_object_unref (data->icon);
  g_free (data);
}

static void
load_async_callback (GObject      *source_object,
		     GAsyncResult *res,
		     gpointer      user_data)
{
  GFileInputStream *stream;
  GError *error = NULL;
  GSimpleAsyncResult *simple;
  LoadData *data = user_data;

  stream = g_file_read_finish (G_FILE (source_object), res, &error);
  
  if (stream == NULL)
    {
      simple = g_simple_async_result_new_from_error (G_OBJECT (data->icon),
						     data->callback,
						     data->user_data,
						     error);
      g_error_free (error);
    }
  else
    {
      simple = g_simple_async_result_new (G_OBJECT (data->icon),
					  data->callback,
					  data->user_data,
					  g_file_icon_load_async);
      
      g_simple_async_result_set_op_res_gpointer (simple,
						 stream,
						 g_object_unref);
  }


  g_simple_async_result_complete (simple);
  
  load_data_free (data);
}

static void
g_file_icon_load_async (GLoadableIcon       *icon,
                        int                  size,
                        GCancellable        *cancellable,
                        GAsyncReadyCallback  callback,
                        gpointer             user_data)
{
  GFileIcon *file_icon = G_FILE_ICON (icon);
  LoadData *data;

  data = g_new0 (LoadData, 1);
  data->icon = g_object_ref (icon);
  data->callback = callback;
  data->user_data = user_data;
  
  g_file_read_async (file_icon->file, 0,
		     cancellable,
		     load_async_callback, data);
  
}

static GInputStream *
g_file_icon_load_finish (GLoadableIcon  *icon,
			 GAsyncResult   *res,
			 char          **type,
			 GError        **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  gpointer op;

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_file_icon_load_async);

  if (type)
    *type = NULL;
  
  op = g_simple_async_result_get_op_res_gpointer (simple);
  if (op)
    return g_object_ref (op);
  
  return NULL;
}

static void
g_file_icon_loadable_icon_iface_init (GLoadableIconIface *iface)
{
  iface->load = g_file_icon_load;
  iface->load_async = g_file_icon_load_async;
  iface->load_finish = g_file_icon_load_finish;
}

#define __G_FILE_ICON_C__
#include "gioaliasdef.c"
