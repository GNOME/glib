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

#include "gactiongroup.h"
#include "gapplication.h"
#include "gfile.h"
#include "gdbusconnection.h"
#include "gdbusintrospection.h"
#include "gdbuserror.h"

#include <string.h>
#include <stdio.h>

#include "gapplicationcommandline.h"
#include "gdbusmethodinvocation.h"

/* DBus Interface definition {{{1 */

static GDBusInterfaceInfo *
get_interface (const gchar *name)
{
  static GDBusInterfaceInfo *org_gtk_Application;
  static GDBusInterfaceInfo *org_gtk_private_CommandLine;
  static GDBusInterfaceInfo *org_gtk_Actions;

  if (org_gtk_Application == NULL)
    {
      GError *error = NULL;
      GDBusNodeInfo *info;

      info = g_dbus_node_info_new_for_xml (
        "<node>"
        "  <interface name='org.gtk.Application'>"
        "    <method name='Activate'>"
        "      <arg type='a{sv}' name='platform_data' direction='in'/>"
        "    </method>"
        "    <method name='Open'>"
        "      <arg type='as' name='uris' direction='in'/>"
        "      <arg type='s' name='hint' direction='in'/>"
        "      <arg type='a{sv}' name='platform_data' direction='in'/>"
        "    </method>"
        "    <method name='CommandLine'>"
        "      <arg type='o' name='path' direction='in'/>"
        "      <arg type='aay' name='arguments' direction='in'/>"
        "      <arg type='a{sv}' name='platform_data' direction='in'/>"
        "      <arg type='i' name='exit_status' direction='out'/>"
        "    </method>"
        "  </interface>"
        "  <interface name='org.gtk.private.CommandLine'>"
        "    <method name='Print'>"
        "      <arg type='s' name='message' direction='in'/>"
        "    </method>"
        "    <method name='PrintError'>"
        "      <arg type='s' name='message' direction='in'/>"
        "    </method>"
        "  </interface>"
        "  <interface name='org.gtk.Actions'>"
        "    <method name='DescribeAll'>"
        "      <arg type='a(savbav)' name='list' direction='out'/>"
        "    </method>"
        "    <method name='SetState'>"
        "      <arg type='s' name='action_name' direction='in'/>"
        "      <arg type='v' name='value' direction='in'/>"
        "      <arg type='a{sv}' name='platform_data' direction='in'/>"
        "    </method>"
        "    <method name='Activate'>"
        "      <arg type='s' name='action_name' direction='in'/>"
        "      <arg type='av' name='parameter' direction='in'/>"
        "      <arg type='a{sv}' name='platform_data' direction='in'/>"
        "    </method>"
        "  </interface>"
        "</node>", &error);

      if (info == NULL)
        g_error ("%s\n", error->message);

      org_gtk_Application = g_dbus_node_info_lookup_interface (info, "org.gtk.Application");
      g_assert (org_gtk_Application != NULL);
      g_dbus_interface_info_ref (org_gtk_Application);

      org_gtk_private_CommandLine = g_dbus_node_info_lookup_interface (info, "org.gtk.private.CommandLine");
      g_assert (org_gtk_private_CommandLine != NULL);
      g_dbus_interface_info_ref (org_gtk_private_CommandLine);

      org_gtk_Actions = g_dbus_node_info_lookup_interface (info, "org.gtk.Actions");
      g_assert (org_gtk_Actions != NULL);
      g_dbus_interface_info_ref (org_gtk_Actions);

      g_dbus_node_info_unref (info);
    }

  if (strcmp (name, "org.gtk.Application") == 0)
    return org_gtk_Application;
  else if (strcmp (name, "org.gtk.private.CommandLine") == 0)
    return org_gtk_private_CommandLine;
  else
    return org_gtk_Actions;
}

/* GApplication implementation {{{1 */
struct _GApplicationImpl
{
  GDBusConnection *session_bus;
  const gchar     *bus_name;
  gchar           *object_path;
  guint            object_id;
  guint            action_id;
  gpointer         app;

  GHashTable      *actions;
  guint            signal_id;
};


static GApplicationCommandLine *
g_dbus_command_line_new (GDBusMethodInvocation *invocation);


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

      g_dbus_method_invocation_return_value (invocation, NULL);
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

      g_dbus_method_invocation_return_value (invocation, NULL);
    }

  else if (strcmp (method_name, "CommandLine") == 0)
    {
      GApplicationCommandLine *cmdline;
      GVariant *platform_data;
      int status;

      cmdline = g_dbus_command_line_new (invocation);
      platform_data = g_variant_get_child_value (parameters, 2);
      class->before_emit (impl->app, platform_data);
      g_signal_emit_by_name (impl->app, "command-line", cmdline, &status);
      g_application_command_line_set_exit_status (cmdline, status);
      class->after_emit (impl->app, platform_data);
      g_variant_unref (platform_data);
      g_object_unref (cmdline);
    }
  else
    g_assert_not_reached ();
}

static void
g_application_impl_actions_method_call (GDBusConnection       *connection,
                                        const gchar           *sender,
                                        const gchar           *object_path,
                                        const gchar           *interface_name,
                                        const gchar           *method_name,
                                        GVariant              *parameters,
                                        GDBusMethodInvocation *invocation,
                                        gpointer               user_data)
{
  GApplicationImpl *impl = user_data;
  GActionGroup *action_group;
  GApplicationClass *class;

  class = G_APPLICATION_GET_CLASS (impl->app);
  action_group = G_ACTION_GROUP (impl->app);

  if (strcmp (method_name, "DescribeAll") == 0)
    {
      GVariantBuilder builder;
      gchar **actions;
      gint i;

      actions = g_action_group_list_actions (action_group);
      g_variant_builder_init (&builder, G_VARIANT_TYPE ("(a(savbav))"));
      g_variant_builder_open (&builder, G_VARIANT_TYPE ("a(savbav)"));

      for (i = 0; actions[i]; i++)
        {
          /* Open */
          g_variant_builder_open (&builder, G_VARIANT_TYPE ("(savbav)"));

          /* Name */
          g_variant_builder_add (&builder, "s", actions[i]);

          /* Parameter type */
          g_variant_builder_open (&builder, G_VARIANT_TYPE ("av"));
            {
              const GVariantType *type;

              type = g_action_group_get_action_parameter_type (action_group,
                                                               actions[i]);
              if (type != NULL)
                {
                  GVariantType *array_type;

                  array_type = g_variant_type_new_array (type);
                  g_variant_builder_open (&builder, G_VARIANT_TYPE_VARIANT);
                  g_variant_builder_open (&builder, array_type);
                  g_variant_builder_close (&builder);
                  g_variant_builder_close (&builder);
                  g_variant_type_free (array_type);
                }
            }
          g_variant_builder_close (&builder);

          /* Enabled */
          {
            gboolean enabled = g_action_group_get_action_enabled (action_group,
                                                                  actions[i]);
            g_variant_builder_add (&builder, "b", enabled);
          }

          /* State */
          g_variant_builder_open (&builder, G_VARIANT_TYPE ("av"));
          {
            GVariant *state = g_action_group_get_action_state (action_group,
                                                               actions[i]);
            if (state != NULL)
              {
                g_variant_builder_add (&builder, "v", state);
                g_variant_unref (state);
              }
          }
          g_variant_builder_close (&builder);

          /* Close */
          g_variant_builder_close (&builder);
        }
      g_variant_builder_close (&builder);

      g_dbus_method_invocation_return_value (invocation,
                                             g_variant_builder_end (&builder));

      g_strfreev (actions);
    }

  else if (strcmp (method_name, "SetState") == 0)
    {
      const gchar *action_name;
      GVariant *platform_data;
      GVariant *state;

      g_variant_get (parameters, "(&sv@a{sv})",
                     &action_name, &state, &platform_data);

      class->before_emit (impl->app, platform_data);
      g_action_group_change_action_state (action_group, action_name, state);
      class->after_emit (impl->app, platform_data);
      g_variant_unref (platform_data);
      g_variant_unref (state);

      g_dbus_method_invocation_return_value (invocation, NULL);
    }

  else if (strcmp (method_name, "Activate") == 0)
    {
      const gchar *action_name;
      GVariant *platform_data;
      GVariantIter *param;
      GVariant *parameter;
      GVariant *unboxed_parameter;

      g_variant_get (parameters, "(&sav@a{sv})",
                     &action_name, &param, &platform_data);
      parameter = g_variant_iter_next_value (param);
      unboxed_parameter = parameter ? g_variant_get_variant (parameter) : NULL;
      g_variant_iter_free (param);

      class->before_emit (impl->app, platform_data);
      g_action_group_activate_action (action_group, action_name, unboxed_parameter);
      class->after_emit (impl->app, platform_data);
      g_variant_unref (platform_data);

      if (unboxed_parameter)
        g_variant_unref (unboxed_parameter);
      if (parameter)
        g_variant_unref (parameter);

      g_dbus_method_invocation_return_value (invocation, NULL);
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

      if (*iter == '-')
        *iter = '_';
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
      if (impl->action_id)
        g_dbus_connection_unregister_object (impl->session_bus,
                                             impl->action_id);

      g_dbus_connection_call (impl->session_bus,
                              "org.freedesktop.DBus",
                              "/org/freedesktop/DBus",
                              "org.freedesktop.DBus",
                              "ReleaseName",
                              g_variant_new ("(s)",
                                             impl->bus_name),
                              NULL,
                              G_DBUS_CALL_FLAGS_NONE,
                              -1, NULL, NULL, NULL);

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

static void
unwrap_fake_maybe (GVariant **value)
{
  GVariant *tmp;

  if (g_variant_n_children (*value))
    g_variant_get_child (*value, 0, "v", &tmp);
  else
    tmp = NULL;

  g_variant_unref (*value);
  *value = tmp;
}

static RemoteActionInfo *
remote_action_info_new_from_iter (GVariantIter *iter)
{
  RemoteActionInfo *info;
  GVariant *param_type;
  gboolean enabled;
  GVariant *state;
  gchar *name;

  if (!g_variant_iter_next (iter, "(s@avb@av)", &name,
                            &param_type, &enabled, &state))
    return NULL;

  unwrap_fake_maybe (&param_type);
  unwrap_fake_maybe (&state);

  info = g_slice_new (RemoteActionInfo);
  info->name = name;
  info->enabled = enabled;
  info->state = state;

  if (param_type != NULL)
    {
      info->parameter_type = g_variant_type_copy (
                               g_variant_type_element (
                                 g_variant_get_type (param_type)));
      g_variant_unref (param_type);
    }
  else
    info->parameter_type = NULL;

  return info;
}

static void
g_application_impl_action_signal (GDBusConnection *connection,
                                  const gchar     *sender_name,
                                  const gchar     *object_path,
                                  const gchar     *interface_name,
                                  const gchar     *signal_name,
                                  GVariant        *parameters,
                                  gpointer         user_data)
{
  GApplicationImpl *impl = user_data;
  GActionGroup *action_group;

  action_group = G_ACTION_GROUP (impl->app);

  if (strcmp (signal_name, "Added") == 0 &&
      g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(a(savbav))")))
    {
      RemoteActionInfo *info;
      GVariantIter *iter;

      g_variant_get_child (parameters, 0, "a(savbav)", &iter);

      while ((info = remote_action_info_new_from_iter (iter)))
        {
          g_hash_table_replace (impl->actions, info->name, info);
          g_action_group_action_added (action_group, info->name);
        }

      g_variant_iter_free (iter);
    }

  else if (strcmp (signal_name, "Removed") == 0 &&
           g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(as)")))
    {
      GVariantIter *iter;
      const gchar *name;

      g_variant_get_child (parameters, 0, "as", &iter);
      while (g_variant_iter_next (iter, "&s", &name))
        if (g_hash_table_remove (impl->actions, name))
          g_action_group_action_removed (action_group, name);
      g_variant_iter_free (iter);
    }

  else if (strcmp (signal_name, "EnabledChanged") == 0 &&
           g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(sb)")))
    {
      RemoteActionInfo *info;
      const gchar *name;
      gboolean enabled;

      g_variant_get (parameters, "(&sb)", &name, &enabled);
      info = g_hash_table_lookup (impl->actions, name);

      if (info && enabled != info->enabled)
        {
          info->enabled = enabled;
          g_action_group_action_enabled_changed (action_group,
                                                 info->name,
                                                 enabled);
        }
    }

  else if (strcmp (signal_name, "StateChanged") == 0 &&
           g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(sv)")))
    {
      RemoteActionInfo *info;
      const gchar *name;
      GVariant *state;

      g_variant_get (parameters, "(&sv)", &name, &state);
      info = g_hash_table_lookup (impl->actions, name);

      if (info && info->state &&
          g_variant_is_of_type (state, g_variant_get_type (info->state)) &&
          !g_variant_equal (state, info->state))
        {
          g_variant_unref (info->state);
          info->state = g_variant_ref (state);
          g_action_group_action_state_changed (action_group,
                                               info->name,
                                               state);
        }
      g_variant_unref (state);
    }
}

GApplicationImpl *
g_application_impl_register (GApplication       *application,
                             const gchar        *appid,
                             GApplicationFlags   flags,
                             GHashTable        **remote_actions,
                             GCancellable       *cancellable,
                             GError            **error)
{
  const static GDBusInterfaceVTable vtable = {
    g_application_impl_method_call
  };
  const static GDBusInterfaceVTable actions_vtable = {
    g_application_impl_actions_method_call
  };
  GApplicationImpl *impl;
  GVariant *reply;
  guint32 rval;

  impl = g_slice_new0 (GApplicationImpl);

  impl->app = application;
  impl->bus_name = appid;

  impl->session_bus = g_bus_get_sync (G_BUS_TYPE_SESSION, cancellable, NULL);

  if (impl->session_bus == NULL)
    {
      /* If we can't connect to the session bus, proceed as a normal
       * non-unique application.
       */
      *remote_actions = NULL;
      return impl;
    }

  impl->object_path = application_path_from_appid (appid);

  /* Only try to be the primary instance if
   * G_APPLICATION_IS_LAUNCHER was not specified.
   */
  if (~flags & G_APPLICATION_IS_LAUNCHER)
    {
      /* Attempt to become primary instance. */
      impl->object_id =
        g_dbus_connection_register_object (impl->session_bus,
                                           impl->object_path,
                                           get_interface ("org.gtk.Application"),
                                           &vtable, impl, NULL, error);

      if (impl->object_id == 0)
        {
          g_object_unref (impl->session_bus);
          g_free (impl->object_path);
          impl->session_bus = NULL;
          impl->object_path = NULL;

          g_slice_free (GApplicationImpl, impl);
          return NULL;
        }

      impl->action_id =
        g_dbus_connection_register_object (impl->session_bus,
                                           impl->object_path,
                                           get_interface ("org.gtk.Actions"),
                                           &actions_vtable,
                                           impl, NULL, error);

      if (impl->action_id == 0)
        {
          g_dbus_connection_unregister_object (impl->session_bus,
                                               impl->object_id);

          g_object_unref (impl->session_bus);
          g_free (impl->object_path);
          impl->session_bus = NULL;
          impl->object_path = NULL;

          g_slice_free (GApplicationImpl, impl);
          return NULL;
        }

      /* DBUS_NAME_FLAG_DO_NOT_QUEUE: 0x4 */
      reply = g_dbus_connection_call_sync (impl->session_bus,
                                           "org.freedesktop.DBus",
                                           "/org/freedesktop/DBus",
                                           "org.freedesktop.DBus",
                                           "RequestName",
                                           g_variant_new ("(su)",
                                                          impl->bus_name,
                                                          0x4),
                                           G_VARIANT_TYPE ("(u)"),
                                           0, -1, cancellable, error);

      if (reply == NULL)
        {
          g_dbus_connection_unregister_object (impl->session_bus,
                                               impl->object_id);
          impl->object_id = 0;
          g_dbus_connection_unregister_object (impl->session_bus,
                                               impl->action_id);
          impl->action_id = 0;

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
      if (rval != 3)
        {
          /* We are the primary instance. */
          g_dbus_connection_emit_signal (impl->session_bus,
                                         NULL,
                                         impl->object_path,
                                         "org.gtk.Application",
                                         "Hello",
                                         g_variant_new ("(s)",
                                                        impl->bus_name),
                                         NULL);
          *remote_actions = NULL;
          return impl;
        }

      /* We didn't make it.  Drop our service-side stuff. */
      g_dbus_connection_unregister_object (impl->session_bus,
                                           impl->object_id);
      impl->object_id = 0;
      g_dbus_connection_unregister_object (impl->session_bus,
                                           impl->action_id);
      impl->action_id = 0;

      if (flags & G_APPLICATION_IS_SERVICE)
        {
          g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                       "Unable to acquire bus name `%s'", appid);
          g_object_unref (impl->session_bus);
          g_free (impl->object_path);

          g_slice_free (GApplicationImpl, impl);
          return NULL;
        }
    }

  /* We are non-primary.  Try to get the primary's list of actions.
   * This also serves as a mechanism to ensure that the primary exists
   * (ie: DBus service files installed correctly, etc).
   */
  impl->signal_id =
    g_dbus_connection_signal_subscribe (impl->session_bus, impl->bus_name,
                                        "org.gtk.Actions", NULL,
                                        impl->object_path, NULL,
                                        G_DBUS_SIGNAL_FLAGS_NONE,
                                        g_application_impl_action_signal,
                                        impl, NULL);

  reply = g_dbus_connection_call_sync (impl->session_bus, impl->bus_name,
                                       impl->object_path, "org.gtk.Actions",
                                       "DescribeAll", NULL,
                                       G_VARIANT_TYPE ("(a(savbav))"),
                                       G_DBUS_CALL_FLAGS_NONE, -1,
                                       cancellable, error);

  if (reply == NULL)
    {
      /* The primary appears not to exist.  Fail the registration. */
      g_object_unref (impl->session_bus);
      g_free (impl->object_path);
      impl->session_bus = NULL;
      impl->object_path = NULL;

      g_slice_free (GApplicationImpl, impl);
      return NULL;
    }

  /* Create and populate the hashtable */
  {
    RemoteActionInfo *info;
    GVariant *descriptions;
    GVariantIter iter;

    *remote_actions = g_hash_table_new (g_str_hash, g_str_equal);
    descriptions = g_variant_get_child_value (reply, 0);
    g_variant_iter_init (&iter, descriptions);

    while ((info = remote_action_info_new_from_iter (&iter)))
      g_hash_table_insert (*remote_actions, info->name, info);

    g_variant_unref (descriptions);
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

static void
g_application_impl_cmdline_method_call (GDBusConnection       *connection,
                                        const gchar           *sender,
                                        const gchar           *object_path,
                                        const gchar           *interface_name,
                                        const gchar           *method_name,
                                        GVariant              *parameters,
                                        GDBusMethodInvocation *invocation,
                                        gpointer               user_data)
{
  const gchar *message;

  g_variant_get_child (parameters, 0, "&s", &message);

  if (strcmp (method_name, "Print") == 0)
    g_print ("%s", message);
  else if (strcmp (method_name, "PrintError") == 0)
    g_printerr ("%s", message);
  else
    g_assert_not_reached ();

  g_dbus_method_invocation_return_value (invocation, NULL);
}

typedef struct
{
  GMainLoop *loop;
  int status;
} CommandLineData;

static void
g_application_impl_cmdline_done (GObject      *source,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  CommandLineData *data = user_data;
  GError *error = NULL;
  GVariant *reply;

  reply = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source),
                                         result, &error);

  if (reply != NULL)
    {
      g_variant_get (reply, "(i)", &data->status);
      g_variant_unref (reply);
    }

  else
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      data->status = 1;
    }

  g_main_loop_quit (data->loop);
}

int
g_application_impl_command_line (GApplicationImpl  *impl,
                                 gchar            **arguments,
                                 GVariant          *platform_data)
{
  const static GDBusInterfaceVTable vtable = {
    g_application_impl_cmdline_method_call
  };
  const gchar *object_path = "/org/gtk/Application/CommandLine";
  GMainContext *context;
  CommandLineData data;
  guint object_id;

  context = g_main_context_new ();
  data.loop = g_main_loop_new (context, FALSE);
  g_main_context_push_thread_default (context);

  object_id = g_dbus_connection_register_object (impl->session_bus,
                                                 object_path,
                                                 get_interface ("org.gtk.private.CommandLine"),
                                                 &vtable, &data, NULL, NULL);
  /* In theory we should try other paths... */
  g_assert (object_id != 0);

  g_dbus_connection_call (impl->session_bus,
                          impl->bus_name,
                          impl->object_path,
                          "org.gtk.Application",
                          "CommandLine",
                          g_variant_new ("(o^aay@a{sv})", object_path,
                                         arguments, platform_data),
                          G_VARIANT_TYPE ("(i)"), 0, G_MAXINT, NULL,
                          g_application_impl_cmdline_done, &data);

  g_main_loop_run (data.loop);

  g_main_context_pop_thread_default (context);
  g_main_context_unref (context);
  g_main_loop_unref (data.loop);

  return data.status;
}

void
g_application_impl_change_action_state (GApplicationImpl *impl,
                                        const gchar      *action_name,
                                        GVariant         *value,
                                        GVariant         *platform_data)
{
  g_dbus_connection_call (impl->session_bus,
                          impl->bus_name,
                          impl->object_path,
                          "org.gtk.Actions",
                          "SetState",
                          g_variant_new ("(sv@a{sv})", action_name,
                                         value, platform_data),
                          NULL, 0, -1, NULL, NULL, NULL);
}

void
g_application_impl_activate_action (GApplicationImpl *impl,
                                    const gchar      *action_name,
                                    GVariant         *parameter,
                                    GVariant         *platform_data)
{
  GVariant *param;

  if (parameter)
    parameter = g_variant_new_variant (parameter);

  param = g_variant_new_array (G_VARIANT_TYPE_VARIANT,
                               &parameter, parameter != NULL);

  g_dbus_connection_call (impl->session_bus,
                          impl->bus_name,
                          impl->object_path,
                          "org.gtk.Actions",
                          "Activate",
                          g_variant_new ("(s@av@a{sv})", action_name,
                                         param, platform_data),
                          NULL, 0, -1, NULL, NULL, NULL);
}

void
g_application_impl_flush (GApplicationImpl *impl)
{
  if (impl->session_bus)
    g_dbus_connection_flush_sync (impl->session_bus, NULL, NULL);
}


/* GDBusCommandLine implementation {{{1 */

typedef GApplicationCommandLineClass GDBusCommandLineClass;
static GType g_dbus_command_line_get_type (void);
typedef struct
{
  GApplicationCommandLine  parent_instance;
  GDBusMethodInvocation   *invocation;

  GDBusConnection *connection;
  const gchar     *bus_name;
  const gchar     *object_path;
} GDBusCommandLine;


G_DEFINE_TYPE (GDBusCommandLine,
               g_dbus_command_line,
               G_TYPE_APPLICATION_COMMAND_LINE)

static void
g_dbus_command_line_print_literal (GApplicationCommandLine *cmdline,
                                   const gchar             *message)
{
  GDBusCommandLine *gdbcl = (GDBusCommandLine *) cmdline;

  g_dbus_connection_call (gdbcl->connection,
                          gdbcl->bus_name,
                          gdbcl->object_path,
                          "org.gtk.private.CommandLine", "Print",
                          g_variant_new ("(s)", message),
                          NULL, 0, -1, NULL, NULL, NULL);
}

static void
g_dbus_command_line_printerr_literal (GApplicationCommandLine *cmdline,
                                      const gchar             *message)
{
  GDBusCommandLine *gdbcl = (GDBusCommandLine *) cmdline;

  g_dbus_connection_call (gdbcl->connection,
                          gdbcl->bus_name,
                          gdbcl->object_path,
                          "org.gtk.private.CommandLine", "PrintError",
                          g_variant_new ("(s)", message),
                          NULL, 0, -1, NULL, NULL, NULL);
}

static void
g_dbus_command_line_finalize (GObject *object)
{
  GApplicationCommandLine *cmdline = G_APPLICATION_COMMAND_LINE (object);
  GDBusCommandLine *gdbcl = (GDBusCommandLine *) object;
  gint status;

  status = g_application_command_line_get_exit_status (cmdline);

  g_dbus_method_invocation_return_value (gdbcl->invocation,
                                         g_variant_new ("(i)", status));
  g_object_unref (gdbcl->invocation);

  G_OBJECT_CLASS (g_dbus_command_line_parent_class)
    ->finalize (object);
}

static void
g_dbus_command_line_init (GDBusCommandLine *gdbcl)
{
}

static void
g_dbus_command_line_class_init (GApplicationCommandLineClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = g_dbus_command_line_finalize;
  class->printerr_literal = g_dbus_command_line_printerr_literal;
  class->print_literal = g_dbus_command_line_print_literal;
}

static GApplicationCommandLine *
g_dbus_command_line_new (GDBusMethodInvocation *invocation)
{
  GDBusCommandLine *gdbcl;
  GVariant *args;

  args = g_dbus_method_invocation_get_parameters (invocation);

  gdbcl = g_object_new (g_dbus_command_line_get_type (),
                        "arguments", g_variant_get_child_value (args, 1),
                        "platform-data", g_variant_get_child_value (args, 2),
                        NULL);
  gdbcl->connection = g_dbus_method_invocation_get_connection (invocation);
  gdbcl->bus_name = g_dbus_method_invocation_get_sender (invocation);
  g_variant_get_child (args, 0, "&o", &gdbcl->object_path);
  gdbcl->invocation = g_object_ref (invocation);

  return G_APPLICATION_COMMAND_LINE (gdbcl);
}

/* Epilogue {{{1 */

/* vim:set foldmethod=marker: */
