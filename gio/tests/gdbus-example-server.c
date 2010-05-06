/*
 * Copyright © 2010 Red Hat, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 *
 * Author: David Zeuthen <davidz@redhat.com>
 */

#include <gio/gio.h>
#include <stdlib.h>

#ifdef G_OS_UNIX
/* For STDOUT_FILENO */
#include <unistd.h>
#endif

/* ---------------------------------------------------------------------------------------------------- */

static GDBusNodeInfo *introspection_data = NULL;

/* Introspection data for the service we are exporting */
static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='org.gtk.GDBus.TestInterface'>"
  "    <method name='HelloWorld'>"
  "      <arg type='s' name='greeting' direction='in'/>"
  "      <arg type='s' name='response' direction='out'/>"
  "    </method>"
  "    <method name='EmitSignal'>"
  "      <arg type='d' name='speed_in_mph' direction='in'/>"
  "    </method>"
  "    <method name='GimmeStdout'/>"
  "    <signal name='VelocityChanged'>"
  "      <arg type='d' name='speed_in_mph'/>"
  "      <arg type='s' name='speed_as_string'/>"
  "    </signal>"
  "    <property type='s' name='FluxCapicitorName' access='read'/>"
  "    <property type='s' name='Title' access='readwrite'/>"
  "    <property type='s' name='ReadingAlwaysThrowsError' access='read'/>"
  "    <property type='s' name='WritingAlwaysThrowsError' access='readwrite'/>"
  "    <property type='s' name='OnlyWritable' access='write'/>"
  "    <property type='s' name='Foo' access='read'/>"
  "    <property type='s' name='Bar' access='read'/>"
  "  </interface>"
  "</node>";

/* ---------------------------------------------------------------------------------------------------- */

static void
handle_method_call (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
  if (g_strcmp0 (method_name, "HelloWorld") == 0)
    {
      const gchar *greeting;

      g_variant_get (parameters, "(s)", &greeting);

      if (g_strcmp0 (greeting, "Return Unregistered") == 0)
        {
          g_dbus_method_invocation_return_error (invocation,
                                                 G_IO_ERROR,
                                                 G_IO_ERROR_FAILED_HANDLED,
                                                 "As requested, here's a GError not registered (G_IO_ERROR_FAILED_HANDLED)");
        }
      else if (g_strcmp0 (greeting, "Return Registered") == 0)
        {
          g_dbus_method_invocation_return_error (invocation,
                                                 G_DBUS_ERROR,
                                                 G_DBUS_ERROR_MATCH_RULE_NOT_FOUND,
                                                 "As requested, here's a GError that is registered (G_DBUS_ERROR_MATCH_RULE_NOT_FOUND)");
        }
      else if (g_strcmp0 (greeting, "Return Raw") == 0)
        {
          g_dbus_method_invocation_return_dbus_error (invocation,
                                                      "org.gtk.GDBus.SomeErrorName",
                                                      "As requested, here's a raw D-Bus error");
        }
      else
        {
          gchar *response;
          response = g_strdup_printf ("You greeted me with '%s'. Thanks!", greeting);
          g_dbus_method_invocation_return_value (invocation,
                                                 g_variant_new ("(s)", response));
          g_free (response);
        }
    }
  else if (g_strcmp0 (method_name, "EmitSignal") == 0)
    {
      GError *local_error;
      gdouble speed_in_mph;
      gchar *speed_as_string;

      g_variant_get (parameters, "(d)", &speed_in_mph);
      speed_as_string = g_strdup_printf ("%g mph!", speed_in_mph);

      local_error = NULL;
      g_dbus_connection_emit_signal (connection,
                                     NULL,
                                     object_path,
                                     interface_name,
                                     "VelocityChanged",
                                     g_variant_new ("(ds)",
                                                    speed_in_mph,
                                                    speed_as_string),
                                     &local_error);
      g_assert_no_error (local_error);
      g_free (speed_as_string);

      g_dbus_method_invocation_return_value (invocation, NULL);
    }
  else if (g_strcmp0 (method_name, "GimmeStdout") == 0)
    {
#ifdef G_OS_UNIX
      if (g_dbus_connection_get_capabilities (connection) & G_DBUS_CAPABILITY_FLAGS_UNIX_FD_PASSING)
        {
          GDBusMessage *reply;
          GUnixFDList *fd_list;
          GError *error;

          fd_list = g_unix_fd_list_new ();
          error = NULL;
          g_unix_fd_list_append (fd_list, STDOUT_FILENO, &error);
          g_assert_no_error (error);

          reply = g_dbus_message_new_method_reply (g_dbus_method_invocation_get_message (invocation));
          g_dbus_message_set_unix_fd_list (reply, fd_list);

          error = NULL;
          g_dbus_connection_send_message (connection,
                                          reply,
                                          NULL, /* out_serial */
                                          &error);
          g_assert_no_error (error);

          g_object_unref (invocation);
          g_object_unref (fd_list);
          g_object_unref (reply);
        }
      else
        {
          g_dbus_method_invocation_return_dbus_error (invocation,
                                                      "org.gtk.GDBus.Failed",
                                                      "Your message bus daemon does not support file descriptor passing (need D-Bus >= 1.3.0)");
        }
#else
      g_dbus_method_invocation_return_dbus_error (invocation,
                                                  "org.gtk.GDBus.NotOnUnix",
                                                  "Your OS does not support file descriptor passing");
#endif
    }
}

static gchar *_global_title = NULL;

static gboolean swap_a_and_b = FALSE;

static GVariant *
handle_get_property (GDBusConnection  *connection,
                     const gchar      *sender,
                     const gchar      *object_path,
                     const gchar      *interface_name,
                     const gchar      *property_name,
                     GError          **error,
                     gpointer          user_data)
{
  GVariant *ret;

  ret = NULL;
  if (g_strcmp0 (property_name, "FluxCapicitorName") == 0)
    {
      ret = g_variant_new_string ("DeLorean");
    }
  else if (g_strcmp0 (property_name, "Title") == 0)
    {
      if (_global_title == NULL)
        _global_title = g_strdup ("Back To C!");
      ret = g_variant_new_string (_global_title);
    }
  else if (g_strcmp0 (property_name, "ReadingAlwaysThrowsError") == 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Hello %s. I thought I said reading this property "
                   "always results in an error. kthxbye",
                   sender);
    }
  else if (g_strcmp0 (property_name, "WritingAlwaysThrowsError") == 0)
    {
      ret = g_variant_new_string ("There's no home like home");
    }
  else if (g_strcmp0 (property_name, "Foo") == 0)
    {
      ret = g_variant_new_string (swap_a_and_b ? "Tock" : "Tick");
    }
  else if (g_strcmp0 (property_name, "Bar") == 0)
    {
      ret = g_variant_new_string (swap_a_and_b ? "Tick" : "Tock");
    }

  return ret;
}

static gboolean
handle_set_property (GDBusConnection  *connection,
                     const gchar      *sender,
                     const gchar      *object_path,
                     const gchar      *interface_name,
                     const gchar      *property_name,
                     GVariant         *value,
                     GError          **error,
                     gpointer          user_data)
{
  if (g_strcmp0 (property_name, "Title") == 0)
    {
      if (g_strcmp0 (_global_title, g_variant_get_string (value, NULL)) != 0)
        {
          GVariantBuilder *builder;
          GError *local_error;

          g_free (_global_title);
          _global_title = g_variant_dup_string (value, NULL);

          local_error = NULL;
          builder = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);
          g_variant_builder_add (builder,
                                 "{sv}",
                                 "Title",
                                 g_variant_new_string (_global_title));
          g_dbus_connection_emit_signal (connection,
                                         NULL,
                                         object_path,
                                         "org.freedesktop.DBus.Properties",
                                         "PropertiesChanged",
                                         g_variant_new ("(sa{sv})",
                                                        interface_name,
                                                        builder),
                                         &local_error);
          g_assert_no_error (local_error);
        }
    }
  else if (g_strcmp0 (property_name, "ReadingAlwaysThrowsError") == 0)
    {
      /* do nothing - they can't read it after all! */
    }
  else if (g_strcmp0 (property_name, "WritingAlwaysThrowsError") == 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Hello AGAIN %s. I thought I said writing this property "
                   "always results in an error. kthxbye",
                   sender);
    }

  return *error == NULL;
}


/* for now */
static const GDBusInterfaceVTable interface_vtable =
{
  handle_method_call,
  handle_get_property,
  handle_set_property
};

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
on_timeout_cb (gpointer user_data)
{
  GDBusConnection *connection = G_DBUS_CONNECTION (user_data);
  GVariantBuilder *builder;
  GError *error;

  swap_a_and_b = !swap_a_and_b;

  error = NULL;
  builder = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);
  g_variant_builder_add (builder,
                         "{sv}",
                         "Foo",
                         g_variant_new_string (swap_a_and_b ? "Tock" : "Tick"));
  g_variant_builder_add (builder,
                         "{sv}",
                         "Bar",
                         g_variant_new_string (swap_a_and_b ? "Tick" : "Tock"));
  g_dbus_connection_emit_signal (connection,
                                 NULL,
                                 "/org/gtk/GDBus/TestObject",
                                 "org.freedesktop.DBus.Properties",
                                 "PropertiesChanged",
                                 g_variant_new ("(sa{sv})",
                                                "org.gtk.GDBus.TestInterface",
                                                builder),
                                 &error);
  g_assert_no_error (error);


  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
  guint registration_id;

  registration_id = g_dbus_connection_register_object (connection,
                                                       "/org/gtk/GDBus/TestObject",
                                                       "org.gtk.GDBus.TestInterface",
                                                       introspection_data->interfaces[0],
                                                       &interface_vtable,
                                                       NULL,  /* user_data */
                                                       NULL,  /* user_data_free_func */
                                                       NULL); /* GError** */
  g_assert (registration_id > 0);

  /* swap value of properties Foo and Bar every two seconds */
  g_timeout_add_seconds (2,
                         on_timeout_cb,
                         connection);
}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
  exit (1);
}

int
main (int argc, char *argv[])
{
  guint owner_id;
  GMainLoop *loop;

  g_type_init ();

  /* We are lazy here - we don't want to manually provide
   * the introspection data structures - so we just build
   * them from XML.
   */
  introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
  g_assert (introspection_data != NULL);

  owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                             "org.gtk.GDBus.TestServer",
                             G_BUS_NAME_OWNER_FLAGS_NONE,
                             on_bus_acquired,
                             on_name_acquired,
                             on_name_lost,
                             NULL,
                             NULL);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_bus_unown_name (owner_id);

  g_dbus_node_info_unref (introspection_data);

  return 0;
}
