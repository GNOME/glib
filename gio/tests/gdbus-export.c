/* GLib testing framework examples and tests
 *
 * Copyright (C) 2008-2010 Red Hat, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: David Zeuthen <davidz@redhat.com>
 */

#include <gio/gio.h>
#include <unistd.h>
#include <string.h>

#include "gdbus-tests.h"
#include "gdbusprivate.h"

/* all tests rely on a shared mainloop */
static GMainLoop *loop = NULL;

static GDBusConnection *c = NULL;

/* ---------------------------------------------------------------------------------------------------- */
/* Test that we can export objects, the hierarchy is correct and the right handlers are invoked */
/* ---------------------------------------------------------------------------------------------------- */

static const GDBusArgInfo foo_method1_in_args =
{
  -1,
  "an_input_string",
  "s",
  NULL
};
static const GDBusArgInfo * const foo_method1_in_arg_pointers[] = {&foo_method1_in_args, NULL};

static const GDBusArgInfo foo_method1_out_args =
{
  -1,
  "an_output_string",
  "s",
  NULL
};
static const GDBusArgInfo * const foo_method1_out_arg_pointers[] = {&foo_method1_out_args, NULL};

static const GDBusMethodInfo foo_method_info_method1 =
{
  -1,
  "Method1",
  (GDBusArgInfo **) &foo_method1_in_arg_pointers,
  (GDBusArgInfo **) &foo_method1_out_arg_pointers,
  NULL
};
static const GDBusMethodInfo foo_method_info_method2 =
{
  -1,
  "Method2",
  NULL,
  NULL,
  NULL
};
static const GDBusMethodInfo * const foo_method_info_pointers[] = {&foo_method_info_method1, &foo_method_info_method2, NULL};

static const GDBusSignalInfo foo_signal_info =
{
  -1,
  "SignalAlpha",
  NULL,
  NULL
};
static const GDBusSignalInfo * const foo_signal_info_pointers[] = {&foo_signal_info, NULL};

static const GDBusPropertyInfo foo_property_info[] =
{
  {
    -1,
    "PropertyUno",
    "s",
    G_DBUS_PROPERTY_INFO_FLAGS_READABLE | G_DBUS_PROPERTY_INFO_FLAGS_WRITABLE,
    NULL
  },
  {
    -1,
    "NotWritable",
    "s",
    G_DBUS_PROPERTY_INFO_FLAGS_READABLE,
    NULL
  },
  {
    -1,
    "NotReadable",
    "s",
    G_DBUS_PROPERTY_INFO_FLAGS_WRITABLE,
    NULL
  }
};
static const GDBusPropertyInfo * const foo_property_info_pointers[] =
{
  &foo_property_info[0],
  &foo_property_info[1],
  &foo_property_info[2],
  NULL
};

static const GDBusInterfaceInfo foo_interface_info =
{
  -1,
  "org.example.Foo",
  (GDBusMethodInfo **) &foo_method_info_pointers,
  (GDBusSignalInfo **) &foo_signal_info_pointers,
  (GDBusPropertyInfo **)&foo_property_info_pointers,
  NULL,
};

/* Foo2 is just Foo without the properties */
static const GDBusInterfaceInfo foo2_interface_info =
{
  -1,
  "org.example.Foo2",
  (GDBusMethodInfo **) &foo_method_info_pointers,
  (GDBusSignalInfo **) &foo_signal_info_pointers,
  NULL,
  NULL
};

static void
foo_method_call (GDBusConnection       *connection,
                 const gchar           *sender,
                 const gchar           *object_path,
                 const gchar           *interface_name,
                 const gchar           *method_name,
                 GVariant              *parameters,
                 GDBusMethodInvocation *invocation,
                 gpointer               user_data)
{
  if (g_strcmp0 (method_name, "Method1") == 0)
    {
      const gchar *input;
      gchar *output;
      g_variant_get (parameters, "(&s)", &input);
      output = g_strdup_printf ("You passed the string '%s'. Jolly good!", input);
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", output));
      g_free (output);
    }
  else
    {
      g_dbus_method_invocation_return_dbus_error (invocation,
                                                  "org.example.SomeError",
                                                  "How do you like them apples, buddy!");
    }
}

static void
foo_method_call_with_closure (GDBusConnection       *connection,
                              const gchar           *sender,
                              const gchar           *object_path,
                              const gchar           *interface_name,
                              const gchar           *method_name,
                              GVariant              *parameters,
                              GDBusMethodInvocation *invocation,
                              gpointer               user_data)
{
  /* The call below takes ownership of the invocation but ownership is not
   * passed into the callback so get an additional reference here */
  g_object_ref (invocation);

  foo_method_call (connection, sender, object_path, interface_name, method_name, parameters, invocation, user_data);
}

static GVariant *
foo_get_property (GDBusConnection       *connection,
                  const gchar           *sender,
                  const gchar           *object_path,
                  const gchar           *interface_name,
                  const gchar           *property_name,
                  GError               **error,
                  gpointer               user_data)
{
  GVariant *ret;
  gchar *s;
  s = g_strdup_printf ("Property '%s' Is What It Is!", property_name);
  ret = g_variant_new_string (s);
  g_free (s);
  return ret;
}

static gboolean
foo_set_property (GDBusConnection       *connection,
                  const gchar           *sender,
                  const gchar           *object_path,
                  const gchar           *interface_name,
                  const gchar           *property_name,
                  GVariant              *value,
                  GError               **error,
                  gpointer               user_data)
{
  gchar *s;
  s = g_variant_print (value, TRUE);
  g_set_error (error,
               G_DBUS_ERROR,
               G_DBUS_ERROR_SPAWN_FILE_INVALID,
               "Returning some error instead of writing the value '%s' to the property '%s'",
               s, property_name);
  g_free (s);
  return FALSE;
}

static const GDBusInterfaceVTable foo_vtable =
{
  foo_method_call,
  foo_get_property,
  foo_set_property,
  { 0 },
};

/* -------------------- */

static const GDBusMethodInfo bar_method_info[] =
{
  {
    -1,
    "MethodA",
    NULL,
    NULL,
    NULL
  },
  {
    -1,
    "MethodB",
    NULL,
    NULL,
    NULL
  }
};
static const GDBusMethodInfo * const bar_method_info_pointers[] = {&bar_method_info[0], &bar_method_info[1], NULL};

static const GDBusSignalInfo bar_signal_info[] =
{
  {
    -1,
    "SignalMars",
    NULL,
    NULL
  }
};
static const GDBusSignalInfo * const bar_signal_info_pointers[] = {&bar_signal_info[0], NULL};

static const GDBusPropertyInfo bar_property_info[] =
{
  {
    -1,
    "PropertyDuo",
    "s",
    G_DBUS_PROPERTY_INFO_FLAGS_READABLE,
    NULL
  }
};
static const GDBusPropertyInfo * const bar_property_info_pointers[] = {&bar_property_info[0], NULL};

static const GDBusInterfaceInfo bar_interface_info =
{
  -1,
  "org.example.Bar",
  (GDBusMethodInfo **) bar_method_info_pointers,
  (GDBusSignalInfo **) bar_signal_info_pointers,
  (GDBusPropertyInfo **) bar_property_info_pointers,
  NULL,
};

/* -------------------- */

static const GDBusMethodInfo dyna_method_info[] =
{
  {
    -1,
    "DynaCyber",
    NULL,
    NULL,
    NULL
  }
};
static const GDBusMethodInfo * const dyna_method_info_pointers[] = {&dyna_method_info[0], NULL};

static const GDBusInterfaceInfo dyna_interface_info =
{
  -1,
  "org.example.Dyna",
  (GDBusMethodInfo **) dyna_method_info_pointers,
  NULL, /* no signals */
  NULL, /* no properties */
  NULL,
};

static void
dyna_cyber (GDBusConnection *connection,
            const gchar *sender,
            const gchar *object_path,
            const gchar *interface_name,
            const gchar *method_name,
            GVariant *parameters,
            GDBusMethodInvocation *invocation,
            gpointer user_data)
{
  GPtrArray *data = user_data;
  gchar *node_name;
  guint n;

  node_name = strrchr (object_path, '/') + 1;

  /* Add new node if it is not already known */
  for (n = 0; n < data->len ; n++)
    {
      if (g_strcmp0 (g_ptr_array_index (data, n), node_name) == 0)
        goto out;
    }
  g_ptr_array_add (data, g_strdup(node_name));

  out:
    g_dbus_method_invocation_return_value (invocation, NULL);
}

static const GDBusInterfaceVTable dyna_interface_vtable =
{
  dyna_cyber,
  NULL,
  NULL,
  { 0 }
};

/* ---------------------------------------------------------------------------------------------------- */

static void
introspect_callback (GDBusProxy   *proxy,
                     GAsyncResult *res,
                     gpointer      user_data)
{
  gchar **xml_data = user_data;
  GVariant *result;
  GError *error;

  error = NULL;
  result = g_dbus_proxy_call_finish (proxy,
                                              res,
                                              &error);
  g_assert_no_error (error);
  g_assert_nonnull (result);
  g_variant_get (result, "(s)", xml_data);
  g_variant_unref (result);

  g_main_loop_quit (loop);
}

static gint
compare_strings (gconstpointer a,
                 gconstpointer b)
{
  const gchar *sa = a;
  const gchar *sb = b;

  /* Array terminator must sort last */
  if (sa == NULL)
    return 1;
  if (sb == NULL)
    return -1;

  return strcmp (sa, sb);
}

static gchar **
get_nodes_at (GDBusConnection  *c,
              const gchar      *object_path)
{
  GError *error;
  GDBusProxy *proxy;
  gchar *xml_data;
  GPtrArray *p;
  GDBusNodeInfo *node_info;
  guint n;

  error = NULL;
  proxy = g_dbus_proxy_new_sync (c,
                                 G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                 G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                 NULL,
                                 g_dbus_connection_get_unique_name (c),
                                 object_path,
                                 DBUS_INTERFACE_INTROSPECTABLE,
                                 NULL,
                                 &error);
  g_assert_no_error (error);
  g_assert_nonnull (proxy);

  /* do this async to avoid libdbus-1 deadlocks */
  xml_data = NULL;
  g_dbus_proxy_call (proxy,
                     "Introspect",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     (GAsyncReadyCallback) introspect_callback,
                     &xml_data);
  g_main_loop_run (loop);
  g_assert_nonnull (xml_data);

  node_info = g_dbus_node_info_new_for_xml (xml_data, &error);
  g_assert_no_error (error);
  g_assert_nonnull (node_info);

  p = g_ptr_array_new ();
  for (n = 0; node_info->nodes != NULL && node_info->nodes[n] != NULL; n++)
    {
      const GDBusNodeInfo *sub_node_info = node_info->nodes[n];
      g_ptr_array_add (p, g_strdup (sub_node_info->path));
    }
  g_ptr_array_add (p, NULL);

  g_object_unref (proxy);
  g_free (xml_data);
  g_dbus_node_info_unref (node_info);

  /* Nodes are semantically unordered; sort array so tests can rely on order */
  g_ptr_array_sort_values (p, compare_strings);

  return (gchar **) g_ptr_array_free (p, FALSE);
}

static gboolean
has_interface (GDBusConnection *c,
               const gchar     *object_path,
               const gchar     *interface_name)
{
  GError *error;
  GDBusProxy *proxy;
  gchar *xml_data;
  GDBusNodeInfo *node_info;
  gboolean ret;

  error = NULL;
  proxy = g_dbus_proxy_new_sync (c,
                                 G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                 G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                 NULL,
                                 g_dbus_connection_get_unique_name (c),
                                 object_path,
                                 DBUS_INTERFACE_INTROSPECTABLE,
                                 NULL,
                                 &error);
  g_assert_no_error (error);
  g_assert_nonnull (proxy);

  /* do this async to avoid libdbus-1 deadlocks */
  xml_data = NULL;
  g_dbus_proxy_call (proxy,
                     "Introspect",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     (GAsyncReadyCallback) introspect_callback,
                     &xml_data);
  g_main_loop_run (loop);
  g_assert_nonnull (xml_data);

  node_info = g_dbus_node_info_new_for_xml (xml_data, &error);
  g_assert_no_error (error);
  g_assert_nonnull (node_info);

  ret = (g_dbus_node_info_lookup_interface (node_info, interface_name) != NULL);

  g_object_unref (proxy);
  g_free (xml_data);
  g_dbus_node_info_unref (node_info);

  return ret;
}

static guint
count_interfaces (GDBusConnection *c,
                  const gchar     *object_path)
{
  GError *error;
  GDBusProxy *proxy;
  gchar *xml_data;
  GDBusNodeInfo *node_info;
  guint ret;

  error = NULL;
  proxy = g_dbus_proxy_new_sync (c,
                                 G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                 G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                 NULL,
                                 g_dbus_connection_get_unique_name (c),
                                 object_path,
                                 DBUS_INTERFACE_INTROSPECTABLE,
                                 NULL,
                                 &error);
  g_assert_no_error (error);
  g_assert_nonnull (proxy);

  /* do this async to avoid libdbus-1 deadlocks */
  xml_data = NULL;
  g_dbus_proxy_call (proxy,
                     "Introspect",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     (GAsyncReadyCallback) introspect_callback,
                     &xml_data);
  g_main_loop_run (loop);
  g_assert_nonnull (xml_data);

  node_info = g_dbus_node_info_new_for_xml (xml_data, &error);
  g_assert_no_error (error);
  g_assert_nonnull (node_info);

  ret = 0;
  while (node_info->interfaces != NULL && node_info->interfaces[ret] != NULL)
    ret++;

  g_object_unref (proxy);
  g_free (xml_data);
  g_dbus_node_info_unref (node_info);

  return ret;
}

static void
dyna_create_callback (GDBusProxy   *proxy,
                      GAsyncResult  *res,
                      gpointer      user_data)
{
  GVariant *result;
  GError *error;

  error = NULL;
  result = g_dbus_proxy_call_finish (proxy,
                                     res,
                                     &error);
  g_assert_no_error (error);
  g_assert_nonnull (result);
  g_variant_unref (result);

  g_main_loop_quit (loop);
}

/* Dynamically create @object_name under /foo/dyna */
static void
dyna_create (GDBusConnection *c,
             const gchar     *object_name)
{
  GError *error;
  GDBusProxy *proxy;
  gchar *object_path;

  object_path = g_strconcat ("/foo/dyna/", object_name, NULL);

  error = NULL;
  proxy = g_dbus_proxy_new_sync (c,
                                 G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                 G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                 NULL,
                                 g_dbus_connection_get_unique_name (c),
                                 object_path,
                                 "org.example.Dyna",
                                 NULL,
                                 &error);
  g_assert_no_error (error);
  g_assert_nonnull (proxy);

  /* do this async to avoid libdbus-1 deadlocks */
  g_dbus_proxy_call (proxy,
                     "DynaCyber",
                     g_variant_new ("()"),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     (GAsyncReadyCallback) dyna_create_callback,
                     NULL);
  g_main_loop_run (loop);

  g_assert_no_error (error);

  g_object_unref (proxy);
  g_free (object_path);

  return;
}

typedef struct
{
  guint num_unregistered_calls;
  guint num_unregistered_subtree_calls;
  guint num_subtree_nodes;
} ObjectRegistrationData;

static void
on_object_unregistered (gpointer user_data)
{
  ObjectRegistrationData *data = user_data;

  data->num_unregistered_calls++;
}

static void
on_subtree_unregistered (gpointer user_data)
{
  ObjectRegistrationData *data = user_data;

  data->num_unregistered_subtree_calls++;
}

/* -------------------- */

static gchar **
subtree_enumerate (GDBusConnection       *connection,
                   const gchar           *sender,
                   const gchar           *object_path,
                   gpointer               user_data)
{
  ObjectRegistrationData *data = user_data;
  GPtrArray *p;
  gchar **nodes;
  guint n;

  p = g_ptr_array_new ();

  for (n = 0; n < data->num_subtree_nodes; n++)
    {
      g_ptr_array_add (p, g_strdup_printf ("vp%d", n));
      g_ptr_array_add (p, g_strdup_printf ("evp%d", n));
    }
  g_ptr_array_add (p, NULL);

  nodes = (gchar **) g_ptr_array_free (p, FALSE);

  return nodes;
}

/* Only allows certain objects, and aborts on unknowns */
static GDBusInterfaceInfo **
subtree_introspect (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *node,
                    gpointer               user_data)
{
  const GDBusInterfaceInfo *interfaces[2] = {
     NULL /* filled in below */, NULL
  };

  /* VPs implement the Foo interface, EVPs implement the Bar interface. The root
   * does not implement any interfaces
   */
  if (node == NULL)
    {
      return NULL;
    }
  else if (g_str_has_prefix (node, "vp"))
    {
      interfaces[0] = &foo_interface_info;
    }
  else if (g_str_has_prefix (node, "evp"))
    {
      interfaces[0] = &bar_interface_info;
    }
  else
    {
      g_assert_not_reached ();
    }

  return g_memdup2 (interfaces, 2 * sizeof (void *));
}

static const GDBusInterfaceVTable *
subtree_dispatch (GDBusConnection             *connection,
                  const gchar                 *sender,
                  const gchar                 *object_path,
                  const gchar                 *interface_name,
                  const gchar                 *node,
                  gpointer                    *out_user_data,
                  gpointer                     user_data)
{
  if (g_strcmp0 (interface_name, "org.example.Foo") == 0)
    return &foo_vtable;
  else
    return NULL;
}

static const GDBusSubtreeVTable subtree_vtable =
{
  subtree_enumerate,
  subtree_introspect,
  subtree_dispatch,
  { 0 }
};

/* -------------------- */

static gchar **
dynamic_subtree_enumerate (GDBusConnection       *connection,
                           const gchar           *sender,
                           const gchar           *object_path,
                           gpointer               user_data)
{
  GPtrArray *data = user_data;
  gchar **nodes = g_new (gchar*, data->len + 1);
  guint n;

  for (n = 0; n < data->len; n++)
    {
      nodes[n] = g_strdup (g_ptr_array_index (data, n));
    }
  nodes[data->len] = NULL;

  return nodes;
}

/* Allow all objects to be introspected */
static GDBusInterfaceInfo **
dynamic_subtree_introspect (GDBusConnection       *connection,
                            const gchar           *sender,
                            const gchar           *object_path,
                            const gchar           *node,
                            gpointer               user_data)
{
  const GDBusInterfaceInfo *interfaces[2] = { &dyna_interface_info, NULL };

  return g_memdup2 (interfaces, 2 * sizeof (void *));
}

static const GDBusInterfaceVTable *
dynamic_subtree_dispatch (GDBusConnection             *connection,
                          const gchar                 *sender,
                          const gchar                 *object_path,
                          const gchar                 *interface_name,
                          const gchar                 *node,
                          gpointer                    *out_user_data,
                          gpointer                     user_data)
{
  *out_user_data = user_data;
  return &dyna_interface_vtable;
}

static const GDBusSubtreeVTable dynamic_subtree_vtable =
{
  dynamic_subtree_enumerate,
  dynamic_subtree_introspect,
  dynamic_subtree_dispatch,
  { 0 }
};

/* -------------------- */

typedef struct
{
  const gchar *object_path;
  gboolean check_remote_errors;
} TestDispatchThreadFuncArgs;

static gpointer
test_dispatch_thread_func (gpointer user_data)
{
  TestDispatchThreadFuncArgs *args = user_data;
  const gchar *object_path = args->object_path;
  GDBusProxy *foo_proxy;
  GVariant *value;
  GVariant *inner;
  GError *error;
  gchar *s;
  const gchar *value_str;

  foo_proxy = g_dbus_proxy_new_sync (c,
                                     G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS |
                                     G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                     NULL,
                                     g_dbus_connection_get_unique_name (c),
                                     object_path,
                                     "org.example.Foo",
                                     NULL,
                                     &error);
  g_assert_nonnull (foo_proxy);

  /* generic interfaces */
  error = NULL;
  value = g_dbus_proxy_call_sync (foo_proxy,
                                  DBUS_INTERFACE_PEER ".Ping",
                                  NULL,
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1,
                                  NULL,
                                  &error);
  g_assert_no_error (error);
  g_assert_nonnull (value);
  g_variant_unref (value);

  /* user methods */
  error = NULL;
  value = g_dbus_proxy_call_sync (foo_proxy,
                                  "Method1",
                                  g_variant_new ("(s)", "winwinwin"),
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1,
                                  NULL,
                                  &error);
  g_assert_no_error (error);
  g_assert_nonnull (value);
  g_assert_true (g_variant_is_of_type (value, G_VARIANT_TYPE ("(s)")));
  g_variant_get (value, "(&s)", &value_str);
  g_assert_cmpstr (value_str, ==, "You passed the string 'winwinwin'. Jolly good!");
  g_variant_unref (value);

  error = NULL;
  value = g_dbus_proxy_call_sync (foo_proxy,
                                  "Method2",
                                  NULL,
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1,
                                  NULL,
                                  &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_DBUS_ERROR);
  g_assert_cmpstr (error->message, ==, "GDBus.Error:org.example.SomeError: How do you like them apples, buddy!");
  g_error_free (error);
  g_assert_null (value);

  error = NULL;
  value = g_dbus_proxy_call_sync (foo_proxy,
                                  "Method2",
                                  g_variant_new ("(s)", "failfailfail"),
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1,
                                  NULL,
                                  &error);
  g_assert_error (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS);
  g_assert_cmpstr (error->message, ==, "GDBus.Error:" DBUS_ERROR_INVALID_ARGS ": Type of message, “(s)”, does not match expected type “()”");
  g_error_free (error);
  g_assert_null (value);

  error = NULL;
  value = g_dbus_proxy_call_sync (foo_proxy,
                                  "NonExistantMethod",
                                  NULL,
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1,
                                  NULL,
                                  &error);
  g_assert_error (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD);
  g_assert_cmpstr (error->message, ==, "GDBus.Error:" DBUS_ERROR_UNKNOWN_METHOD ": No such method “NonExistantMethod”");
  g_error_free (error);
  g_assert_null (value);

  error = NULL;
  value = g_dbus_proxy_call_sync (foo_proxy,
                                  "org.example.FooXYZ.NonExistant",
                                  NULL,
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1,
                                  NULL,
                                  &error);
  g_assert_error (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD);
  g_error_free (error);
  g_assert_null (value);

  /* user properties */
  error = NULL;
  value = g_dbus_proxy_call_sync (foo_proxy,
                                  DBUS_INTERFACE_PROPERTIES ".Get",
                                  g_variant_new ("(ss)",
                                                 "org.example.Foo",
                                                 "PropertyUno"),
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1,
                                  NULL,
                                  &error);
  g_assert_no_error (error);
  g_assert_nonnull (value);
  g_assert_true (g_variant_is_of_type (value, G_VARIANT_TYPE ("(v)")));
  g_variant_get (value, "(v)", &inner);
  g_assert_true (g_variant_is_of_type (inner, G_VARIANT_TYPE_STRING));
  g_assert_cmpstr (g_variant_get_string (inner, NULL), ==, "Property 'PropertyUno' Is What It Is!");
  g_variant_unref (value);
  g_variant_unref (inner);

  error = NULL;
  value = g_dbus_proxy_call_sync (foo_proxy,
                                  DBUS_INTERFACE_PROPERTIES ".Get",
                                  g_variant_new ("(ss)",
                                                 "org.example.Foo",
                                                 "ThisDoesntExist"),
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1,
                                  NULL,
                                  &error);
  g_assert_null (value);
  g_assert_error (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS);
  g_assert_cmpstr (error->message, ==, "GDBus.Error:" DBUS_ERROR_INVALID_ARGS ": No such property “ThisDoesntExist”");
  g_error_free (error);

  error = NULL;
  value = g_dbus_proxy_call_sync (foo_proxy,
                                  DBUS_INTERFACE_PROPERTIES ".Get",
                                  g_variant_new ("(ss)",
                                                 "org.example.Foo",
                                                 "NotReadable"),
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1,
                                  NULL,
                                  &error);
  g_assert_null (value);
  g_assert_error (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS);
  g_assert_cmpstr (error->message, ==, "GDBus.Error:" DBUS_ERROR_INVALID_ARGS ": Property “NotReadable” is not readable");
  g_error_free (error);

  error = NULL;
  value = g_dbus_proxy_call_sync (foo_proxy,
                                  DBUS_INTERFACE_PROPERTIES ".Set",
                                  g_variant_new ("(ssv)",
                                                 "org.example.Foo",
                                                 "NotReadable",
                                                 g_variant_new_string ("But Writable you are!")),
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1,
                                  NULL,
                                  &error);
  g_assert_null (value);
  if (args->check_remote_errors)
    {
      /* _with_closures variant doesn't support customizing error data. */
      g_assert_error (error, G_DBUS_ERROR, G_DBUS_ERROR_SPAWN_FILE_INVALID);
      g_assert_cmpstr (error->message, ==, "GDBus.Error:org.freedesktop.DBus.Error.Spawn.FileInvalid: Returning some error instead of writing the value ''But Writable you are!'' to the property 'NotReadable'");
    }
  g_assert_true (error != NULL && error->domain == G_DBUS_ERROR);
  g_error_free (error);

  error = NULL;
  value = g_dbus_proxy_call_sync (foo_proxy,
                                  DBUS_INTERFACE_PROPERTIES ".Set",
                                  g_variant_new ("(ssv)",
                                                 "org.example.Foo",
                                                 "NotWritable",
                                                 g_variant_new_uint32 (42)),
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1,
                                  NULL,
                                  &error);
  g_assert_null (value);
  g_assert_error (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS);
  g_assert_cmpstr (error->message, ==, "GDBus.Error:" DBUS_ERROR_INVALID_ARGS ": Property “NotWritable” is not writable");
  g_error_free (error);

  error = NULL;
  value = g_dbus_proxy_call_sync (foo_proxy,
                                  DBUS_INTERFACE_PROPERTIES ".GetAll",
                                  g_variant_new ("(s)",
                                                 "org.example.Foo"),
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1,
                                  NULL,
                                  &error);
  g_assert_no_error (error);
  g_assert_nonnull (value);
  g_assert_true (g_variant_is_of_type (value, G_VARIANT_TYPE ("(a{sv})")));
  s = g_variant_print (value, TRUE);
  g_assert_cmpstr (s, ==, "({'PropertyUno': <\"Property 'PropertyUno' Is What It Is!\">, 'NotWritable': <\"Property 'NotWritable' Is What It Is!\">},)");
  g_free (s);
  g_variant_unref (value);

  g_object_unref (foo_proxy);

  g_main_loop_quit (loop);
  return NULL;
}

static void
test_dispatch (const gchar *object_path,
               gboolean     check_remote_errors)
{
  GThread *thread;
  
  TestDispatchThreadFuncArgs args = {object_path, check_remote_errors};

  /* run this in a thread to avoid deadlocks */
  thread = g_thread_new ("test_dispatch",
                         test_dispatch_thread_func,
                         (gpointer) &args);
  g_main_loop_run (loop);
  g_thread_join (thread);
}

static void
test_object_registration (void)
{
  GError *error;
  ObjectRegistrationData data;
  GPtrArray *dyna_data;
  gchar **nodes;
  guint boss_foo_reg_id;
  guint boss_bar_reg_id;
  guint worker1_foo_reg_id;
  guint worker1p1_foo_reg_id;
  guint worker2_bar_reg_id;
  guint intern1_foo_reg_id;
  guint intern2_bar_reg_id;
  guint intern2_foo_reg_id;
  guint intern3_bar_reg_id;
  guint registration_id;
  guint subtree_registration_id;
  guint non_subtree_object_path_foo_reg_id;
  guint non_subtree_object_path_bar_reg_id;
  guint dyna_subtree_registration_id;
  guint num_successful_registrations;
  guint num_failed_registrations;

  data.num_unregistered_calls = 0;
  data.num_unregistered_subtree_calls = 0;
  data.num_subtree_nodes = 0;

  num_successful_registrations = 0;
  num_failed_registrations = 0;

  error = NULL;
  c = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (c);

  registration_id = g_dbus_connection_register_object (c,
                                                       "/foo/boss",
                                                       (GDBusInterfaceInfo *) &foo_interface_info,
                                                       &foo_vtable,
                                                       &data,
                                                       on_object_unregistered,
                                                       &error);
  g_assert_no_error (error);
  g_assert_cmpuint (registration_id, >, 0);
  boss_foo_reg_id = registration_id;
  num_successful_registrations++;

  registration_id = g_dbus_connection_register_object (c,
                                                       "/foo/boss",
                                                       (GDBusInterfaceInfo *) &bar_interface_info,
                                                       NULL,
                                                       &data,
                                                       on_object_unregistered,
                                                       &error);
  g_assert_no_error (error);
  g_assert_cmpuint (registration_id, >, 0);
  boss_bar_reg_id = registration_id;
  num_successful_registrations++;

  registration_id = g_dbus_connection_register_object (c,
                                                       "/foo/boss/worker1",
                                                       (GDBusInterfaceInfo *) &foo_interface_info,
                                                       NULL,
                                                       &data,
                                                       on_object_unregistered,
                                                       &error);
  g_assert_no_error (error);
  g_assert_cmpuint (registration_id, >, 0);
  worker1_foo_reg_id = registration_id;
  num_successful_registrations++;

  registration_id = g_dbus_connection_register_object (c,
                                                       "/foo/boss/worker1p1",
                                                       (GDBusInterfaceInfo *) &foo_interface_info,
                                                       NULL,
                                                       &data,
                                                       on_object_unregistered,
                                                       &error);
  g_assert_no_error (error);
  g_assert_cmpuint (registration_id, >, 0);
  worker1p1_foo_reg_id = registration_id;
  num_successful_registrations++;

  registration_id = g_dbus_connection_register_object (c,
                                                       "/foo/boss/worker2",
                                                       (GDBusInterfaceInfo *) &bar_interface_info,
                                                       NULL,
                                                       &data,
                                                       on_object_unregistered,
                                                       &error);
  g_assert_no_error (error);
  g_assert_cmpuint (registration_id, >, 0);
  worker2_bar_reg_id = registration_id;
  num_successful_registrations++;

  registration_id = g_dbus_connection_register_object (c,
                                                       "/foo/boss/interns/intern1",
                                                       (GDBusInterfaceInfo *) &foo_interface_info,
                                                       NULL,
                                                       &data,
                                                       on_object_unregistered,
                                                       &error);
  g_assert_no_error (error);
  g_assert_cmpuint (registration_id, >, 0);
  intern1_foo_reg_id = registration_id;
  num_successful_registrations++;

  /* ... and try again at another path */
  registration_id = g_dbus_connection_register_object (c,
                                                       "/foo/boss/interns/intern2",
                                                       (GDBusInterfaceInfo *) &bar_interface_info,
                                                       NULL,
                                                       &data,
                                                       on_object_unregistered,
                                                       &error);
  g_assert_no_error (error);
  g_assert_cmpuint (registration_id, >, 0);
  intern2_bar_reg_id = registration_id;
  num_successful_registrations++;

  /* register at the same path/interface - this should fail and result in an
   * immediate unregistration (so the user_data isn’t leaked) */
  registration_id = g_dbus_connection_register_object (c,
                                                       "/foo/boss/interns/intern2",
                                                       (GDBusInterfaceInfo *) &bar_interface_info,
                                                       NULL,
                                                       &data,
                                                       on_object_unregistered,
                                                       &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_EXISTS);
  g_assert_false (g_dbus_error_is_remote_error (error));
  g_error_free (error);
  error = NULL;
  g_assert_cmpuint (registration_id, ==, 0);
  g_assert_cmpint (data.num_unregistered_calls, ==, 1);
  num_failed_registrations++;

  /* register at different interface - shouldn't fail */
  registration_id = g_dbus_connection_register_object (c,
                                                       "/foo/boss/interns/intern2",
                                                       (GDBusInterfaceInfo *) &foo_interface_info,
                                                       NULL,
                                                       &data,
                                                       on_object_unregistered,
                                                       &error);
  g_assert_no_error (error);
  g_assert_cmpuint (registration_id, >, 0);
  intern2_foo_reg_id = registration_id;
  num_successful_registrations++;

  /* unregister it via the id */
  g_assert_true (g_dbus_connection_unregister_object (c, registration_id));
  g_main_context_iteration (NULL, FALSE);
  g_assert_cmpint (data.num_unregistered_calls, ==, 2);
  intern2_foo_reg_id = 0;

  /* register it back */
  registration_id = g_dbus_connection_register_object (c,
                                                       "/foo/boss/interns/intern2",
                                                       (GDBusInterfaceInfo *) &foo_interface_info,
                                                       NULL,
                                                       &data,
                                                       on_object_unregistered,
                                                       &error);
  g_assert_no_error (error);
  g_assert_cmpuint (registration_id, >, 0);
  intern2_foo_reg_id = registration_id;
  num_successful_registrations++;

  registration_id = g_dbus_connection_register_object (c,
                                                       "/foo/boss/interns/intern3",
                                                       (GDBusInterfaceInfo *) &bar_interface_info,
                                                       NULL,
                                                       &data,
                                                       on_object_unregistered,
                                                       &error);
  g_assert_no_error (error);
  g_assert_cmpuint (registration_id, >, 0);
  intern3_bar_reg_id = registration_id;
  num_successful_registrations++;

  /* now register a whole subtree at /foo/boss/executives */
  subtree_registration_id = g_dbus_connection_register_subtree (c,
                                                                "/foo/boss/executives",
                                                                &subtree_vtable,
                                                                G_DBUS_SUBTREE_FLAGS_NONE,
                                                                &data,
                                                                on_subtree_unregistered,
                                                                &error);
  g_assert_no_error (error);
  g_assert_cmpuint (subtree_registration_id, >, 0);
  /* try registering it again.. this should fail */
  registration_id = g_dbus_connection_register_subtree (c,
                                                        "/foo/boss/executives",
                                                        &subtree_vtable,
                                                        G_DBUS_SUBTREE_FLAGS_NONE,
                                                        &data,
                                                        on_subtree_unregistered,
                                                        &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_EXISTS);
  g_assert_false (g_dbus_error_is_remote_error (error));
  g_error_free (error);
  error = NULL;
  g_assert_cmpuint (registration_id, ==, 0);
  g_assert_cmpint (data.num_unregistered_subtree_calls, ==, 1);

  /* unregister it, then register it again */
  g_assert_true (g_dbus_connection_unregister_subtree (c, subtree_registration_id));
  g_main_context_iteration (NULL, FALSE);
  g_assert_cmpint (data.num_unregistered_subtree_calls, ==, 2);
  subtree_registration_id = g_dbus_connection_register_subtree (c,
                                                                "/foo/boss/executives",
                                                                &subtree_vtable,
                                                                G_DBUS_SUBTREE_FLAGS_NONE,
                                                                &data,
                                                                on_subtree_unregistered,
                                                                &error);
  g_assert_no_error (error);
  g_assert_cmpuint (subtree_registration_id, >, 0);

  /* try to register something under /foo/boss/executives - this should work
   * because registered subtrees and registered objects can coexist.
   *
   * Make the exported object implement *two* interfaces so we can check
   * that the right introspection handler is invoked.
   */
  registration_id = g_dbus_connection_register_object (c,
                                                       "/foo/boss/executives/non_subtree_object",
                                                       (GDBusInterfaceInfo *) &bar_interface_info,
                                                       NULL,
                                                       &data,
                                                       on_object_unregistered,
                                                       &error);
  g_assert_no_error (error);
  g_assert_cmpuint (registration_id, >, 0);
  non_subtree_object_path_bar_reg_id = registration_id;
  num_successful_registrations++;
  registration_id = g_dbus_connection_register_object (c,
                                                       "/foo/boss/executives/non_subtree_object",
                                                       (GDBusInterfaceInfo *) &foo_interface_info,
                                                       NULL,
                                                       &data,
                                                       on_object_unregistered,
                                                       &error);
  g_assert_no_error (error);
  g_assert_cmpuint (registration_id, >, 0);
  non_subtree_object_path_foo_reg_id = registration_id;
  num_successful_registrations++;

  /* now register a dynamic subtree, spawning objects as they are called */
  dyna_data = g_ptr_array_new_with_free_func (g_free);
  dyna_subtree_registration_id = g_dbus_connection_register_subtree (c,
                                                                     "/foo/dyna",
                                                                     &dynamic_subtree_vtable,
                                                                     G_DBUS_SUBTREE_FLAGS_DISPATCH_TO_UNENUMERATED_NODES,
                                                                     dyna_data,
                                                                     (GDestroyNotify)g_ptr_array_unref,
                                                                     &error);
  g_assert_no_error (error);
  g_assert_cmpuint (dyna_subtree_registration_id, >, 0);

  /* First assert that we have no nodes in the dynamic subtree */
  nodes = get_nodes_at (c, "/foo/dyna");
  g_assert_nonnull (nodes);
  g_assert_cmpint (g_strv_length (nodes), ==, 0);
  g_strfreev (nodes);
  g_assert_cmpint (count_interfaces (c, "/foo/dyna"), ==, 4);

  /* Install three nodes in the dynamic subtree via the dyna_data backdoor and
   * assert that they show up correctly in the introspection data */
  g_ptr_array_add (dyna_data, g_strdup ("lol"));
  g_ptr_array_add (dyna_data, g_strdup ("cat"));
  g_ptr_array_add (dyna_data, g_strdup ("cheezburger"));
  nodes = get_nodes_at (c, "/foo/dyna");
  g_assert_nonnull (nodes);
  g_assert_cmpint (g_strv_length (nodes), ==, 3);
  g_assert_cmpstr (nodes[0], ==, "cat");
  g_assert_cmpstr (nodes[1], ==, "cheezburger");
  g_assert_cmpstr (nodes[2], ==, "lol");
  g_strfreev (nodes);
  g_assert_cmpint (count_interfaces (c, "/foo/dyna/lol"), ==, 4);
  g_assert_cmpint (count_interfaces (c, "/foo/dyna/cat"), ==, 4);
  g_assert_cmpint (count_interfaces (c, "/foo/dyna/cheezburger"), ==, 4);

  /* Call a non-existing object path and assert that it has been created */
  dyna_create (c, "dynamicallycreated");
  nodes = get_nodes_at (c, "/foo/dyna");
  g_assert_nonnull (nodes);
  g_assert_cmpint (g_strv_length (nodes), ==, 4);
  g_assert_cmpstr (nodes[0], ==, "cat");
  g_assert_cmpstr (nodes[1], ==, "cheezburger");
  g_assert_cmpstr (nodes[2], ==, "dynamicallycreated");
  g_assert_cmpstr (nodes[3], ==, "lol");
  g_strfreev (nodes);
  g_assert_cmpint (count_interfaces (c, "/foo/dyna/dynamicallycreated"), ==, 4);

  /* now check that the object hierarchy is properly generated... yes, it's a bit
   * perverse that we round-trip to the bus to introspect ourselves ;-)
   */
  nodes = get_nodes_at (c, "/");
  g_assert_nonnull (nodes);
  g_assert_cmpint (g_strv_length (nodes), ==, 1);
  g_assert_cmpstr (nodes[0], ==, "foo");
  g_strfreev (nodes);
  g_assert_cmpint (count_interfaces (c, "/"), ==, 0);

  nodes = get_nodes_at (c, "/foo");
  g_assert_nonnull (nodes);
  g_assert_cmpint (g_strv_length (nodes), ==, 2);
  g_assert_cmpstr (nodes[0], ==, "boss");
  g_assert_cmpstr (nodes[1], ==, "dyna");
  g_strfreev (nodes);
  g_assert_cmpint (count_interfaces (c, "/foo"), ==, 0);

  nodes = get_nodes_at (c, "/foo/boss");
  g_assert_nonnull (nodes);
  g_assert_cmpint (g_strv_length (nodes), ==, 5);
  g_assert_true (g_strv_contains ((const gchar* const *) nodes, "worker1"));
  g_assert_true (g_strv_contains ((const gchar* const *) nodes, "worker1p1"));
  g_assert_true (g_strv_contains ((const gchar* const *) nodes, "worker2"));
  g_assert_true (g_strv_contains ((const gchar* const *) nodes, "interns"));
  g_assert_true (g_strv_contains ((const gchar* const *) nodes, "executives"));
  g_strfreev (nodes);
  /* any registered object always implement org.freedesktop.DBus.[Peer,Introspectable,Properties] */
  g_assert_cmpint (count_interfaces (c, "/foo/boss"), ==, 5);
  g_assert_true (has_interface (c, "/foo/boss", foo_interface_info.name));
  g_assert_true (has_interface (c, "/foo/boss", bar_interface_info.name));

  /* check subtree nodes - we should have only non_subtree_object in /foo/boss/executives
   * because data.num_subtree_nodes is 0
   */
  nodes = get_nodes_at (c, "/foo/boss/executives");
  g_assert_nonnull (nodes);
  g_assert_true (g_strv_contains ((const gchar* const *) nodes, "non_subtree_object"));
  g_assert_cmpint (g_strv_length (nodes), ==, 1);
  g_strfreev (nodes);
  g_assert_cmpint (count_interfaces (c, "/foo/boss/executives"), ==, 0);

  /* now change data.num_subtree_nodes and check */
  data.num_subtree_nodes = 2;
  nodes = get_nodes_at (c, "/foo/boss/executives");
  g_assert_nonnull (nodes);
  g_assert_cmpint (g_strv_length (nodes), ==, 5);
  g_assert_true (g_strv_contains ((const gchar* const *) nodes, "non_subtree_object"));
  g_assert_true (g_strv_contains ((const gchar* const *) nodes, "vp0"));
  g_assert_true (g_strv_contains ((const gchar* const *) nodes, "vp1"));
  g_assert_true (g_strv_contains ((const gchar* const *) nodes, "evp0"));
  g_assert_true (g_strv_contains ((const gchar* const *) nodes, "evp1"));
  /* check that /foo/boss/executives/non_subtree_object is not handled by the
   * subtree handlers - we can do this because objects from subtree handlers
   * has exactly one interface and non_subtree_object has two
   */
  g_assert_cmpint (count_interfaces (c, "/foo/boss/executives/non_subtree_object"), ==, 5);
  g_assert_true (has_interface (c, "/foo/boss/executives/non_subtree_object", foo_interface_info.name));
  g_assert_true (has_interface (c, "/foo/boss/executives/non_subtree_object", bar_interface_info.name));
  /* check that the vp and evp objects are handled by the subtree handlers */
  g_assert_cmpint (count_interfaces (c, "/foo/boss/executives/vp0"), ==, 4);
  g_assert_cmpint (count_interfaces (c, "/foo/boss/executives/vp1"), ==, 4);
  g_assert_cmpint (count_interfaces (c, "/foo/boss/executives/evp0"), ==, 4);
  g_assert_cmpint (count_interfaces (c, "/foo/boss/executives/evp1"), ==, 4);
  g_assert_true (has_interface (c, "/foo/boss/executives/vp0", foo_interface_info.name));
  g_assert_true (has_interface (c, "/foo/boss/executives/vp1", foo_interface_info.name));
  g_assert_true (has_interface (c, "/foo/boss/executives/evp0", bar_interface_info.name));
  g_assert_true (has_interface (c, "/foo/boss/executives/evp1", bar_interface_info.name));
  g_strfreev (nodes);
  data.num_subtree_nodes = 3;
  nodes = get_nodes_at (c, "/foo/boss/executives");
  g_assert_nonnull (nodes);
  g_assert_cmpint (g_strv_length (nodes), ==, 7);
  g_assert_true (g_strv_contains ((const gchar* const *) nodes, "non_subtree_object"));
  g_assert_true (g_strv_contains ((const gchar* const *) nodes, "vp0"));
  g_assert_true (g_strv_contains ((const gchar* const *) nodes, "vp1"));
  g_assert_true (g_strv_contains ((const gchar* const *) nodes, "vp2"));
  g_assert_true (g_strv_contains ((const gchar* const *) nodes, "evp0"));
  g_assert_true (g_strv_contains ((const gchar* const *) nodes, "evp1"));
  g_assert_true (g_strv_contains ((const gchar* const *) nodes, "evp2"));
  g_strfreev (nodes);

  /* This is to check that a bug (rather, class of bugs) in gdbusconnection.c's
   *
   *  g_dbus_connection_list_registered_unlocked()
   *
   * where /foo/boss/worker1 reported a child '1', is now fixed.
   */
  nodes = get_nodes_at (c, "/foo/boss/worker1");
  g_assert_nonnull (nodes);
  g_assert_cmpint (g_strv_length (nodes), ==, 0);
  g_strfreev (nodes);

  /* check that calls are properly dispatched to the functions in foo_vtable for objects
   * implementing the org.example.Foo interface
   *
   * We do this for both a regular registered object (/foo/boss) and also for an object
   * registered through the subtree mechanism.
   */
  test_dispatch ("/foo/boss", TRUE);
  test_dispatch ("/foo/boss/executives/vp0", TRUE);

  /* To prevent from exiting and attaching a D-Bus tool like D-Feet; uncomment: */
#if 0
  g_debug ("Point D-feet or other tool at: %s", g_test_dbus_get_temporary_address());
  g_main_loop_run (loop);
#endif

  /* check that unregistering the subtree handler works */
  g_assert_cmpint (data.num_unregistered_subtree_calls, ==, 2);
  g_assert_true (g_dbus_connection_unregister_subtree (c, subtree_registration_id));
  g_main_context_iteration (NULL, FALSE);
  g_assert_cmpint (data.num_unregistered_subtree_calls, ==, 3);
  nodes = get_nodes_at (c, "/foo/boss/executives");
  g_assert_nonnull (nodes);
  g_assert_cmpint (g_strv_length (nodes), ==, 1);
  g_assert_true (g_strv_contains ((const gchar* const *) nodes, "non_subtree_object"));
  g_strfreev (nodes);

  g_assert_true (g_dbus_connection_unregister_object (c, boss_foo_reg_id));
  g_assert_true (g_dbus_connection_unregister_object (c, boss_bar_reg_id));
  g_assert_true (g_dbus_connection_unregister_object (c, worker1_foo_reg_id));
  g_assert_true (g_dbus_connection_unregister_object (c, worker1p1_foo_reg_id));
  g_assert_true (g_dbus_connection_unregister_object (c, worker2_bar_reg_id));
  g_assert_true (g_dbus_connection_unregister_object (c, intern1_foo_reg_id));
  g_assert_true (g_dbus_connection_unregister_object (c, intern2_bar_reg_id));
  g_assert_true (g_dbus_connection_unregister_object (c, intern2_foo_reg_id));
  g_assert_true (g_dbus_connection_unregister_object (c, intern3_bar_reg_id));
  g_assert_true (g_dbus_connection_unregister_object (c, non_subtree_object_path_bar_reg_id));
  g_assert_true (g_dbus_connection_unregister_object (c, non_subtree_object_path_foo_reg_id));

  g_main_context_iteration (NULL, FALSE);
  g_assert_cmpint (data.num_unregistered_calls, ==, num_successful_registrations + num_failed_registrations);

  /* check that we no longer export any objects - TODO: it looks like there's a bug in
   * libdbus-1 here: libdbus still reports the '/foo' object; so disable the test for now
   */
#if 0
  nodes = get_nodes_at (c, "/");
  g_assert_nonnull (nodes);
  g_assert_cmpint (g_strv_length (nodes), ==, 0);
  g_strfreev (nodes);
#endif

  g_object_unref (c);
}

static void
test_object_registration_with_closures (gconstpointer test_data)
{
  gboolean use_new_api = GPOINTER_TO_INT (test_data);
  GError *error;
  guint registration_id;

  error = NULL;
  c = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (c);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS

  if (use_new_api)
    registration_id = g_dbus_connection_register_object_with_closures2 (c,
                                                                        "/foo/boss",
                                                                        (GDBusInterfaceInfo *) &foo_interface_info,
                                                                        g_cclosure_new (G_CALLBACK (foo_method_call_with_closure), NULL, NULL),
                                                                        g_cclosure_new (G_CALLBACK (foo_get_property), NULL, NULL),
                                                                        g_cclosure_new (G_CALLBACK (foo_set_property), NULL, NULL),
                                                                        &error);
  else
    registration_id = g_dbus_connection_register_object_with_closures (c,
                                                                       "/foo/boss",
                                                                       (GDBusInterfaceInfo *) &foo_interface_info,
                                                                       g_cclosure_new (G_CALLBACK (foo_method_call), NULL, NULL),
                                                                       g_cclosure_new (G_CALLBACK (foo_get_property), NULL, NULL),
                                                                       g_cclosure_new (G_CALLBACK (foo_set_property), NULL, NULL),
                                                                       &error);

  G_GNUC_END_IGNORE_DEPRECATIONS

  g_assert_no_error (error);
  g_assert_cmpuint (registration_id, >, 0);

  test_dispatch ("/foo/boss", FALSE);

  g_assert_true (g_dbus_connection_unregister_object (c, registration_id));

  g_object_unref (c);
}

static const GDBusInterfaceInfo test_interface_info1 =
{
  -1,
  "org.example.Foo",
  (GDBusMethodInfo **) NULL,
  (GDBusSignalInfo **) NULL,
  (GDBusPropertyInfo **) NULL,
  NULL,
};

static const GDBusInterfaceInfo test_interface_info2 =
{
  -1,
  DBUS_INTERFACE_PROPERTIES,
  (GDBusMethodInfo **) NULL,
  (GDBusSignalInfo **) NULL,
  (GDBusPropertyInfo **) NULL,
  NULL,
};

static void
check_interfaces (GDBusConnection  *c,
                  const gchar      *object_path,
                  const gchar     **interfaces)
{
  GError *error;
  GDBusProxy *proxy;
  gchar *xml_data;
  GDBusNodeInfo *node_info;
  gint i, j;

  error = NULL;
  proxy = g_dbus_proxy_new_sync (c,
                                 G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                 G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                 NULL,
                                 g_dbus_connection_get_unique_name (c),
                                 object_path,
                                 DBUS_INTERFACE_INTROSPECTABLE,
                                 NULL,
                                 &error);
  g_assert_no_error (error);
  g_assert_nonnull (proxy);

  /* do this async to avoid libdbus-1 deadlocks */
  xml_data = NULL;
  g_dbus_proxy_call (proxy,
                     "Introspect",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     (GAsyncReadyCallback) introspect_callback,
                     &xml_data);
  g_main_loop_run (loop);
  g_assert_nonnull (xml_data);

  node_info = g_dbus_node_info_new_for_xml (xml_data, &error);
  g_assert_no_error (error);
  g_assert_nonnull (node_info);

  g_assert_nonnull (node_info->interfaces);
  for (i = 0; node_info->interfaces[i]; i++) ;
#if 0
  if (g_strv_length ((gchar**)interfaces) != i - 1)
    {
      g_printerr ("expected ");
      for (i = 0; interfaces[i]; i++)
        g_printerr ("%s ", interfaces[i]);
      g_printerr ("\ngot ");
      for (i = 0; node_info->interfaces[i]; i++)
        g_printerr ("%s ", node_info->interfaces[i]->name);
      g_printerr ("\n");
    }
#endif
  g_assert_cmpint (g_strv_length ((gchar**)interfaces), ==, i - 1);

  for (i = 0; interfaces[i]; i++)
    {
      for (j = 0; node_info->interfaces[j]; j++)
        {
          if (strcmp (interfaces[i], node_info->interfaces[j]->name) == 0)
            goto found;
        }

      g_assert_not_reached ();

 found: ;
    }

  g_object_unref (proxy);
  g_free (xml_data);
  g_dbus_node_info_unref (node_info);
}

static void
test_registered_interfaces (void)
{
  GError *error;
  guint id1, id2;
  const gchar *interfaces[] = {
    "org.example.Foo",
    DBUS_INTERFACE_PROPERTIES,
    DBUS_INTERFACE_INTROSPECTABLE,
    NULL,
  };

  error = NULL;
  c = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (c);

  id1 = g_dbus_connection_register_object (c,
                                           "/test",
                                           (GDBusInterfaceInfo *) &test_interface_info1,
                                           NULL,
                                           NULL,
                                           NULL,
                                           &error);
  g_assert_no_error (error);
  g_assert_cmpuint (id1, >, 0);
  id2 = g_dbus_connection_register_object (c,
                                           "/test",
                                           (GDBusInterfaceInfo *) &test_interface_info2,
                                           NULL,
                                           NULL,
                                           NULL,
                                           &error);
  g_assert_no_error (error);
  g_assert_cmpuint (id2, >, 0);

  check_interfaces (c, "/test", interfaces);

  g_assert_true (g_dbus_connection_unregister_object (c, id1));
  g_assert_true (g_dbus_connection_unregister_object (c, id2));
  g_object_unref (c);
}


/* ---------------------------------------------------------------------------------------------------- */

static void
test_async_method_call (GDBusConnection       *connection,
                        const gchar           *sender,
                        const gchar           *object_path,
                        const gchar           *interface_name,
                        const gchar           *method_name,
                        GVariant              *parameters,
                        GDBusMethodInvocation *invocation,
                        gpointer               user_data)
{
  const GDBusPropertyInfo *property;

  /* Strictly speaking, this function should also expect to receive
   * method calls not on the DBUS_INTERFACE_PROPERTIES interface,
   * but we don't do any during this testcase, so assert that.
   */
  g_assert_cmpstr (interface_name, ==, DBUS_INTERFACE_PROPERTIES);
  g_assert_null (g_dbus_method_invocation_get_method_info (invocation));

  property = g_dbus_method_invocation_get_property_info (invocation);

  /* We should never be seeing any property calls on the com.example.Bar
   * interface because it doesn't export any properties.
   *
   * In each case below make sure the interface is org.example.Foo.
   */

  /* Do a whole lot of asserts to make sure that invalid calls are still
   * getting properly rejected by GDBusConnection and that our
   * environment is as we expect it to be.
   */
  if (g_str_equal (method_name, "Get"))
    {
      const gchar *iface_name, *prop_name;

      g_variant_get (parameters, "(&s&s)", &iface_name, &prop_name);
      g_assert_cmpstr (iface_name, ==, "org.example.Foo");
      g_assert_nonnull (property);
      g_assert_cmpstr (prop_name, ==, property->name);
      g_assert_true (property->flags & G_DBUS_PROPERTY_INFO_FLAGS_READABLE);
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(v)", g_variant_new_string (prop_name)));
    }

  else if (g_str_equal (method_name, "Set"))
    {
      const gchar *iface_name, *prop_name;
      GVariant *value;

      g_variant_get (parameters, "(&s&sv)", &iface_name, &prop_name, &value);
      g_assert_cmpstr (iface_name, ==, "org.example.Foo");
      g_assert_nonnull (property);
      g_assert_cmpstr (prop_name, ==, property->name);
      g_assert_true (property->flags & G_DBUS_PROPERTY_INFO_FLAGS_WRITABLE);
      g_assert_true (g_variant_is_of_type (value, G_VARIANT_TYPE (property->signature)));
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("()"));
      g_variant_unref (value);
    }

  else if (g_str_equal (method_name, "GetAll"))
    {
      const gchar *iface_name;

      g_variant_get (parameters, "(&s)", &iface_name);
      g_assert_cmpstr (iface_name, ==, "org.example.Foo");
      g_assert_null (property);
      g_dbus_method_invocation_return_value (invocation,
                                             g_variant_new_parsed ("({ 'PropertyUno': < 'uno' >,"
                                                                   "   'NotWritable': < 'notwrite' > },)"));
    }

  else
    g_assert_not_reached ();
}

static gint outstanding_cases;

static void
ensure_result_cb (GObject      *source,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  GDBusConnection *connection = G_DBUS_CONNECTION (source);
  GVariant *reply;

  reply = g_dbus_connection_call_finish (connection, result, NULL);

  if (user_data == NULL)
    {
      /* Expected an error */
      g_assert_null (reply);
    }
  else
    {
      /* Expected a reply of a particular format. */
      gchar *str;

      g_assert_nonnull (reply);
      str = g_variant_print (reply, TRUE);
      g_assert_cmpstr (str, ==, (const gchar *) user_data);
      g_free (str);

      g_variant_unref (reply);
    }

  g_assert_cmpint (outstanding_cases, >, 0);
  outstanding_cases--;
}

static void
test_async_case (GDBusConnection *connection,
                 const gchar     *expected_reply,
                 const gchar     *method,
                 const gchar     *format_string,
                 ...)
{
  va_list ap;

  va_start (ap, format_string);

  g_dbus_connection_call (connection, g_dbus_connection_get_unique_name (connection), "/foo",
                          DBUS_INTERFACE_PROPERTIES, method, g_variant_new_va (format_string, NULL, &ap),
                          NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, ensure_result_cb, (gpointer) expected_reply);

  va_end (ap);

  outstanding_cases++;
}

static void
test_async_properties (void)
{
  GError *error = NULL;
  guint registration_id, registration_id2;
  static const GDBusInterfaceVTable vtable = {
    test_async_method_call, NULL, NULL, { 0 }
  };

  c = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (c);

  registration_id = g_dbus_connection_register_object (c,
                                                       "/foo",
                                                       (GDBusInterfaceInfo *) &foo_interface_info,
                                                       &vtable, NULL, NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (registration_id, !=, 0);
  registration_id2 = g_dbus_connection_register_object (c,
                                                        "/foo",
                                                        (GDBusInterfaceInfo *) &foo2_interface_info,
                                                        &vtable, NULL, NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (registration_id, !=, 0);

  test_async_case (c, NULL, "random", "()");

  /* Test a variety of error cases */
  test_async_case (c, NULL, "Get", "(si)", "wrong signature", 5);
  test_async_case (c, NULL, "Get", "(ss)", "org.example.WrongInterface", "zzz");
  test_async_case (c, NULL, "Get", "(ss)", "org.example.Foo", "NoSuchProperty");
  test_async_case (c, NULL, "Get", "(ss)", "org.example.Foo", "NotReadable");

  test_async_case (c, NULL, "Set", "(si)", "wrong signature", 5);
  test_async_case (c, NULL, "Set", "(ssv)", "org.example.WrongInterface", "zzz", g_variant_new_string (""));
  test_async_case (c, NULL, "Set", "(ssv)", "org.example.Foo", "NoSuchProperty", g_variant_new_string (""));
  test_async_case (c, NULL, "Set", "(ssv)", "org.example.Foo", "NotWritable", g_variant_new_string (""));
  test_async_case (c, NULL, "Set", "(ssv)", "org.example.Foo", "PropertyUno", g_variant_new_object_path ("/wrong"));

  test_async_case (c, NULL, "GetAll", "(si)", "wrong signature", 5);
  test_async_case (c, NULL, "GetAll", "(s)", "org.example.WrongInterface");

  /* Make sure that we get no unexpected async property calls for com.example.Foo2 */
  test_async_case (c, NULL, "Get", "(ss)", "org.example.Foo2", "zzz");
  test_async_case (c, NULL, "Set", "(ssv)", "org.example.Foo2", "zzz", g_variant_new_string (""));
  test_async_case (c, "(@a{sv} {},)", "GetAll", "(s)", "org.example.Foo2");

  /* Now do the proper things */
  test_async_case (c, "(<'PropertyUno'>,)", "Get", "(ss)", "org.example.Foo", "PropertyUno");
  test_async_case (c, "(<'NotWritable'>,)", "Get", "(ss)", "org.example.Foo", "NotWritable");
  test_async_case (c, "()", "Set", "(ssv)", "org.example.Foo", "PropertyUno", g_variant_new_string (""));
  test_async_case (c, "()", "Set", "(ssv)", "org.example.Foo", "NotReadable", g_variant_new_string (""));
  test_async_case (c, "({'PropertyUno': <'uno'>, 'NotWritable': <'notwrite'>},)", "GetAll", "(s)", "org.example.Foo");

  while (outstanding_cases)
    g_main_context_iteration (NULL, TRUE);

  g_dbus_connection_unregister_object (c, registration_id);
  g_dbus_connection_unregister_object (c, registration_id2);
  g_object_unref (c);
}

typedef struct
{
  GDBusConnection *connection;  /* (owned) */
  guint registration_id;
  guint subtree_registration_id;
} ThreadedUnregistrationData;

static gpointer
unregister_thread_cb (gpointer user_data)
{
  ThreadedUnregistrationData *data = user_data;

  /* Sleeping here makes the race more likely to be hit, as it balances the
   * time taken to set up the thread and unregister, with the time taken to
   * make and handle the D-Bus call. This will likely change with future kernel
   * versions, but there isn’t a more deterministic synchronisation point that
   * I can think of to use instead. */
  usleep (330);

  if (data->registration_id > 0)
    g_assert_true (g_dbus_connection_unregister_object (data->connection, data->registration_id));

  if (data->subtree_registration_id > 0)
    g_assert_true (g_dbus_connection_unregister_subtree (data->connection, data->subtree_registration_id));

  return NULL;
}

static void
async_result_cb (GObject      *source_object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  GAsyncResult **result_out = user_data;

  *result_out = g_object_ref (result);
  g_main_context_wakeup (NULL);
}

/* Returns %TRUE if this iteration resolved the race with the unregistration
 * first, %FALSE if the call handler was invoked first. */
static gboolean
test_threaded_unregistration_iteration (gboolean subtree)
{
  ThreadedUnregistrationData data = { NULL, 0, 0 };
  ObjectRegistrationData object_registration_data = { 0, 0, 2 };
  GError *local_error = NULL;
  GThread *unregister_thread = NULL;
  const gchar *object_path;
  GVariant *value = NULL;
  const gchar *value_str;
  GAsyncResult *call_result = NULL;
  gboolean unregistration_was_first;

  data.connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &local_error);
  g_assert_no_error (local_error);
  g_assert_nonnull (data.connection);

  /* Register an object or a subtree */
  if (!subtree)
    {
      data.registration_id = g_dbus_connection_register_object (data.connection,
                                                                "/foo/boss",
                                                                (GDBusInterfaceInfo *) &foo_interface_info,
                                                                &foo_vtable,
                                                                &object_registration_data,
                                                                on_object_unregistered,
                                                                &local_error);
      g_assert_no_error (local_error);
      g_assert_cmpint (data.registration_id, >, 0);

      object_path = "/foo/boss";
    }
  else
    {
      data.subtree_registration_id = g_dbus_connection_register_subtree (data.connection,
                                                                         "/foo/boss/executives",
                                                                         &subtree_vtable,
                                                                         G_DBUS_SUBTREE_FLAGS_NONE,
                                                                         &object_registration_data,
                                                                         on_subtree_unregistered,
                                                                         &local_error);
      g_assert_no_error (local_error);
      g_assert_cmpint (data.subtree_registration_id, >, 0);

      object_path = "/foo/boss/executives/vp0";
    }

  /* Allow the registrations to go through. */
  g_main_context_iteration (NULL, FALSE);

  /* Spawn a thread to unregister the object/subtree. This will race with
   * the call we subsequently make. */
  unregister_thread = g_thread_new ("unregister-object",
                                    unregister_thread_cb, &data);

  /* Call a method on the object (or an object in the subtree). The callback
   * will be invoked in this main context. */
  g_dbus_connection_call (data.connection,
                          g_dbus_connection_get_unique_name (data.connection),
                          object_path,
                          "org.example.Foo",
                          "Method1",
                          g_variant_new ("(s)", "winwinwin"),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          async_result_cb,
                          &call_result);

  while (call_result == NULL)
    g_main_context_iteration (NULL, TRUE);

  value = g_dbus_connection_call_finish (data.connection, call_result, &local_error);

  /* The result of the method could either be an error (that the object doesn’t
   * exist) or a valid result, depending on how the thread was scheduled
   * relative to the call. */
  unregistration_was_first = (value == NULL);
  if (value != NULL)
    {
      g_assert_no_error (local_error);
      g_assert_true (g_variant_is_of_type (value, G_VARIANT_TYPE ("(s)")));
      g_variant_get (value, "(&s)", &value_str);
      g_assert_cmpstr (value_str, ==, "You passed the string 'winwinwin'. Jolly good!");
    }
  else
    {
      g_assert_null (value);
      g_assert_error (local_error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD);
    }

  g_clear_pointer (&value, g_variant_unref);
  g_clear_error (&local_error);

  /* Tidy up. */
  g_thread_join (g_steal_pointer (&unregister_thread));

  g_clear_object (&call_result);
  g_clear_object (&data.connection);

  /* We defer quitting to a G_PRIORITY_DEFAULT_IDLE function so other queued
   * signal callbacks have a chance to run first.
   * In particular we want to ensure that all calls to on_object_unregistered()
   * are delivered here before we end this function, so that there won't be any
   * invalid stack access.
   * They get dispatched with a higher priority (G_PRIORITY_DEFAULT), so as
   * long as the queue is non-empty g_main_loop_quit won't run
   */
  g_idle_add_once ((GSourceOnceFunc) g_main_loop_quit, loop);
  g_main_loop_run (loop);

  return unregistration_was_first;
}

static void
test_threaded_unregistration (gconstpointer test_data)
{
  gboolean subtree = GPOINTER_TO_INT (test_data);
  guint i;
  guint n_iterations_unregistration_first = 0;
  guint n_iterations_call_first = 0;

  g_test_bug ("https://gitlab.gnome.org/GNOME/glib/-/issues/2400");
  g_test_summary ("Test that object/subtree unregistration from one thread "
                  "doesn’t cause problems when racing with method callbacks "
                  "in another thread for that object or subtree");

  /* Run iterations of the test until it’s likely we’ve hit the race. Limit the
   * number of iterations so the test doesn’t run forever if not. The choice of
   * 100 is arbitrary. */
  for (i = 0; i < 1000 && (n_iterations_unregistration_first < 100 || n_iterations_call_first < 100); i++)
    {
      if (test_threaded_unregistration_iteration (subtree))
        n_iterations_unregistration_first++;
      else
        n_iterations_call_first++;
    }

  /* If the condition below is met, we probably failed to reproduce the race.
   * Don’t fail the test, though, as we can’t always control whether we hit the
   * race, and spurious test failures are annoying. */
  if (n_iterations_unregistration_first < 100 ||
      n_iterations_call_first < 100)
    g_test_skip_printf ("Failed to reproduce race (%u iterations with unregistration first, %u with call first); skipping test",
                        n_iterations_unregistration_first, n_iterations_call_first);
}

/* ---------------------------------------------------------------------------------------------------- */

int
main (int   argc,
      char *argv[])
{
  gint ret;

  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  /* all the tests rely on a shared main loop */
  loop = g_main_loop_new (NULL, FALSE);

  g_test_add_func ("/gdbus/object-registration", test_object_registration);
  g_test_add_data_func ("/gdbus/object-registration-with-closures", GINT_TO_POINTER (FALSE), test_object_registration_with_closures);
  g_test_add_data_func ("/gdbus/object-registration-with-closures2", GINT_TO_POINTER (TRUE), test_object_registration_with_closures);
  g_test_add_func ("/gdbus/registered-interfaces", test_registered_interfaces);
  g_test_add_func ("/gdbus/async-properties", test_async_properties);
  g_test_add_data_func ("/gdbus/threaded-unregistration/object", GINT_TO_POINTER (FALSE), test_threaded_unregistration);
  g_test_add_data_func ("/gdbus/threaded-unregistration/subtree", GINT_TO_POINTER (TRUE), test_threaded_unregistration);

  /* TODO: check that we spit out correct introspection data */
  /* TODO: check that registering a whole subtree works */

  ret = session_bus_run ();

  g_main_loop_unref (loop);

  return ret;
}
