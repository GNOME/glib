/*
 * Copyright © 2010 Codethink Limited
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
 * Authors: Ryan Lortie <desrt@desrt.ca>
 */

#include "gapplicationimpl.h"

#include "gapplication.h"
#include "gfile.h"
#include "gdbusconnection.h"
#include "gdbusintrospection.h"
#include "gdbuserror.h"

#include <string.h>


#include "gapplicationimpl-dbus-interface.c"

struct _GApplicationImpl
{
  GDBusConnection *session_bus;
  const gchar     *bus_name;
  gchar           *object_path;
  guint            object_id;
  gpointer         app;

  GMainLoop       *cmdline_mainloop;
};

static void
g_application_impl_method_call (GDBusConnection       *connection,
                                const gchar           *sender,
                                const gchar           *object_path,
                                const gchar           *interface_name,
                                const gchar           *method_name,
                                GVariant              *parameters,
                                GDBusMethodInvocation *invocation,
                                gpointer               user_data)
{
  GApplicationImpl *impl = user_data;
  GApplicationClass *class;

  class = G_APPLICATION_GET_CLASS (impl->app);

  if (strcmp (method_name, "Activate") == 0)
    {
      GVariant *platform_data;

      g_variant_get (parameters, "(@a{sv})", &platform_data);
      class->before_emit (impl->app, platform_data);
      g_signal_emit_by_name (impl->app, "activate");
      class->after_emit (impl->app, platform_data);
      g_variant_unref (platform_data);
    }

  else if (strcmp (method_name, "Open") == 0)
    {
      GVariant *platform_data;
      const gchar *hint;
      GVariant *array;
      GFile **files;
      gint n, i;

      g_variant_get (parameters, "(@ass@a{sv})",
                     &array, &hint, &platform_data);

      n = g_variant_n_children (array);
      files = g_new (GFile *, n + 1);

      for (i = 0; i < n; i++)
        {
          const gchar *uri;

          g_variant_get_child (array, i, "&s", &uri);
          files[i] = g_file_new_for_uri (uri);
        }
      g_variant_unref (array);
      files[n] = NULL;

      class->before_emit (impl->app, platform_data);
      g_signal_emit_by_name (impl->app, "open", files, n, hint);
      class->after_emit (impl->app, platform_data);

      g_variant_unref (platform_data);

      for (i = 0; i < n; i++)
        g_object_unref (files[i]);
      g_free (files);
    }

  else
    g_assert_not_reached ();
}

static gchar *
application_path_from_appid (const gchar *appid)
{
  gchar *appid_path, *iter;

  appid_path = g_strconcat ("/", appid, NULL);
  for (iter = appid_path; *iter; iter++)
    {
      if (*iter == '.')
        *iter = '/';
    }

  return appid_path;
}

void
g_application_impl_destroy (GApplicationImpl *impl)
{
  if (impl->session_bus)
    {
      if (impl->object_id)
        g_dbus_connection_unregister_object (impl->session_bus,
                                             impl->object_id);

      g_object_unref (impl->session_bus);
      g_free (impl->object_path);
    }
  else
    {
      g_assert (impl->object_path == NULL);
      g_assert (impl->object_id == 0);
    }

  g_slice_free (GApplicationImpl, impl);
}

GApplicationImpl *
g_application_impl_register (GApplication       *application,
                             const gchar        *appid,
                             GApplicationFlags   flags,
                             gboolean           *is_remote,
                             GCancellable       *cancellable,
                             GError            **error)
{
  const static GDBusInterfaceVTable vtable = {
    g_application_impl_method_call
  };
  GApplicationImpl *impl;
  GVariant *reply;
  guint32 rval;

  impl = g_slice_new (GApplicationImpl);

  impl->app = application;
  impl->bus_name = appid;

  impl->session_bus = g_bus_get_sync (G_BUS_TYPE_SESSION,
                                      cancellable, error);

  if (impl->session_bus == NULL)
    {
      g_slice_free (GApplicationImpl, impl);
      return NULL;
    }

  impl->object_path = application_path_from_appid (appid);

  if (flags & G_APPLICATION_FLAGS_IS_LAUNCHER)
    {
      impl->object_id = 0;
      *is_remote = TRUE;

      return impl;
    }

  impl->object_id = g_dbus_connection_register_object (impl->session_bus,
                                                       impl->object_path,
                                                       (GDBusInterfaceInfo *)
                                                         &org_gtk_Application,
                                                       &vtable,
                                                       impl, NULL,
                                                       error);

  if (impl->object_id == 0)
    {
      g_object_unref (impl->session_bus);
      g_free (impl->object_path);
      impl->session_bus = NULL;
      impl->object_path = NULL;

      g_slice_free (GApplicationImpl, impl);
      return NULL;
    }

  reply = g_dbus_connection_call_sync (impl->session_bus,
                                       "org.freedesktop.DBus",
                                       "/org/freedesktop/DBus",
                                       "org.freedesktop.DBus",
                                       "RequestName",
                                       g_variant_new ("(su)",
                                       /* DBUS_NAME_FLAG_DO_NOT_QUEUE: 0x4 */
                                                      impl->bus_name, 0x4),
                                       G_VARIANT_TYPE ("(u)"),
                                       0, -1, cancellable, error);

  if (reply == NULL)
    {
      g_dbus_connection_unregister_object (impl->session_bus,
                                           impl->object_id);
      impl->object_id = 0;

      g_object_unref (impl->session_bus);
      g_free (impl->object_path);
      impl->session_bus = NULL;
      impl->object_path = NULL;

      g_slice_free (GApplicationImpl, impl);
      return NULL;
    }

  g_variant_get (reply, "(u)", &rval);
  g_variant_unref (reply);

  /* DBUS_REQUEST_NAME_REPLY_EXISTS: 3 */
  if ((*is_remote = (rval == 3)))
    {
      g_dbus_connection_unregister_object (impl->session_bus,
                                           impl->object_id);
      impl->object_id = 0;

      if (flags & G_APPLICATION_FLAGS_IS_SERVICE)
        {
          g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                       "Unable to acquire bus name `%s'", appid);
          g_object_unref (impl->session_bus);
          g_free (impl->object_path);

          g_slice_free (GApplicationImpl, impl);
          impl = NULL;
        }
    }

  return impl;
}

void
g_application_impl_activate (GApplicationImpl *impl,
                             GVariant         *platform_data)
{
  g_dbus_connection_call (impl->session_bus,
                          impl->bus_name,
                          impl->object_path,
                          "org.gtk.Application",
                          "Activate",
                          g_variant_new ("(@a{sv})", platform_data),
                          NULL, 0, -1, NULL, NULL, NULL);
}

void
g_application_impl_open (GApplicationImpl  *impl,
                         GFile            **files,
                         gint               n_files,
                         const gchar       *hint,
                         GVariant          *platform_data)
{
  GVariantBuilder builder;
  gint i;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("(assa{sv})"));
  g_variant_builder_open (&builder, G_VARIANT_TYPE_STRING_ARRAY);
  for (i = 0; i < n_files; i++)
    {
      gchar *uri = g_file_get_uri (files[i]);
      g_variant_builder_add (&builder, "s", uri);
      g_free (uri);
    }
  g_variant_builder_close (&builder);
  g_variant_builder_add (&builder, "s", hint);
  g_variant_builder_add_value (&builder, platform_data);

  g_dbus_connection_call (impl->session_bus,
                          impl->bus_name,
                          impl->object_path,
                          "org.gtk.Application",
                          "Open",
                          g_variant_builder_end (&builder),
                          NULL, 0, -1, NULL, NULL, NULL);
}

void
g_application_impl_flush (GApplicationImpl *impl)
{
  g_dbus_connection_flush_sync (impl->session_bus, NULL, NULL);
}
