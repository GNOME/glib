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
#include "gdbusinterface.h"
#include "gdbusutils.h"
#include "gio-marshal.h"

#include "glibintl.h"

/**
 * SECTION:gdbusobject
 * @short_description: Base type for D-Bus objects
 * @include: gio/gio.h
 *
 * The #GDBusObject type is the base type for D-Bus objects on both
 * the service side (see #GDBusObjectStub) and the client side (see
 * #GDBusObjectProxy). It is essentially just a container of
 * interfaces.
 */

typedef GDBusObjectIface GDBusObjectInterface;
G_DEFINE_INTERFACE (GDBusObject, g_dbus_object, G_TYPE_OBJECT)

static void
g_dbus_object_default_init (GDBusObjectIface *iface)
{
  /**
   * GDBusObject::interface-added:
   * @object: The #GDBusObject emitting the signal.
   * @interface: The #GDBusInterface that was added.
   *
   * Emitted when @interface is added to @object.
   *
   * Since: 2.30
   */
  g_signal_new ("interface-added",
                G_TYPE_FROM_INTERFACE (iface),
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (GDBusObjectIface, interface_added),
                NULL,
                NULL,
                g_cclosure_marshal_VOID__OBJECT,
                G_TYPE_NONE,
                1,
                G_TYPE_DBUS_INTERFACE);

  /**
   * GDBusObject::interface-removed:
   * @object: The #GDBusObject emitting the signal.
   * @interface: The #GDBusInterface that was removed.
   *
   * Emitted when @interface is removed from @object.
   *
   * Since: 2.30
   */
  g_signal_new ("interface-removed",
                G_TYPE_FROM_INTERFACE (iface),
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (GDBusObjectIface, interface_removed),
                NULL,
                NULL,
                g_cclosure_marshal_VOID__OBJECT,
                G_TYPE_NONE,
                1,
                G_TYPE_DBUS_INTERFACE);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_object_get_object_path:
 * @object: A #GDBusObject.
 *
 * Gets the object path for @object.
 *
 * Returns: A string owned by @object. Do not free.
 *
 * Since: 2.30
 */
const gchar *
g_dbus_object_get_object_path (GDBusObject *object)
{
  GDBusObjectIface *iface = G_DBUS_OBJECT_GET_IFACE (object);
  return iface->get_object_path (object);
}

/**
 * g_dbus_object_get_interfaces:
 * @object: A #GDBusObject.
 *
 * Gets the D-Bus interfaces associated with @object.
 *
 * Returns: (element-type GDBusInterface) (transfer full) : A list of #GDBusInterface instances.
 *   The returned list must be freed by g_list_free() after each element has been freed
 *   with g_object_unref().
 *
 * Since: 2.30
 */
GList *
g_dbus_object_get_interfaces (GDBusObject *object)
{
  GDBusObjectIface *iface = G_DBUS_OBJECT_GET_IFACE (object);
  return iface->get_interfaces (object);
}

/**
 * g_dbus_object_get_interface:
 * @object: A #GDBusObject.
 * @interface_name: A D-Bus interface name.
 *
 * Gets the D-Bus interface with name @interface_name associated with
 * @object, if any.
 *
 * Returns: %NULL if not found, otherwise a #GDBusInterface that must
 *   be freed with g_object_unref().
 *
 * Since: 2.30
 */
GDBusInterface *
g_dbus_object_get_interface (GDBusObject *object,
                             const gchar *interface_name)
{
  GDBusObjectIface *iface = G_DBUS_OBJECT_GET_IFACE (object);
  g_return_val_if_fail (g_dbus_is_interface_name (interface_name), NULL);
  return iface->get_interface (object, interface_name);
}


/**
 * g_dbus_object_peek_with_typecheck:
 * @object: A #GDBusObject.
 * @interface_name: A D-Bus interface name.
 * @type: The #GType that the returned object must conform to.
 *
 * Like g_dbus_object_lookup_with_typecheck() except that the caller
 * does not own a reference to the returned object.
 *
 * <note><para>This function is intended to only be used in type
 * implementations.</para></note>
 *
 * Returns: A #GDBusInterface implementing @type or %NULL if
 * not found. Do not free the returned object, it is owned by
 * @object.
 *
 * Since: 2.30
 */
gpointer
g_dbus_object_peek_with_typecheck (GDBusObject *object,
                                   const gchar *interface_name,
                                   GType        type)
{
  GDBusObjectIface *iface = G_DBUS_OBJECT_GET_IFACE (object);
  g_return_val_if_fail (g_dbus_is_interface_name (interface_name), NULL);
  return iface->peek_with_typecheck (object, interface_name, type);
}

/**
 * g_dbus_object_lookup_with_typecheck:
 * @object: A #GDBusObject.
 * @interface_name: A D-Bus interface name.
 * @type: The #GType that the returned object must conform to.
 *
 * Like g_dbus_object_get_interface() but warns on stderr if the
 * returned object, if any, does not conform to @type.
 *
 * <note><para>This function is intended to only be used in type
 * implementations.</para></note>
 *
 * Returns: A #GDBusInterface implementing @type or %NULL if
 * not found. Free with g_object_unref().
 *
 * Since: 2.30
 */
gpointer
g_dbus_object_lookup_with_typecheck (GDBusObject *object,
                                     const gchar *interface_name,
                                     GType        type)
{
  GDBusObjectIface *iface = G_DBUS_OBJECT_GET_IFACE (object);
  g_return_val_if_fail (g_dbus_is_interface_name (interface_name), NULL);
  return iface->lookup_with_typecheck (object, interface_name, type);
}

