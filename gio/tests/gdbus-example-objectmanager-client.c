
#include "gdbus-example-objectmanager-generated.h"

/* ---------------------------------------------------------------------------------------------------- */

static void
on_object_added (GDBusObjectManager *manager,
                 GDBusObject        *object,
                 gpointer            user_data)
{
  gchar *owner;
  owner = g_dbus_object_manager_client_get_name_owner (G_DBUS_OBJECT_MANAGER_CLIENT (manager));
  g_debug ("added object at %s (owner %s)", g_dbus_object_get_object_path (object), owner);
  g_free (owner);
}

static void
on_object_removed (GDBusObjectManager *manager,
                   GDBusObject        *object,
                   gpointer            user_data)
{
  gchar *owner;
  owner = g_dbus_object_manager_client_get_name_owner (G_DBUS_OBJECT_MANAGER_CLIENT (manager));
  g_debug ("removed object at %s (owner %s)", g_dbus_object_get_object_path (object), owner);
  g_free (owner);
}

static void
on_notify_name_owner (GObject    *object,
                      GParamSpec *pspec,
                      gpointer    user_data)
{
  GDBusObjectManagerClient *manager = G_DBUS_OBJECT_MANAGER_CLIENT (object);
  gchar *name_owner;

  name_owner = g_dbus_object_manager_client_get_name_owner (manager);
  g_debug ("name-owner: %s", name_owner);
  g_free (name_owner);
}

static void
on_interface_proxy_properties_changed (GDBusObjectManagerClient *manager,
                                       GDBusObjectProxy         *object_proxy,
                                       GDBusProxy               *interface_proxy,
                                       GVariant                 *changed_properties,
                                       const gchar *const       *invalidated_properties,
                                       gpointer                  user_data)
{
  GVariantIter iter;
  const gchar *key;
  GVariant *value;
  gchar *s;

  g_print ("Properties Changed on %s:\n", g_dbus_object_get_object_path (G_DBUS_OBJECT (object_proxy)));
  g_variant_iter_init (&iter, changed_properties);
  while (g_variant_iter_next (&iter, "{&sv}", &key, &value))
    {
      s = g_variant_print (value, TRUE);
      g_print ("  %s -> %s\n", key, s);
      g_variant_unref (value);
      g_free (s);
    }
}

gint
main (gint argc, gchar *argv[])
{
  GDBusObjectManager *manager;
  GMainLoop *loop;
  GError *error;
  gchar *name_owner;
  GList *objects;
  GList *l;

  manager = NULL;
  loop = NULL;

  g_type_init ();

  loop = g_main_loop_new (NULL, FALSE);

  error = NULL;
  manager = example_object_manager_client_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                            G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
                                                            "org.gtk.GDBus.Examples.ObjectManager",
                                                            "/example/Animals",
                                                            NULL, /* GCancellable */
                                                            &error);
  if (manager == NULL)
    {
      g_printerr ("Error getting object manager client: %s", error->message);
      g_error_free (error);
      goto out;
    }

  name_owner = g_dbus_object_manager_client_get_name_owner (G_DBUS_OBJECT_MANAGER_CLIENT (manager));
  g_debug ("name-owner: %s", name_owner);
  g_free (name_owner);

  objects = g_dbus_object_manager_get_objects (manager);
  for (l = objects; l != NULL; l = l->next)
    {
      GDBusObject *object = G_DBUS_OBJECT (l->data);
      g_debug ("proxy has object at %s", g_dbus_object_get_object_path (object));
    }
  g_list_foreach (objects, (GFunc) g_object_unref, NULL);
  g_list_free (objects);

  g_signal_connect (manager,
                    "notify::name-owner",
                    G_CALLBACK (on_notify_name_owner),
                    NULL);
  g_signal_connect (manager,
                    "object-added",
                    G_CALLBACK (on_object_added),
                    NULL);
  g_signal_connect (manager,
                    "object-removed",
                    G_CALLBACK (on_object_removed),
                    NULL);
  g_signal_connect (manager,
                    "interface-proxy-properties-changed",
                    G_CALLBACK (on_interface_proxy_properties_changed),
                    NULL);

  g_main_loop_run (loop);

 out:
  if (manager != NULL)
    g_object_unref (manager);
  if (loop != NULL)
    g_main_loop_unref (loop);

  return 0;
}
