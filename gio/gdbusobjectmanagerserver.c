/* GDBus - GLib D-Bus Library
 *
 * Copyright (C) 2008-2010 Red Hat, Inc.
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

#include "config.h"

#include "gdbusobjectmanager.h"
#include "gdbusobjectmanagerserver.h"
#include "gdbusobject.h"
#include "gdbusobjectstub.h"
#include "gdbusinterfacestub.h"
#include "gdbusconnection.h"
#include "gdbusintrospection.h"
#include "gdbusmethodinvocation.h"
#include "gdbuserror.h"

#include "glibintl.h"

/**
 * SECTION:gdbusobjectmanagerserver
 * @short_description: Service-side object manager
 * @include: gio/gio.h
 *
 * #GDBusObjectManagerServer is used to export #GDBusObject instances using
 * the standardized <ulink
 * url="http://dbus.freedesktop.org/doc/dbus-specification.html#standard-interfaces-objectmanager">org.freedesktop.DBus.ObjectManager</ulink>
 * interface. For example, remote D-Bus clients can get all objects
 * and properties in a single call. Additionally, any change in the
 * object hierarchy is broadcast using signals. This means that D-Bus
 * clients can keep caches up to date by only listening to D-Bus
 * signals.
 *
 * See #GDBusObjectManagerClient for the client-side code that is intended to
 * be used with #GDBusObjectManagerServer.
 */

typedef struct
{
  GDBusObjectStub *object;
  GDBusObjectManagerServer *manager;
  GHashTable *map_iface_name_to_iface;
  gboolean exported;
} RegistrationData;

static void registration_data_free (RegistrationData *data);

static void g_dbus_object_manager_server_emit_interfaces_added (GDBusObjectManagerServer *manager,
                                                         RegistrationData   *data,
                                                         const gchar *const *interfaces);

static void g_dbus_object_manager_server_emit_interfaces_removed (GDBusObjectManagerServer *manager,
                                                           RegistrationData   *data,
                                                           const gchar *const *interfaces);

struct _GDBusObjectManagerServerPrivate
{
  GDBusConnection *connection;
  gchar *object_path;
  gchar *object_path_ending_in_slash;
  GHashTable *map_object_path_to_data;
  guint manager_reg_id;
};

enum
{
  PROP_0,
  PROP_CONNECTION,
  PROP_OBJECT_PATH
};

static void dbus_object_manager_interface_init (GDBusObjectManagerIface *iface);

G_DEFINE_TYPE_WITH_CODE (GDBusObjectManagerServer, g_dbus_object_manager_server, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_DBUS_OBJECT_MANAGER, dbus_object_manager_interface_init));

static void g_dbus_object_manager_server_constructed (GObject *object);

static void
g_dbus_object_manager_server_finalize (GObject *object)
{
  GDBusObjectManagerServer *manager = G_DBUS_OBJECT_MANAGER_SERVER (object);

  g_hash_table_unref (manager->priv->map_object_path_to_data);
  if (manager->priv->manager_reg_id > 0)
    g_warn_if_fail (g_dbus_connection_unregister_object (manager->priv->connection, manager->priv->manager_reg_id));
  g_object_unref (manager->priv->connection);
  g_free (manager->priv->object_path);
  g_free (manager->priv->object_path_ending_in_slash);

  if (G_OBJECT_CLASS (g_dbus_object_manager_server_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (g_dbus_object_manager_server_parent_class)->finalize (object);
}

static void
g_dbus_object_manager_server_get_property (GObject    *_object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GDBusObjectManagerServer *manager = G_DBUS_OBJECT_MANAGER_SERVER (_object);

  switch (prop_id)
    {
    case PROP_CONNECTION:
      g_value_set_object (value, g_dbus_object_manager_server_get_connection (manager));
      break;

    case PROP_OBJECT_PATH:
      g_value_set_string (value, g_dbus_object_manager_get_object_path (G_DBUS_OBJECT_MANAGER (manager)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (_object, prop_id, pspec);
      break;
    }
}

static void
g_dbus_object_manager_server_set_property (GObject       *_object,
                                    guint          prop_id,
                                    const GValue  *value,
                                    GParamSpec    *pspec)
{
  GDBusObjectManagerServer *manager = G_DBUS_OBJECT_MANAGER_SERVER (_object);

  switch (prop_id)
    {
    case PROP_CONNECTION:
      g_assert (manager->priv->connection == NULL);
      g_assert (G_IS_DBUS_CONNECTION (g_value_get_object (value)));
      manager->priv->connection = g_value_dup_object (value);
      break;

    case PROP_OBJECT_PATH:
      g_assert (manager->priv->object_path == NULL);
      g_assert (g_variant_is_object_path (g_value_get_string (value)));
      manager->priv->object_path = g_value_dup_string (value);
      manager->priv->object_path_ending_in_slash = g_strdup_printf ("%s/", manager->priv->object_path);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (_object, prop_id, pspec);
      break;
    }
}

static void
g_dbus_object_manager_server_class_init (GDBusObjectManagerServerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize     = g_dbus_object_manager_server_finalize;
  gobject_class->constructed  = g_dbus_object_manager_server_constructed;
  gobject_class->set_property = g_dbus_object_manager_server_set_property;
  gobject_class->get_property = g_dbus_object_manager_server_get_property;

  /**
   * GDBusObjectManagerServer:connection:
   *
   * The #GDBusConnection to export objects on.
   *
   * Since: 2.30
   */
  g_object_class_install_property (gobject_class,
                                   PROP_CONNECTION,
                                   g_param_spec_object ("connection",
                                                        "Connection",
                                                        "The connection to export objects on",
                                                        G_TYPE_DBUS_CONNECTION,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GDBusObjectManagerServer:object-path:
   *
   * The object path to register the manager object at.
   *
   * Since: 2.30
   */
  g_object_class_install_property (gobject_class,
                                   PROP_OBJECT_PATH,
                                   g_param_spec_string ("object-path",
                                                        "Object Path",
                                                        "The object path to register the manager object at",
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (klass, sizeof (GDBusObjectManagerServerPrivate));
}

static void
g_dbus_object_manager_server_init (GDBusObjectManagerServer *manager)
{
  manager->priv = G_TYPE_INSTANCE_GET_PRIVATE (manager,
                                               G_TYPE_DBUS_OBJECT_MANAGER_SERVER,
                                               GDBusObjectManagerServerPrivate);
  manager->priv->map_object_path_to_data = g_hash_table_new_full (g_str_hash,
                                                                  g_str_equal,
                                                                  g_free,
                                                                  (GDestroyNotify) registration_data_free);
}

/**
 * g_dbus_object_manager_server_new:
 * @connection: A #GDBusConnection.
 * @object_path: The object path to export the manager object at.
 *
 * Creates a new #GDBusObjectManagerServer object.
 *
 * TODO: make it so that the objects are not exported yet -
 * e.g. start()/stop() semantics.
 *
 * Returns: A #GDBusObjectManagerServer object. Free with g_object_unref().
 *
 * Since: 2.30
 */
GDBusObjectManagerServer *
g_dbus_object_manager_server_new (GDBusConnection *connection,
                           const gchar     *object_path)
{
  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);
  g_return_val_if_fail (g_variant_is_object_path (object_path), NULL);
  return G_DBUS_OBJECT_MANAGER_SERVER (g_object_new (G_TYPE_DBUS_OBJECT_MANAGER_SERVER,
                                              "connection", connection,
                                              "object-path", object_path,
                                              NULL));
}

/**
 * g_dbus_object_manager_server_get_connection:
 * @manager: A #GDBusObjectManagerServer
 *
 * Gets the #GDBusConnection used by @manager.
 *
 * Returns: A #GDBusConnection object. Do not free, the object belongs
 * to @manager.
 *
 * Since: 2.30
 */
GDBusConnection *
g_dbus_object_manager_server_get_connection (GDBusObjectManagerServer *manager)
{
  g_return_val_if_fail (G_IS_DBUS_OBJECT_MANAGER_SERVER (manager), NULL);
  return manager->priv->connection;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
registration_data_export_interface (RegistrationData     *data,
                                    GDBusInterfaceStub   *interface_stub)
{
  GDBusInterfaceInfo *info;
  GError *error;
  const gchar *object_path;

  object_path = g_dbus_object_get_object_path (G_DBUS_OBJECT (data->object));

  info = g_dbus_interface_stub_get_info (interface_stub);
  error = NULL;
  if (!g_dbus_interface_stub_export (interface_stub,
                                     data->manager->priv->connection,
                                     object_path,
                                     &error))
    {
      /* TODO: probably wrong to complain on stderr */
      g_warning ("%s: Error registering object at %s with interface %s: %s",
                 G_STRLOC,
                 object_path,
                 info->name,
                 error->message);
      g_error_free (error);
      goto out;
    }

  g_assert (g_hash_table_lookup (data->map_iface_name_to_iface, info->name) == NULL);
  g_hash_table_insert (data->map_iface_name_to_iface,
                       info->name,
                       g_object_ref (interface_stub));

  /* if we are already exported, then... */
  if (data->exported)
    {
      const gchar *interfaces[2];
      /* emit InterfacesAdded on the ObjectManager object */
      interfaces[0] = info->name;
      interfaces[1] = NULL;
      g_dbus_object_manager_server_emit_interfaces_added (data->manager, data, interfaces);
    }

 out:
  ;
}

static void
registration_data_unexport_interface (RegistrationData     *data,
                                      GDBusInterfaceStub   *interface_stub)
{
  GDBusInterfaceInfo *info;
  GDBusInterfaceStub *iface;

  info = g_dbus_interface_stub_get_info (interface_stub);
  iface = g_hash_table_lookup (data->map_iface_name_to_iface, info->name);
  g_assert (iface != NULL);

  g_dbus_interface_stub_unexport (iface);

  g_warn_if_fail (g_hash_table_remove (data->map_iface_name_to_iface, info->name));

  /* if we are already exported, then... */
  if (data->exported)
    {
      const gchar *interfaces[2];
      /* emit InterfacesRemoved on the ObjectManager object */
      interfaces[0] = info->name;
      interfaces[1] = NULL;
      g_dbus_object_manager_server_emit_interfaces_removed (data->manager, data, interfaces);
    }
}

/* ---------------------------------------------------------------------------------------------------- */

static void
on_interface_added (GDBusObject    *object,
                    GDBusInterface *interface,
                    gpointer        user_data)
{
  RegistrationData *data = user_data;
  registration_data_export_interface (data, G_DBUS_INTERFACE_STUB (interface));
}

static void
on_interface_removed (GDBusObject    *object,
                      GDBusInterface *interface,
                      gpointer        user_data)
{
  RegistrationData *data = user_data;
  registration_data_unexport_interface (data, G_DBUS_INTERFACE_STUB (interface));
}

/* ---------------------------------------------------------------------------------------------------- */


static void
registration_data_free (RegistrationData *data)
{
  GHashTableIter iter;
  GDBusInterfaceStub *iface;

  data->exported = FALSE;

  g_hash_table_iter_init (&iter, data->map_iface_name_to_iface);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer) &iface))
    g_dbus_interface_stub_unexport (iface);

  g_signal_handlers_disconnect_by_func (data->object, G_CALLBACK (on_interface_added), data);
  g_signal_handlers_disconnect_by_func (data->object, G_CALLBACK (on_interface_removed), data);
  g_object_unref (data->object);
  g_hash_table_destroy (data->map_iface_name_to_iface);
  g_free (data);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_object_manager_server_export:
 * @manager: A #GDBusObjectManagerServer.
 * @object: A #GDBusObjectStub.
 *
 * Exports @object on @manager.
 *
 * If there is already a #GDBusObject exported at the object path,
 * then the old object is removed.
 *
 * The object path for @object must be in the hierarchy rooted by the
 * object path for @manager.
 *
 * Note that @manager will take a reference on @object for as long as
 * it is exported.
 *
 * Since: 2.30
 */
void
g_dbus_object_manager_server_export (GDBusObjectManagerServer  *manager,
                              GDBusObjectStub     *object)
{
  RegistrationData *data;
  GList *existing_interfaces;
  GList *l;
  GPtrArray *interface_names;
  const gchar *object_path;

  object_path = g_dbus_object_get_object_path (G_DBUS_OBJECT (object));

  g_return_if_fail (G_IS_DBUS_OBJECT_MANAGER_SERVER (manager));
  g_return_if_fail (G_IS_DBUS_OBJECT (object));
  g_return_if_fail (g_str_has_prefix (object_path, manager->priv->object_path_ending_in_slash));

  interface_names = g_ptr_array_new ();

  data = g_hash_table_lookup (manager->priv->map_object_path_to_data, object_path);
  if (data != NULL)
    g_dbus_object_manager_server_unexport (manager, object_path);

  data = g_new0 (RegistrationData, 1);
  data->object = g_object_ref (object);
  data->manager = manager;
  data->map_iface_name_to_iface = g_hash_table_new_full (g_str_hash,
                                                         g_str_equal,
                                                         NULL,
                                                         (GDestroyNotify) g_object_unref);

  g_signal_connect (object,
                    "interface-added",
                    G_CALLBACK (on_interface_added),
                    data);
  g_signal_connect (object,
                    "interface-removed",
                    G_CALLBACK (on_interface_removed),
                    data);

  /* Register all known interfaces - note that data->exported is FALSE so
   * we don't emit any InterfacesAdded signals.
   */
  existing_interfaces = g_dbus_object_get_interfaces (G_DBUS_OBJECT (object));
  for (l = existing_interfaces; l != NULL; l = l->next)
    {
      GDBusInterfaceStub *interface_stub = G_DBUS_INTERFACE_STUB (l->data);
      registration_data_export_interface (data, interface_stub);
      g_ptr_array_add (interface_names, g_dbus_interface_stub_get_info (interface_stub)->name);
    }
  g_list_foreach (existing_interfaces, (GFunc) g_object_unref, NULL);
  g_list_free (existing_interfaces);
  g_ptr_array_add (interface_names, NULL);

  data->exported = TRUE;

  /* now emit InterfacesAdded() for all the interfaces */
  g_dbus_object_manager_server_emit_interfaces_added (manager, data, (const gchar *const *) interface_names->pdata);
  g_ptr_array_unref (interface_names);

  g_hash_table_insert (manager->priv->map_object_path_to_data,
                       g_strdup (object_path),
                       data);
}

/**
 * g_dbus_object_manager_server_export_uniquely:
 * @manager: A #GDBusObjectManagerServer.
 * @object: An object.
 *
 * Like g_dbus_object_manager_server_export() but appends a string of
 * the form <literal>_N</literal> (with N being a natural number) to
 * @object<!-- -->'s object path if an object with the given path
 * already exists. As such, the #GDBusObjectProxy:object-path property
 * of @object may be modified.
 *
 * Since: 2.30
 */
void
g_dbus_object_manager_server_export_uniquely (GDBusObjectManagerServer  *manager,
                                              GDBusObjectStub     *object)
{
  gchar *orig_object_path;
  gchar *object_path;
  guint count;
  gboolean modified;

  orig_object_path = g_strdup (g_dbus_object_get_object_path (G_DBUS_OBJECT (object)));

  g_return_if_fail (G_IS_DBUS_OBJECT_MANAGER_SERVER (manager));
  g_return_if_fail (G_IS_DBUS_OBJECT (object));
  g_return_if_fail (g_str_has_prefix (orig_object_path, manager->priv->object_path_ending_in_slash));

  object_path = g_strdup (orig_object_path);
  count = 1;
  modified = FALSE;
  while (TRUE)
    {
      RegistrationData *data;
      data = g_hash_table_lookup (manager->priv->map_object_path_to_data, object_path);
      if (data == NULL)
        {
          break;
        }
      g_free (object_path);
      object_path = g_strdup_printf ("%s_%d", orig_object_path, count++);
      modified = TRUE;
    }

  if (modified)
    g_dbus_object_stub_set_object_path (G_DBUS_OBJECT_STUB (object), object_path);

  g_dbus_object_manager_server_export (manager, object);

  g_free (object_path);
  g_free (orig_object_path);
}

/**
 * g_dbus_object_manager_server_unexport:
 * @manager: A #GDBusObjectManagerServer.
 * @object_path: An object path.
 *
 * If @manager has an object at @path, removes the object. Otherwise
 * does nothing.
 *
 * Note that @object_path must be in the hierarchy rooted by the
 * object path for @manager.
 *
 * Since: 2.30
 */
void
g_dbus_object_manager_server_unexport (GDBusObjectManagerServer  *manager,
                                const gchar         *object_path)
{
  RegistrationData *data;

  g_return_if_fail (G_IS_DBUS_OBJECT_MANAGER_SERVER (manager));
  g_return_if_fail (g_variant_is_object_path (object_path));
  g_return_if_fail (g_str_has_prefix (object_path, manager->priv->object_path_ending_in_slash));

  data = g_hash_table_lookup (manager->priv->map_object_path_to_data, object_path);
  if (data != NULL)
    {
      GPtrArray *interface_names;
      GHashTableIter iter;
      const gchar *iface_name;

      interface_names = g_ptr_array_new ();
      g_hash_table_iter_init (&iter, data->map_iface_name_to_iface);
      while (g_hash_table_iter_next (&iter, (gpointer) &iface_name, NULL))
        g_ptr_array_add (interface_names, (gpointer) iface_name);
      g_ptr_array_add (interface_names, NULL);
      /* now emit InterfacesRemoved() for all the interfaces */
      g_dbus_object_manager_server_emit_interfaces_removed (manager, data, (const gchar *const *) interface_names->pdata);
      g_ptr_array_unref (interface_names);

      g_hash_table_remove (manager->priv->map_object_path_to_data, object_path);
    }
}


/* ---------------------------------------------------------------------------------------------------- */

static const GDBusArgInfo manager_interfaces_added_signal_info_arg0 =
{
  -1,
  "object_path",
  "o",
  (GDBusAnnotationInfo**) NULL,
};

static const GDBusArgInfo manager_interfaces_added_signal_info_arg1 =
{
  -1,
  "interfaces_and_properties",
  "a{sa{sv}}",
  (GDBusAnnotationInfo**) NULL,
};

static const GDBusArgInfo * const manager_interfaces_added_signal_info_arg_pointers[] =
{
  &manager_interfaces_added_signal_info_arg0,
  &manager_interfaces_added_signal_info_arg1,
  NULL
};

static const GDBusSignalInfo manager_interfaces_added_signal_info =
{
  -1,
  "InterfacesAdded",
  (GDBusArgInfo**) &manager_interfaces_added_signal_info_arg_pointers,
  (GDBusAnnotationInfo**) NULL
};

/* ---------- */

static const GDBusArgInfo manager_interfaces_removed_signal_info_arg0 =
{
  -1,
  "object_path",
  "o",
  (GDBusAnnotationInfo**) NULL,
};

static const GDBusArgInfo manager_interfaces_removed_signal_info_arg1 =
{
  -1,
  "interfaces",
  "as",
  (GDBusAnnotationInfo**) NULL,
};

static const GDBusArgInfo * const manager_interfaces_removed_signal_info_arg_pointers[] =
{
  &manager_interfaces_removed_signal_info_arg0,
  &manager_interfaces_removed_signal_info_arg1,
  NULL
};

static const GDBusSignalInfo manager_interfaces_removed_signal_info =
{
  -1,
  "InterfacesRemoved",
  (GDBusArgInfo**) &manager_interfaces_removed_signal_info_arg_pointers,
  (GDBusAnnotationInfo**) NULL
};

/* ---------- */

static const GDBusSignalInfo * const manager_signal_info_pointers[] =
{
  &manager_interfaces_added_signal_info,
  &manager_interfaces_removed_signal_info,
  NULL
};

/* ---------- */

static const GDBusArgInfo manager_get_all_method_info_out_arg0 =
{
  -1,
  "object_paths_interfaces_and_properties",
  "a{oa{sa{sv}}}",
  (GDBusAnnotationInfo**) NULL,
};

static const GDBusArgInfo * const manager_get_all_method_info_out_arg_pointers[] =
{
  &manager_get_all_method_info_out_arg0,
  NULL
};

static const GDBusMethodInfo manager_get_all_method_info =
{
  -1,
  "GetManagedObjects",
  (GDBusArgInfo**) NULL,
  (GDBusArgInfo**) &manager_get_all_method_info_out_arg_pointers,
  (GDBusAnnotationInfo**) NULL
};

static const GDBusMethodInfo * const manager_method_info_pointers[] =
{
  &manager_get_all_method_info,
  NULL
};

/* ---------- */

static const GDBusInterfaceInfo manager_interface_info =
{
  -1,
  "org.freedesktop.DBus.ObjectManager",
  (GDBusMethodInfo **) manager_method_info_pointers,
  (GDBusSignalInfo **) manager_signal_info_pointers,
  (GDBusPropertyInfo **) NULL,
  (GDBusAnnotationInfo **) NULL
};

static void
manager_method_call (GDBusConnection       *connection,
                     const gchar           *sender,
                     const gchar           *object_path,
                     const gchar           *interface_name,
                     const gchar           *method_name,
                     GVariant              *parameters,
                     GDBusMethodInvocation *invocation,
                     gpointer               user_data)
{
  GDBusObjectManagerServer *manager = G_DBUS_OBJECT_MANAGER_SERVER (user_data);
  GVariantBuilder array_builder;
  GHashTableIter object_iter;
  RegistrationData *data;

  if (g_strcmp0 (method_name, "GetManagedObjects") == 0)
    {
      g_variant_builder_init (&array_builder, G_VARIANT_TYPE ("a{oa{sa{sv}}}"));
      g_hash_table_iter_init (&object_iter, manager->priv->map_object_path_to_data);
      while (g_hash_table_iter_next (&object_iter, NULL, (gpointer) &data))
        {
          GVariantBuilder interfaces_builder;
          GHashTableIter interface_iter;
          GDBusInterfaceStub *iface;
          const gchar *iter_object_path;

          g_variant_builder_init (&interfaces_builder, G_VARIANT_TYPE ("a{sa{sv}}"));
          g_hash_table_iter_init (&interface_iter, data->map_iface_name_to_iface);
          while (g_hash_table_iter_next (&interface_iter, NULL, (gpointer) &iface))
            {
              g_variant_builder_add_value (&interfaces_builder,
                                           g_variant_new ("{s@a{sv}}",
                                                          g_dbus_interface_stub_get_info (iface)->name,
                                                          g_dbus_interface_stub_get_properties (iface)));
            }
          iter_object_path = g_dbus_object_get_object_path (G_DBUS_OBJECT (data->object));
          g_variant_builder_add (&array_builder,
                                 "{oa{sa{sv}}}",
                                 iter_object_path,
                                 &interfaces_builder);
        }

      g_dbus_method_invocation_return_value (invocation,
                                             g_variant_new ("(a{oa{sa{sv}}})",
                                                            &array_builder));
    }
  else
    {
      g_dbus_method_invocation_return_error (invocation,
                                             G_DBUS_ERROR,
                                             G_DBUS_ERROR_UNKNOWN_METHOD,
                                             "Unknown method %s - only GetManagedObjects() is supported",
                                             method_name);
    }
}

static const GDBusInterfaceVTable manager_interface_vtable =
{
  manager_method_call, /* handle_method_call */
  NULL, /* get_property */
  NULL  /* set_property */
};

/* ---------------------------------------------------------------------------------------------------- */

static void
g_dbus_object_manager_server_constructed (GObject *object)
{
  GDBusObjectManagerServer *manager = G_DBUS_OBJECT_MANAGER_SERVER (object);
  GError *error;

  error = NULL;
  manager->priv->manager_reg_id = g_dbus_connection_register_object (manager->priv->connection,
                                                                     manager->priv->object_path,
                                                                     (GDBusInterfaceInfo *) &manager_interface_info,
                                                                     &manager_interface_vtable,
                                                                     manager,
                                                                     NULL, /* user_data_free_func */
                                                                     &error);
  if (manager->priv->manager_reg_id == 0)
    {
      /* TODO: probably wrong to complain on stderr */
      g_warning ("%s: Error registering manager at %s: %s",
                 G_STRLOC,
                 manager->priv->object_path,
                 error->message);
      g_error_free (error);
    }

  if (G_OBJECT_CLASS (g_dbus_object_manager_server_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (g_dbus_object_manager_server_parent_class)->constructed (object);
}

static void
g_dbus_object_manager_server_emit_interfaces_added (GDBusObjectManagerServer *manager,
                                             RegistrationData   *data,
                                             const gchar *const *interfaces)
{
  GVariantBuilder array_builder;
  GError *error;
  guint n;
  const gchar *object_path;

  g_variant_builder_init (&array_builder, G_VARIANT_TYPE ("a{sa{sv}}"));
  for (n = 0; interfaces[n] != NULL; n++)
    {
      GDBusInterfaceStub *iface;
      iface = g_hash_table_lookup (data->map_iface_name_to_iface, interfaces[n]);
      g_assert (iface != NULL);
      g_variant_builder_add_value (&array_builder,
                                   g_variant_new ("{s@a{sv}}",
                                                  interfaces[n],
                                                  g_dbus_interface_stub_get_properties (iface)));
    }

  error = NULL;
  object_path = g_dbus_object_get_object_path (G_DBUS_OBJECT (data->object));
  g_dbus_connection_emit_signal (data->manager->priv->connection,
                                 NULL, /* destination_bus_name */
                                 manager->priv->object_path,
                                 manager_interface_info.name,
                                 "InterfacesAdded",
                                 g_variant_new ("(oa{sa{sv}})",
                                                object_path,
                                                &array_builder),
                                 &error);
  g_assert_no_error (error);
}

static void
g_dbus_object_manager_server_emit_interfaces_removed (GDBusObjectManagerServer *manager,
                                               RegistrationData   *data,
                                               const gchar *const *interfaces)
{
  GVariantBuilder array_builder;
  GError *error;
  guint n;
  const gchar *object_path;

  g_variant_builder_init (&array_builder, G_VARIANT_TYPE ("as"));
  for (n = 0; interfaces[n] != NULL; n++)
    g_variant_builder_add (&array_builder, "s", interfaces[n]);

  error = NULL;
  object_path = g_dbus_object_get_object_path (G_DBUS_OBJECT (data->object));
  g_dbus_connection_emit_signal (data->manager->priv->connection,
                                 NULL, /* destination_bus_name */
                                 manager->priv->object_path,
                                 manager_interface_info.name,
                                 "InterfacesRemoved",
                                 g_variant_new ("(oas)",
                                                object_path,
                                                &array_builder),
                                 &error);
  g_assert_no_error (error);
}

/* ---------------------------------------------------------------------------------------------------- */

static GList *
g_dbus_object_manager_server_get_objects (GDBusObjectManager  *_manager)
{
  GDBusObjectManagerServer *manager = G_DBUS_OBJECT_MANAGER_SERVER (_manager);
  GList *ret;
  GHashTableIter iter;
  RegistrationData *data;

  ret = NULL;
  g_hash_table_iter_init (&iter, manager->priv->map_object_path_to_data);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer) &data))
    {
      ret = g_list_prepend (ret, g_object_ref (data->object));
    }

  return ret;
}

static const gchar *
g_dbus_object_manager_server_get_object_path (GDBusObjectManager *_manager)
{
  GDBusObjectManagerServer *manager = G_DBUS_OBJECT_MANAGER_SERVER (_manager);
  return manager->priv->object_path;
}

static GDBusObject *
g_dbus_object_manager_server_get_object (GDBusObjectManager *_manager,
                                         const gchar        *object_path)
{
  GDBusObjectManagerServer *manager = G_DBUS_OBJECT_MANAGER_SERVER (_manager);
  GDBusObject *ret;
  RegistrationData *data;

  ret = NULL;
  data = g_hash_table_lookup (manager->priv->map_object_path_to_data, object_path);
  if (data != NULL)
    ret = g_object_ref (data->object);
  return ret;
}

static GDBusInterface *
g_dbus_object_manager_server_get_interface  (GDBusObjectManager  *_manager,
                                             const gchar         *object_path,
                                             const gchar         *interface_name)
{
  GDBusInterface *ret;
  GDBusObject *object;

  ret = NULL;

  object = g_dbus_object_manager_get_object (_manager, object_path);
  if (object == NULL)
    goto out;

  ret = g_dbus_object_get_interface (object, interface_name);
  g_object_unref (object);

 out:
  return ret;
}

static void
dbus_object_manager_interface_init (GDBusObjectManagerIface *iface)
{
  iface->get_object_path = g_dbus_object_manager_server_get_object_path;
  iface->get_objects     = g_dbus_object_manager_server_get_objects;
  iface->get_object      = g_dbus_object_manager_server_get_object;
  iface->get_interface   = g_dbus_object_manager_server_get_interface;
}
