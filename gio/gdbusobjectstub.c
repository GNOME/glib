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

#include "gdbusobject.h"
#include "gdbusobjectstub.h"
#include "gdbusinterfacestub.h"
#include "gio-marshal.h"
#include "gdbusprivate.h"
#include "gdbusmethodinvocation.h"
#include "gdbusintrospection.h"
#include "gdbusinterface.h"
#include "gdbusutils.h"

#include "glibintl.h"

/**
 * SECTION:gdbusobjectstub
 * @short_description: Service-side D-Bus object
 * @include: gio/gio.h
 *
 * A #GDBusObjectStub instance is essentially a group of D-Bus
 * interfaces. The set of exported interfaces on the object may be
 * dynamic and change at runtime.
 *
 * This type is intended to be used with #GDBusObjectManager.
 */

struct _GDBusObjectStubPrivate
{
  gchar *object_path;
  GHashTable *map_name_to_iface;
};

enum
{
  PROP_0,
  PROP_OBJECT_PATH
};

enum
{
  AUTHORIZE_METHOD_SIGNAL,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = {0};

static void dbus_object_interface_init (GDBusObjectIface *iface);

G_DEFINE_TYPE_WITH_CODE (GDBusObjectStub, g_dbus_object_stub, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_DBUS_OBJECT, dbus_object_interface_init));


static void
g_dbus_object_stub_finalize (GObject *_object)
{
  GDBusObjectStub *object = G_DBUS_OBJECT_STUB (_object);

  g_free (object->priv->object_path);
  g_hash_table_unref (object->priv->map_name_to_iface);

  if (G_OBJECT_CLASS (g_dbus_object_stub_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (g_dbus_object_stub_parent_class)->finalize (_object);
}

static void
g_dbus_object_stub_get_property (GObject    *_object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GDBusObjectStub *object = G_DBUS_OBJECT_STUB (_object);

  switch (prop_id)
    {
    case PROP_OBJECT_PATH:
      g_value_take_string (value, object->priv->object_path);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (_object, prop_id, pspec);
      break;
    }
}

static void
g_dbus_object_stub_set_property (GObject       *_object,
                                 guint          prop_id,
                                 const GValue  *value,
                                 GParamSpec    *pspec)
{
  GDBusObjectStub *object = G_DBUS_OBJECT_STUB (_object);

  switch (prop_id)
    {
    case PROP_OBJECT_PATH:
      g_dbus_object_stub_set_object_path (object, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (_object, prop_id, pspec);
      break;
    }
}

static gboolean
g_dbus_object_stub_authorize_method_default (GDBusObjectStub       *object,
                                             GDBusInterfaceStub    *interface,
                                             GDBusMethodInvocation *invocation)
{
  return TRUE;
}

static void
g_dbus_object_stub_class_init (GDBusObjectStubClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize     = g_dbus_object_stub_finalize;
  gobject_class->set_property = g_dbus_object_stub_set_property;
  gobject_class->get_property = g_dbus_object_stub_get_property;

  klass->authorize_method = g_dbus_object_stub_authorize_method_default;

  /**
   * GDBusObjectStub:object-path:
   *
   * The object path where the object is exported.
   *
   * Since: 2.30
   */
  g_object_class_install_property (gobject_class,
                                   PROP_OBJECT_PATH,
                                   g_param_spec_string ("object-path",
                                                        "Object Path",
                                                        "The object path where the object is exported",
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GDBusObjectStub::authorize-method:
   * @object: The #GDBusObjectStub emitting the signal.
   * @interface: The #GDBusInterfaceStub that @invocation is on.
   * @invocation: A #GDBusMethodInvocation.
   *
   * Emitted when a method is invoked by a remote caller and used to
   * determine if the method call is authorized.
   *
   * This signal is like #GDBusInterfaceStub<!-- -->'s
   * #GDBusInterfaceStub::g-authorize-method signal, except that it is
   * for the enclosing object.
   *
   * The default class handler just returns %TRUE.
   *
   * Returns: %TRUE if the call is authorized, %FALSE otherwise.
   *
   * Since: 2.30
   */
  signals[AUTHORIZE_METHOD_SIGNAL] =
    g_signal_new ("authorize-method",
                  G_TYPE_DBUS_OBJECT_STUB,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GDBusObjectStubClass, authorize_method),
                  _g_signal_accumulator_false_handled,
                  NULL,
                  _gio_marshal_BOOLEAN__OBJECT_OBJECT,
                  G_TYPE_BOOLEAN,
                  2,
                  G_TYPE_DBUS_INTERFACE_STUB,
                  G_TYPE_DBUS_METHOD_INVOCATION);

  g_type_class_add_private (klass, sizeof (GDBusObjectStubPrivate));
}

static void
g_dbus_object_stub_init (GDBusObjectStub *object)
{
  object->priv = G_TYPE_INSTANCE_GET_PRIVATE (object, G_TYPE_DBUS_OBJECT_STUB, GDBusObjectStubPrivate);
  object->priv->map_name_to_iface = g_hash_table_new_full (g_str_hash,
                                                           g_str_equal,
                                                           g_free,
                                                           (GDestroyNotify) g_object_unref);
}

/**
 * g_dbus_object_stub_new:
 * @object_path: An object path.
 *
 * Creates a new #GDBusObjectStub.
 *
 * Returns: A #GDBusObjectStub. Free with g_object_unref().
 *
 * Since: 2.30
 */
GDBusObjectStub *
g_dbus_object_stub_new (const gchar *object_path)
{
  g_return_val_if_fail (g_variant_is_object_path (object_path), NULL);
  return G_DBUS_OBJECT_STUB (g_object_new (G_TYPE_DBUS_OBJECT_STUB,
                                           "object-path", object_path,
                                           NULL));
}

/**
 * g_dbus_object_stub_set_object_path:
 * @object: A #GDBusObjectStub.
 * @object_path: A valid D-Bus object path.
 *
 * Sets the object path for @object.
 *
 * Since: 2.30
 */
void
g_dbus_object_stub_set_object_path (GDBusObjectStub *object,
                                    const gchar     *object_path)
{
  g_return_if_fail (G_IS_DBUS_OBJECT_STUB (object));
  g_return_if_fail (object_path == NULL || g_variant_is_object_path (object_path));
  /* TODO: fail if object is currently exported */
  if (g_strcmp0 (object->priv->object_path, object_path) != 0)
    {
      g_free (object->priv->object_path);
      object->priv->object_path = g_strdup (object_path);
      g_object_notify (G_OBJECT (object), "object-path");
    }
}

static const gchar *
g_dbus_object_stub_get_object_path (GDBusObject *_object)
{
  GDBusObjectStub *object = G_DBUS_OBJECT_STUB (_object);
  return object->priv->object_path;
}

/**
 * g_dbus_object_stub_add_interface:
 * @object: A #GDBusObjectStub.
 * @interface_: A #GDBusInterfaceStub.
 *
 * Adds @interface_ to @object.
 *
 * If @object already contains a #GDBusInterfaceStub with the same
 * interface name, it is removed before @interface_ is added.
 *
 * Note that @object takes its own reference on @interface_ and holds
 * it until removed.
 *
 * Since: 2.30
 */
void
g_dbus_object_stub_add_interface (GDBusObjectStub     *object,
                                  GDBusInterfaceStub  *interface_)
{
  GDBusInterfaceInfo *info;

  g_return_if_fail (G_IS_DBUS_OBJECT_STUB (object));
  g_return_if_fail (G_IS_DBUS_INTERFACE_STUB (interface_));

  info = g_dbus_interface_stub_get_info (interface_);
  g_object_ref (interface_);
  g_dbus_object_stub_remove_interface_by_name (object, info->name);
  g_hash_table_insert (object->priv->map_name_to_iface,
                       g_strdup (info->name),
                       interface_);
  g_dbus_interface_set_object (G_DBUS_INTERFACE (interface_), G_DBUS_OBJECT (object));
  g_signal_emit_by_name (object,
                         "interface-added",
                         interface_);
}

/**
 * g_dbus_object_stub_remove_interface:
 * @object: A #GDBusObjectStub.
 * @interface_: A #GDBusInterfaceStub.
 *
 * Removes @interface_ from @object.
 *
 * Since: 2.30
 */
void
g_dbus_object_stub_remove_interface  (GDBusObjectStub    *object,
                                      GDBusInterfaceStub *interface_)
{
  GDBusInterfaceStub *other_interface;
  GDBusInterfaceInfo *info;

  g_return_if_fail (G_IS_DBUS_OBJECT_STUB (object));
  g_return_if_fail (G_IS_DBUS_INTERFACE (interface_));

  info = g_dbus_interface_stub_get_info (interface_);

  other_interface = g_hash_table_lookup (object->priv->map_name_to_iface, info->name);
  if (other_interface == NULL)
    {
      g_warning ("Tried to remove interface with name %s from object "
                 "at path %s but no such interface exists",
                 info->name,
                 object->priv->object_path);
    }
  else if (other_interface != interface_)
    {
      g_warning ("Tried to remove interface %p with name %s from object "
                 "at path %s but the object has the interface %p",
                 interface_,
                 info->name,
                 object->priv->object_path,
                 other_interface);
    }
  else
    {
      g_object_ref (interface_);
      g_warn_if_fail (g_hash_table_remove (object->priv->map_name_to_iface, info->name));
      g_dbus_interface_set_object (G_DBUS_INTERFACE (interface_), NULL);
      g_signal_emit_by_name (object,
                             "interface-removed",
                             interface_);
      g_object_unref (interface_);
    }
}


/**
 * g_dbus_object_stub_remove_interface_by_name:
 * @object: A #GDBusObjectStub.
 * @interface_name: A D-Bus interface name.
 *
 * Removes the #GDBusInterface with @interface_name from @object.
 *
 * If no D-Bus interface of the given interface exists, this function
 * does nothing.
 *
 * Since: 2.30
 */
void
g_dbus_object_stub_remove_interface_by_name (GDBusObjectStub *object,
                                             const gchar     *interface_name)
{
  GDBusInterface *interface;

  g_return_if_fail (G_IS_DBUS_OBJECT_STUB (object));
  g_return_if_fail (g_dbus_is_interface_name (interface_name));

  interface = g_hash_table_lookup (object->priv->map_name_to_iface, interface_name);
  if (interface != NULL)
    {
      g_object_ref (interface);
      g_warn_if_fail (g_hash_table_remove (object->priv->map_name_to_iface, interface_name));
      g_dbus_interface_set_object (interface, NULL);
      g_signal_emit_by_name (object,
                             "interface-removed",
                             interface);
      g_object_unref (interface);
    }
}

static GDBusInterface *
g_dbus_object_stub_get_interface (GDBusObject *_object,
                                  const gchar  *interface_name)
{
  GDBusObjectStub *object = G_DBUS_OBJECT_STUB (_object);
  GDBusInterface *ret;

  g_return_val_if_fail (G_IS_DBUS_OBJECT_STUB (object), NULL);
  g_return_val_if_fail (g_dbus_is_interface_name (interface_name), NULL);

  ret = g_hash_table_lookup (object->priv->map_name_to_iface, interface_name);
  if (ret != NULL)
    g_object_ref (ret);
  return ret;
}

static GList *
g_dbus_object_stub_get_interfaces (GDBusObject *_object)
{
  GDBusObjectStub *object = G_DBUS_OBJECT_STUB (_object);
  GList *ret;
  GHashTableIter iter;
  GDBusInterface *interface;

  g_return_val_if_fail (G_IS_DBUS_OBJECT_STUB (object), NULL);

  ret = NULL;

  g_hash_table_iter_init (&iter, object->priv->map_name_to_iface);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer) &interface))
    ret = g_list_prepend (ret, g_object_ref (interface));

  return ret;
}

/**
 * g_dbus_object_stub_flush:
 * @object: A #GDBusObjectStub.
 *
 * This method simply calls g_dbus_interface_stub_flush() on all
 * interfaces stubs belonging to @object. See that method for when
 * flushing is useful.
 *
 * Since: 2.30
 */
void
g_dbus_object_stub_flush (GDBusObjectStub *object)
{
  GHashTableIter iter;
  GDBusInterfaceStub *interface_stub;

  g_hash_table_iter_init (&iter, object->priv->map_name_to_iface);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer) &interface_stub))
    {
      g_dbus_interface_stub_flush (interface_stub);
    }
}

static gpointer
g_dbus_object_stub_lookup_with_typecheck (GDBusObject *object,
                                          const gchar *interface_name,
                                          GType        type)
{
  GDBusObjectStub *stub = G_DBUS_OBJECT_STUB (object);
  GDBusProxy *ret;

  ret = g_hash_table_lookup (stub->priv->map_name_to_iface, interface_name);
  if (ret != NULL)
    {
      g_warn_if_fail (G_TYPE_CHECK_INSTANCE_TYPE (ret, type));
      g_object_ref (ret);
    }
  return ret;
}

static gpointer
g_dbus_object_stub_peek_with_typecheck   (GDBusObject *object,
                                          const gchar *interface_name,
                                          GType        type)
{
  GDBusInterfaceStub *ret;
  ret = g_dbus_object_stub_lookup_with_typecheck (object, interface_name, type);
  if (ret != NULL)
    g_object_unref (ret);
  return ret;
}

static void
dbus_object_interface_init (GDBusObjectIface *iface)
{
  iface->get_object_path = g_dbus_object_stub_get_object_path;
  iface->get_interfaces  = g_dbus_object_stub_get_interfaces;
  iface->get_interface  = g_dbus_object_stub_get_interface;
  iface->lookup_with_typecheck = g_dbus_object_stub_lookup_with_typecheck;
  iface->peek_with_typecheck = g_dbus_object_stub_peek_with_typecheck;
}

gboolean
_g_dbus_object_stub_has_authorize_method_handlers (GDBusObjectStub *stub)
{
  gboolean has_handlers;
  gboolean has_default_class_handler;

  has_handlers = g_signal_has_handler_pending (stub,
                                               signals[AUTHORIZE_METHOD_SIGNAL],
                                               0,
                                               TRUE);
  has_default_class_handler = (G_DBUS_OBJECT_STUB_GET_CLASS (stub)->authorize_method ==
                               g_dbus_object_stub_authorize_method_default);

  return has_handlers || !has_default_class_handler;
}
