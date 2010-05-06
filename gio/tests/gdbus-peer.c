/* GLib testing framework examples and tests
 *
 * Copyright (C) 2008-2009 Red Hat, Inc.
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
 * Author: David Zeuthen <davidz@redhat.com>
 */

#include <gio/gio.h>
#include <unistd.h>
#include <string.h>

/* for open(2) */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <gio/gunixsocketaddress.h>

#include "gdbus-tests.h"


#ifdef G_OS_UNIX
static gboolean is_unix = TRUE;
#else
static gboolean is_unix = FALSE;
#endif

static gchar *test_guid = NULL;
static GMainLoop *service_loop = NULL;
static GDBusServer *server = NULL;
static GMainLoop *loop = NULL;

/* ---------------------------------------------------------------------------------------------------- */
/* Test that peer-to-peer connections work */
/* ---------------------------------------------------------------------------------------------------- */


typedef struct
{
  gboolean accept_connection;
  gint num_connection_attempts;
  GPtrArray *current_connections;
  guint num_method_calls;
  gboolean signal_received;
} PeerData;

static const gchar *test_interface_introspection_xml =
  "<node>"
  "  <interface name='org.gtk.GDBus.PeerTestInterface'>"
  "    <method name='HelloPeer'>"
  "      <arg type='s' name='greeting' direction='in'/>"
  "      <arg type='s' name='response' direction='out'/>"
  "    </method>"
  "    <method name='EmitSignal'/>"
  "    <method name='OpenFile'>"
  "      <arg type='s' name='path' direction='in'/>"
  "    </method>"
  "    <signal name='PeerSignal'>"
  "      <arg type='s' name='a_string'/>"
  "    </signal>"
  "    <property type='s' name='PeerProperty' access='read'/>"
  "  </interface>"
  "</node>";
static const GDBusInterfaceInfo *test_interface_introspection_data = NULL;

static void
test_interface_method_call (GDBusConnection       *connection,
                            const gchar           *sender,
                            const gchar           *object_path,
                            const gchar           *interface_name,
                            const gchar           *method_name,
                            GVariant              *parameters,
                            GDBusMethodInvocation *invocation,
                            gpointer               user_data)
{
  PeerData *data = user_data;

  data->num_method_calls++;

  g_assert_cmpstr (object_path, ==, "/org/gtk/GDBus/PeerTestObject");
  g_assert_cmpstr (interface_name, ==, "org.gtk.GDBus.PeerTestInterface");

  if (g_strcmp0 (method_name, "HelloPeer") == 0)
    {
      const gchar *greeting;
      gchar *response;

      g_variant_get (parameters, "(s)", &greeting);

      response = g_strdup_printf ("You greeted me with '%s'.",
                                  greeting);
      g_dbus_method_invocation_return_value (invocation,
                                             g_variant_new ("(s)", response));
      g_free (response);
    }
  else if (g_strcmp0 (method_name, "EmitSignal") == 0)
    {
      GError *error;

      error = NULL;
      g_dbus_connection_emit_signal (connection,
                                     NULL,
                                     "/org/gtk/GDBus/PeerTestObject",
                                     "org.gtk.GDBus.PeerTestInterface",
                                     "PeerSignal",
                                     NULL,
                                     &error);
      g_assert_no_error (error);
      g_dbus_method_invocation_return_value (invocation, NULL);
    }
  else if (g_strcmp0 (method_name, "OpenFile") == 0)
    {
      const gchar *path;
      GDBusMessage *reply;
      GError *error;
      gint fd;
      GUnixFDList *fd_list;

      g_variant_get (parameters, "(s)", &path);

      fd_list = g_unix_fd_list_new ();

      error = NULL;

      fd = open (path, O_RDONLY);
      g_unix_fd_list_append (fd_list, fd, &error);
      g_assert_no_error (error);
      close (fd);

      reply = g_dbus_message_new_method_reply (g_dbus_method_invocation_get_message (invocation));
      g_dbus_message_set_unix_fd_list (reply, fd_list);
      g_object_unref (invocation);

      error = NULL;
      g_dbus_connection_send_message (connection,
                                      reply,
                                      NULL, /* out_serial */
                                      &error);
      g_assert_no_error (error);
      g_object_unref (reply);
    }
  else
    {
      g_assert_not_reached ();
    }
}

static GVariant *
test_interface_get_property (GDBusConnection  *connection,
                             const gchar      *sender,
                             const gchar      *object_path,
                             const gchar      *interface_name,
                             const gchar      *property_name,
                             GError          **error,
                             gpointer          user_data)
{
  g_assert_cmpstr (object_path, ==, "/org/gtk/GDBus/PeerTestObject");
  g_assert_cmpstr (interface_name, ==, "org.gtk.GDBus.PeerTestInterface");
  g_assert_cmpstr (property_name, ==, "PeerProperty");

  return g_variant_new_string ("ThePropertyValue");
}


static const GDBusInterfaceVTable test_interface_vtable =
{
  test_interface_method_call,
  test_interface_get_property,
  NULL  /* set_property */
};

static void
on_proxy_signal_received (GDBusProxy *proxy,
                          gchar      *sender_name,
                          gchar      *signal_name,
                          GVariant   *parameters,
                          gpointer    user_data)
{
  PeerData *data = user_data;

  data->signal_received = TRUE;

  g_assert (sender_name == NULL);
  g_assert_cmpstr (signal_name, ==, "PeerSignal");
  g_main_loop_quit (loop);
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
on_deny_authenticated_peer (GDBusAuthObserver *observer,
                            GIOStream         *stream,
                            GCredentials      *credentials,
                            gpointer           user_data)
{
  PeerData *data = user_data;
  gboolean deny_peer;

  data->num_connection_attempts++;

  deny_peer = FALSE;
  if (!data->accept_connection)
    {
      deny_peer = TRUE;
      g_main_loop_quit (loop);
    }

  return deny_peer;
}

/* Runs in thread we created GDBusServer in (since we didn't pass G_DBUS_SERVER_FLAGS_RUN_IN_THREAD) */
static void
on_new_connection (GDBusServer *server,
                   GDBusConnection *connection,
                   gpointer user_data)
{
  PeerData *data = user_data;
  GError *error;
  guint reg_id;

  //g_print ("Client connected.\n"
  //         "Negotiated capabilities: unix-fd-passing=%d\n",
  //         g_dbus_connection_get_capabilities (connection) & G_DBUS_CAPABILITY_FLAGS_UNIX_FD_PASSING);

  g_ptr_array_add (data->current_connections, g_object_ref (connection));

  /* export object on the newly established connection */
  error = NULL;
  reg_id = g_dbus_connection_register_object (connection,
                                              "/org/gtk/GDBus/PeerTestObject",
                                              "org.gtk.GDBus.PeerTestInterface",
                                              test_interface_introspection_data,
                                              &test_interface_vtable,
                                              data,
                                              NULL, /* GDestroyNotify for data */
                                              &error);
  g_assert_no_error (error);
  g_assert (reg_id > 0);

  g_main_loop_quit (loop);
}

static gpointer
service_thread_func (gpointer user_data)
{
  PeerData *data = user_data;
  GMainContext *service_context;
  GDBusAuthObserver *observer;
  GError *error;

  service_context = g_main_context_new ();
  g_main_context_push_thread_default (service_context);

  error = NULL;
  observer = g_dbus_auth_observer_new ();
  server = g_dbus_server_new_sync (is_unix ? "unix:tmpdir=/tmp/gdbus-test-" : "nonce-tcp:",
                                   G_DBUS_SERVER_FLAGS_NONE,
                                   test_guid,
                                   observer,
                                   NULL, /* cancellable */
                                   &error);
  g_assert_no_error (error);

  g_signal_connect (server,
                    "new-connection",
                    G_CALLBACK (on_new_connection),
                    data);
  g_signal_connect (observer,
                    "deny-authenticated-peer",
                    G_CALLBACK (on_deny_authenticated_peer),
                    data);
  g_object_unref (observer);

  g_dbus_server_start (server);

  service_loop = g_main_loop_new (service_context, FALSE);
  g_main_loop_run (service_loop);

  g_main_context_pop_thread_default (service_context);

  g_main_loop_unref (service_loop);
  g_main_context_unref (service_context);

  /* test code specifically unrefs the server - see below */
  g_assert (server == NULL);

  return NULL;
}

#if 0
static gboolean
on_incoming_connection (GSocketService     *service,
                        GSocketConnection  *socket_connection,
                        GObject            *source_object,
                        gpointer           user_data)
{
  PeerData *data = user_data;

  if (data->accept_connection)
    {
      GError *error;
      guint reg_id;
      GDBusConnection *connection;

      error = NULL;
      connection = g_dbus_connection_new_sync (G_IO_STREAM (socket_connection),
                                               test_guid,
                                               G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_SERVER,
                                               NULL, /* cancellable */
                                               &error);
      g_assert_no_error (error);

      g_ptr_array_add (data->current_connections, connection);

      /* export object on the newly established connection */
      error = NULL;
      reg_id = g_dbus_connection_register_object (connection,
                                                  "/org/gtk/GDBus/PeerTestObject",
                                                  "org.gtk.GDBus.PeerTestInterface",
                                                  &test_interface_introspection_data,
                                                  &test_interface_vtable,
                                                  data,
                                                  NULL, /* GDestroyNotify for data */
                                                  &error);
      g_assert_no_error (error);
      g_assert (reg_id > 0);

    }
  else
    {
      /* don't do anything */
    }

  data->num_connection_attempts++;

  g_main_loop_quit (loop);

  /* stops other signal handlers from being invoked */
  return TRUE;
}

static gpointer
service_thread_func (gpointer data)
{
  GMainContext *service_context;
  gchar *socket_path;
  GSocketAddress *address;
  GError *error;

  service_context = g_main_context_new ();
  g_main_context_push_thread_default (service_context);

  socket_path = g_strdup_printf ("/tmp/gdbus-test-pid-%d", getpid ());
  address = g_unix_socket_address_new (socket_path);

  service = g_socket_service_new ();
  error = NULL;
  g_socket_listener_add_address (G_SOCKET_LISTENER (service),
                                 address,
                                 G_SOCKET_TYPE_STREAM,
                                 G_SOCKET_PROTOCOL_DEFAULT,
                                 NULL, /* source_object */
                                 NULL, /* effective_address */
                                 &error);
  g_assert_no_error (error);
  g_signal_connect (service,
                    "incoming",
                    G_CALLBACK (on_incoming_connection),
                    data);
  g_socket_service_start (service);

  service_loop = g_main_loop_new (service_context, FALSE);
  g_main_loop_run (service_loop);

  g_main_context_pop_thread_default (service_context);

  g_main_loop_unref (service_loop);
  g_main_context_unref (service_context);

  g_object_unref (address);
  g_free (socket_path);
  return NULL;
}
#endif

/* ---------------------------------------------------------------------------------------------------- */

#if 0
static gboolean
check_connection (gpointer user_data)
{
  PeerData *data = user_data;
  guint n;

  for (n = 0; n < data->current_connections->len; n++)
    {
      GDBusConnection *c;
      GIOStream *stream;

      c = G_DBUS_CONNECTION (data->current_connections->pdata[n]);
      stream = g_dbus_connection_get_stream (c);

      g_debug ("In check_connection for %d: connection %p, stream %p", n, c, stream);
      g_debug ("closed = %d", g_io_stream_is_closed (stream));

      GSocket *socket;
      socket = g_socket_connection_get_socket (G_SOCKET_CONNECTION (stream));
      g_debug ("socket_closed = %d", g_socket_is_closed (socket));
      g_debug ("socket_condition_check = %d", g_socket_condition_check (socket, G_IO_IN|G_IO_OUT|G_IO_ERR|G_IO_HUP));

      gchar buf[128];
      GError *error;
      gssize num_read;
      error = NULL;
      num_read = g_input_stream_read (g_io_stream_get_input_stream (stream),
                                      buf,
                                      128,
                                      NULL,
                                      &error);
      if (num_read < 0)
        {
          g_debug ("error: %s", error->message);
          g_error_free (error);
        }
      else
        {
          g_debug ("no error, read %d bytes", (gint) num_read);
        }
    }

  return FALSE;
}

static gboolean
on_do_disconnect_in_idle (gpointer data)
{
  GDBusConnection *c = G_DBUS_CONNECTION (data);
  g_debug ("GDC %p has ref_count %d", c, G_OBJECT (c)->ref_count);
  g_dbus_connection_disconnect (c);
  g_object_unref (c);
  return FALSE;
}
#endif

static void
test_peer (void)
{
  GDBusConnection *c;
  GDBusConnection *c2;
  GDBusProxy *proxy;
  GError *error;
  PeerData data;
  GVariant *value;
  GVariant *result;
  const gchar *s;
  GThread *service_thread;

  memset (&data, '\0', sizeof (PeerData));
  data.current_connections = g_ptr_array_new_with_free_func (g_object_unref);

  /* first try to connect when there is no server */
  error = NULL;
  c = g_dbus_connection_new_for_address_sync (is_unix ? "unix:path=/tmp/gdbus-test-does-not-exist-pid" :
                                              /* NOTE: Even if something is listening on port 12345 the connection
                                               * will fail because the nonce file doesn't exist */
                                              "nonce-tcp:host=localhost,port=12345,noncefile=this-does-not-exist-gdbus",
                                              G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
                                              NULL, /* cancellable */
                                              &error);
  _g_assert_error_domain (error, G_IO_ERROR);
  g_assert (!g_dbus_error_is_remote_error (error));
  g_clear_error (&error);
  g_assert (c == NULL);

  /* bring up a server - we run the server in a different thread to avoid deadlocks */
  error = NULL;
  service_thread = g_thread_create (service_thread_func,
                                    &data,
                                    TRUE,
                                    &error);
  while (service_loop == NULL)
    g_thread_yield ();
  g_assert (server != NULL);

  /* bring up a connection and accept it */
  data.accept_connection = TRUE;
  error = NULL;
  c = g_dbus_connection_new_for_address_sync (g_dbus_server_get_client_address (server),
                                              G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
                                              NULL, /* cancellable */
                                              &error);
  g_assert_no_error (error);
  g_assert (c != NULL);
  while (data.current_connections->len < 1)
    g_main_loop_run (loop);
  g_assert_cmpint (data.current_connections->len, ==, 1);
  g_assert_cmpint (data.num_connection_attempts, ==, 1);
  //g_assert (g_dbus_connection_get_bus_type (c) == G_BUS_TYPE_NONE);
  g_assert (g_dbus_connection_get_unique_name (c) == NULL);
  g_assert_cmpstr (g_dbus_connection_get_guid (c), ==, test_guid);

  /* check that we create a proxy, read properties, receive signals and invoke
   * the HelloPeer() method. Since the server runs in another thread it's fine
   * to use synchronous blocking API here.
   */
  error = NULL;
  proxy = g_dbus_proxy_new_sync (c,
                                 G_TYPE_DBUS_PROXY,
                                 G_DBUS_PROXY_FLAGS_NONE,
                                 NULL,
                                 NULL, /* bus_name */
                                 "/org/gtk/GDBus/PeerTestObject",
                                 "org.gtk.GDBus.PeerTestInterface",
                                 NULL, /* GCancellable */
                                 &error);
  g_assert_no_error (error);
  g_assert (proxy != NULL);
  error = NULL;
  value = g_dbus_proxy_get_cached_property (proxy, "PeerProperty", &error);
  g_assert_no_error (error);
  g_assert_cmpstr (g_variant_get_string (value, NULL), ==, "ThePropertyValue");

  /* try invoking a method */
  error = NULL;
  result = g_dbus_proxy_invoke_method_sync (proxy,
                                            "HelloPeer",
                                            g_variant_new ("(s)", "Hey Peer!"),
                                            G_DBUS_INVOKE_METHOD_FLAGS_NONE,
                                            -1,
                                            NULL,  /* GCancellable */
                                            &error);
  g_assert_no_error (error);
  g_variant_get (result, "(s)", &s);
  g_assert_cmpstr (s, ==, "You greeted me with 'Hey Peer!'.");
  g_variant_unref (result);
  g_assert_cmpint (data.num_method_calls, ==, 1);

  /* make the other peer emit a signal - catch it */
  g_signal_connect (proxy,
                    "g-signal",
                    G_CALLBACK (on_proxy_signal_received),
                    &data);
  g_assert (!data.signal_received);
  g_dbus_proxy_invoke_method (proxy,
                              "EmitSignal",
                              NULL,  /* no arguments */
                              G_DBUS_INVOKE_METHOD_FLAGS_NONE,
                              -1,
                              NULL,  /* GCancellable */
                              NULL,  /* GAsyncReadyCallback - we don't care about the result */
                              NULL); /* user_data */
  g_main_loop_run (loop);
  g_assert (data.signal_received);
  g_assert_cmpint (data.num_method_calls, ==, 2);

  /* check for UNIX fd passing */
#ifdef G_OS_UNIX
  {
    GDBusMessage *method_call_message;
    GDBusMessage *method_reply_message;
    GUnixFDList *fd_list;
    gint fd;
    gchar buf[1024];
    gssize len;
    gchar *buf2;
    gsize len2;

    method_call_message = g_dbus_message_new_method_call (NULL, /* name */
                                                          "/org/gtk/GDBus/PeerTestObject",
                                                          "org.gtk.GDBus.PeerTestInterface",
                                                          "OpenFile");
    g_dbus_message_set_body (method_call_message, g_variant_new ("(s)", "/etc/hosts"));
    error = NULL;
    method_reply_message = g_dbus_connection_send_message_with_reply_sync (c,
                                                                           method_call_message,
                                                                           -1,
                                                                           NULL, /* out_serial */
                                                                           NULL, /* cancellable */
                                                                           &error);
    g_assert_no_error (error);
    g_assert (g_dbus_message_get_type (method_reply_message) == G_DBUS_MESSAGE_TYPE_METHOD_RETURN);
    fd_list = g_dbus_message_get_unix_fd_list (method_reply_message);
    g_assert (fd_list != NULL);
    g_assert_cmpint (g_unix_fd_list_get_length (fd_list), ==, 1);
    error = NULL;
    fd = g_unix_fd_list_get (fd_list, 0, &error);
    g_assert_no_error (error);
    g_object_unref (method_call_message);
    g_object_unref (method_reply_message);

    memset (buf, '\0', sizeof (buf));
    len = read (fd, buf, sizeof (buf) - 1);
    close (fd);

    error = NULL;
    g_file_get_contents ("/etc/hosts",
                         &buf2,
                         &len2,
                         &error);
    g_assert_no_error (error);
    if (len2 > sizeof (buf))
      buf2[sizeof (buf)] = '\0';
    g_assert_cmpstr (buf, ==, buf2);
    g_free (buf2);
  }
#endif /* G_OS_UNIX */


  /* bring up a connection - don't accept it - this should fail
   */
  data.accept_connection = FALSE;
  error = NULL;
  c2 = g_dbus_connection_new_for_address_sync (g_dbus_server_get_client_address (server),
                                               G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
                                               NULL, /* cancellable */
                                               &error);
  _g_assert_error_domain (error, G_IO_ERROR);
  g_assert (c2 == NULL);

#if 0
  /* TODO: THIS TEST DOESN'T WORK YET */

  /* bring up a connection - accept it.. then disconnect from the client side - check
   * that the server side gets the disconnect signal.
   */
  error = NULL;
  data.accept_connection = TRUE;
  c2 = g_dbus_connection_new_for_address_sync (g_dbus_server_get_client_address (server),
                                               G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
                                               NULL, /* cancellable */
                                               &error);
  g_assert_no_error (error);
  g_assert (c2 != NULL);
  g_assert (!g_dbus_connection_get_is_disconnected (c2));
  while (data.num_connection_attempts < 3)
    g_main_loop_run (loop);
  g_assert_cmpint (data.current_connections->len, ==, 2);
  g_assert_cmpint (data.num_connection_attempts, ==, 3);
  g_assert (!g_dbus_connection_get_is_disconnected (G_DBUS_CONNECTION (data.current_connections->pdata[1])));
  g_idle_add (on_do_disconnect_in_idle, c2);
  g_debug ("==================================================");
  g_debug ("==================================================");
  g_debug ("==================================================");
  g_debug ("waiting for disconnect on connection %p, stream %p",
           data.current_connections->pdata[1],
           g_dbus_connection_get_stream (data.current_connections->pdata[1]));

  g_timeout_add (2000, check_connection, &data);
  //_g_assert_signal_received (G_DBUS_CONNECTION (data.current_connections->pdata[1]), "closed");
  g_main_loop_run (loop);
  g_assert (g_dbus_connection_get_is_disconnected (G_DBUS_CONNECTION (data.current_connections->pdata[1])));
  g_ptr_array_set_size (data.current_connections, 1); /* remove disconnected connection object */
#endif

  /* unref the server and stop listening for new connections
   *
   * This won't bring down the established connections - check that c is still connected
   * by invoking a method
   */
  //g_socket_service_stop (service);
  //g_object_unref (service);
  g_dbus_server_stop (server);
  g_object_unref (server);
  server = NULL;

  error = NULL;
  result = g_dbus_proxy_invoke_method_sync (proxy,
                                            "HelloPeer",
                                            g_variant_new ("(s)", "Hey Again Peer!"),
                                            G_DBUS_INVOKE_METHOD_FLAGS_NONE,
                                            -1,
                                            NULL,  /* GCancellable */
                                            &error);
  g_assert_no_error (error);
  g_variant_get (result, "(s)", &s);
  g_assert_cmpstr (s, ==, "You greeted me with 'Hey Again Peer!'.");
  g_variant_unref (result);
  g_assert_cmpint (data.num_method_calls, ==, 4);

#if 0
  /* TODO: THIS TEST DOESN'T WORK YET */

  /* now disconnect from the server side - check that the client side gets the signal */
  g_assert_cmpint (data.current_connections->len, ==, 1);
  g_assert (G_DBUS_CONNECTION (data.current_connections->pdata[0]) != c);
  g_dbus_connection_disconnect (G_DBUS_CONNECTION (data.current_connections->pdata[0]));
  if (!g_dbus_connection_get_is_disconnected (c))
    _g_assert_signal_received (c, "closed");
  g_assert (g_dbus_connection_get_is_disconnected (c));
#endif

  g_object_unref (c);
  g_ptr_array_unref (data.current_connections);
  g_object_unref (proxy);

  g_main_loop_quit (service_loop);
  g_thread_join (service_thread);
}

/* ---------------------------------------------------------------------------------------------------- */

int
main (int   argc,
      char *argv[])
{
  gint ret;
  GDBusNodeInfo *introspection_data = NULL;

  g_type_init ();
  g_thread_init (NULL);
  g_test_init (&argc, &argv, NULL);

  introspection_data = g_dbus_node_info_new_for_xml (test_interface_introspection_xml, NULL);
  g_assert (introspection_data != NULL);
  test_interface_introspection_data = introspection_data->interfaces[0];

  test_guid = g_dbus_generate_guid ();

  /* all the tests rely on a shared main loop */
  loop = g_main_loop_new (NULL, FALSE);

  g_test_add_func ("/gdbus/peer-to-peer", test_peer);

  ret = g_test_run();

  g_main_loop_unref (loop);
  g_free (test_guid);
  g_dbus_node_info_unref (introspection_data);

  return ret;
}
