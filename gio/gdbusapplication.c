/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2010 Red Hat, Inc
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
 * Authors: Colin Walters <walters@verbum.org>
 */

#define G_APPLICATION_IFACE "org.gtk.Application"

static void
application_dbus_method_call (GDBusConnection       *connection,
                              const gchar           *sender,
                              const gchar           *object_path,
                              const gchar           *interface_name,
                              const gchar           *method_name,
                              GVariant              *parameters,
                              GDBusMethodInvocation *invocation,
                              gpointer               user_data)
{
  GApplication *app = G_APPLICATION (user_data);

  if (method_name == NULL && *method_name == '\0')
    return;

  if (strcmp (method_name, "Quit") == 0)
    {
      GVariant *platform_data;

      g_variant_get (parameters, "(@a{sv})", &platform_data);

      g_dbus_method_invocation_return_value (invocation, NULL);

      g_application_quit_with_data (app, platform_data);

      g_variant_unref (platform_data);
    }
  else if (strcmp (method_name, "ListActions") == 0)
    {
      GHashTableIter iter;
      GApplicationAction *value;
      GVariantBuilder builder;

      g_variant_builder_init (&builder, G_VARIANT_TYPE ("(a{s(sb)})"));
      g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{s(sb)}"));
      g_hash_table_iter_init (&iter, app->priv->actions);
      while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&value))
        g_variant_builder_add (&builder, "{s(sb)}",
                               value->name,
                               value->description ? value->description : "",
                               value->enabled);
      g_variant_builder_close (&builder);

      g_dbus_method_invocation_return_value (invocation,
                                             g_variant_builder_end (&builder));
    }
  else if (strcmp (method_name, "InvokeAction") == 0)
    {
      const char *action_name;
      GVariant *platform_data;
      GApplicationAction *action;

      g_variant_get (parameters, "(&s@a{sv})", &action_name, &platform_data);

      action = g_hash_table_lookup (app->priv->actions, action_name);

      if (!action)
        {
          char *errmsg  = g_strdup_printf ("Invalid action: %s", action_name);
          g_dbus_method_invocation_return_dbus_error (invocation, G_APPLICATION_IFACE ".InvalidAction", errmsg);
          g_free (errmsg);
	  g_variant_unref (platform_data);
          return;
        }

      g_signal_emit (app, application_signals[ACTION_WITH_DATA],
		     g_quark_from_string (action_name), action_name, platform_data);

      g_dbus_method_invocation_return_value (invocation, NULL);
      g_variant_unref (platform_data);
    }
  else if (strcmp (method_name, "Activate") == 0)
    {
      GVariant *args;
      GVariant *platform_data;

      g_variant_get (parameters, "(@aay@a{sv})", &args, &platform_data);

      g_signal_emit (app, application_signals[PREPARE_ACTIVATION], 0, args, platform_data);

      g_variant_unref (args);
      g_variant_unref (platform_data);

      g_dbus_method_invocation_return_value (invocation, NULL);
    }
}

static const GDBusArgInfo application_quit_in_args[] =
{
  {
    -1,
    "platform_data",
    "a{sv}",
    NULL
  }
};

static const GDBusArgInfo * const application_quit_in_args_p[] = {
  &application_quit_in_args[0],
  NULL
};

static const GDBusArgInfo application_list_actions_out_args[] =
{
  {
    -1,
    "actions",
    "a{s(sb)}",
    NULL
  }
};

static const GDBusArgInfo * const application_list_actions_out_args_p[] = {
  &application_list_actions_out_args[0],
  NULL
};

static const GDBusArgInfo application_invoke_action_in_args[] =
{
  {
    -1,
    "action",
    "s",
    NULL
  },
  {
    -1,
    "platform_data",
    "a{sv}",
    NULL
  }
};

static const GDBusArgInfo * const application_invoke_action_in_args_p[] = {
  &application_invoke_action_in_args[0],
  &application_invoke_action_in_args[1],
  NULL
};

static const GDBusMethodInfo application_quit_method_info =
{
  -1,
  "Quit",
  (GDBusArgInfo **) &application_quit_in_args_p,
  NULL,
  NULL
};

static const GDBusMethodInfo application_list_actions_method_info =
{
  -1,
  "ListActions",
  NULL,
  (GDBusArgInfo **) &application_list_actions_out_args_p,
  NULL
};

static const GDBusMethodInfo application_invoke_action_method_info =
{
  -1,
  "InvokeAction",
  (GDBusArgInfo **) &application_invoke_action_in_args_p,
  NULL,
  NULL
};

static const GDBusArgInfo application_activate_in_args[] =
{
  {
    -1,
    "arguments",
    "aay",
    NULL
  },
  {
    -1,
    "data",
    "a{sv}",
    NULL
  }
};

static const GDBusArgInfo * const application_activate_in_args_p[] = {
  &application_activate_in_args[0],
  &application_activate_in_args[1],
  NULL
};

static const GDBusMethodInfo application_activate_method_info =
{
  -1,
  "Activate",
  (GDBusArgInfo **) &application_activate_in_args_p,
  NULL,
  NULL
};

static const GDBusMethodInfo * const application_dbus_method_info_p[] =
{
  &application_quit_method_info,
  &application_list_actions_method_info,
  &application_invoke_action_method_info,
  &application_activate_method_info,
  NULL
};

static const GDBusSignalInfo application_dbus_signal_info[] =
{
  {
    -1,
    "ActionsChanged",
    NULL,
    NULL
  }
};

static const GDBusSignalInfo * const application_dbus_signal_info_p[] = {
  &application_dbus_signal_info[0],
  NULL
};

static const GDBusInterfaceInfo application_dbus_interface_info =
{
  -1,
  G_APPLICATION_IFACE,
  (GDBusMethodInfo **) application_dbus_method_info_p,
  (GDBusSignalInfo **) application_dbus_signal_info_p,
  NULL,
};

static GDBusInterfaceVTable application_dbus_vtable =
{
  application_dbus_method_call,
  NULL,
  NULL
};

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

static gboolean
_g_application_platform_init (GApplication  *app,
			      GCancellable  *cancellable,
			      GError       **error)
{
  if (app->priv->session_bus == NULL)
    app->priv->session_bus = g_bus_get_sync (G_BUS_TYPE_SESSION, cancellable, error);
  if (!app->priv->session_bus)
    return FALSE;

  if (!app->priv->dbus_path)
    app->priv->dbus_path = application_path_from_appid (app->priv->appid);
  return TRUE;
}

static gboolean
_g_application_platform_register (GApplication  *app,
				  gboolean      *unique,
				  GCancellable  *cancellable,
				  GError       **error)
{
  GVariant *request_result;
  guint32 request_status;
  gboolean result;
  guint registration_id;

  registration_id = g_dbus_connection_register_object (app->priv->session_bus,
                                                       app->priv->dbus_path,
                                                       &application_dbus_interface_info,
                                                       &application_dbus_vtable,
                                                       app,
                                                       NULL,
                                                       error);
  if (registration_id == 0)
    return FALSE;

  request_result = g_dbus_connection_call_sync (app->priv->session_bus,
                                                "org.freedesktop.DBus",
                                                "/org/freedesktop/DBus",
                                                "org.freedesktop.DBus",
                                                "RequestName",
                                                g_variant_new ("(su)", app->priv->appid, 0x4),
                                                NULL, 0, -1, cancellable, error);

  if (request_result == NULL)
    {
      result = FALSE;
      goto done;
    }

  if (g_variant_is_of_type (request_result, G_VARIANT_TYPE ("(u)")))
    g_variant_get (request_result, "(u)", &request_status);
  else
    request_status = 0;

  g_variant_unref (request_result);

  *unique = (request_status == 1 || request_status == 4);
  result = TRUE;

  if (*unique)
    {
      app->priv->is_remote = FALSE;
    }
  else if (app->priv->default_quit)
    {
      GVariantBuilder builder;
      GVariant *message;
      GVariant *result;

      g_variant_builder_init (&builder, G_VARIANT_TYPE ("(aaya{sv})"));
      g_variant_builder_add (&builder, "@aay", app->priv->argv);

      if (app->priv->platform_data)
	g_variant_builder_add (&builder, "@a{sv}", app->priv->platform_data);
      else
	{
	  g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{sv}"));
	  g_variant_builder_close (&builder);
	}

      message = g_variant_builder_end (&builder);

      result = g_dbus_connection_call_sync (app->priv->session_bus,
					    app->priv->appid,
					    app->priv->dbus_path,
					    G_APPLICATION_IFACE,
					    "Activate",
					    message,
					    NULL, 0, -1, NULL, NULL);

      if (result)
	g_variant_unref (result);

      exit (0);
    }

done:
  if (!result)
    g_dbus_connection_unregister_object (app->priv->session_bus, registration_id);

  return result;
}

static void
_g_application_platform_on_actions_changed (GApplication *app)
{
  g_dbus_connection_emit_signal (app->priv->session_bus, NULL,
                                 app->priv->dbus_path,
                                 G_APPLICATION_IFACE,
                                 "ActionsChanged", NULL, NULL);
}

static void
_g_application_platform_remote_invoke_action (GApplication  *app,
                                              const gchar   *action,
                                              GVariant      *platform_data)
{
  GVariant *result;

  result = g_dbus_connection_call_sync (app->priv->session_bus,
                                        app->priv->appid,
                                        app->priv->dbus_path,
                                        G_APPLICATION_IFACE,
                                        "InvokeAction",
                                        g_variant_new ("(s@a{sv})",
                                                       action,
                                                       platform_data),
                                        NULL, 0, -1, NULL, NULL);
  if (result)
    g_variant_unref (result);
}

static void
_g_application_platform_remote_quit (GApplication *app,
                                     GVariant     *platform_data)
{
  GVariant *result;

  result = g_dbus_connection_call_sync (app->priv->session_bus,
                                        app->priv->appid,
                                        app->priv->dbus_path,
                                        G_APPLICATION_IFACE,
                                        "Quit",
                                        g_variant_new ("(@a{sv})",
                                                       platform_data),
                                        NULL, 0, -1, NULL, NULL);
  if (result)
    g_variant_unref (result);
}

