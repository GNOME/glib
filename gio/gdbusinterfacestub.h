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

#ifndef __G_DBUS_INTERFACE_STUB_H__
#define __G_DBUS_INTERFACE_STUB_H__

#include <gio/giotypes.h>

G_BEGIN_DECLS

#define G_TYPE_DBUS_INTERFACE_STUB         (g_dbus_interface_stub_get_type ())
#define G_DBUS_INTERFACE_STUB(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_DBUS_INTERFACE_STUB, GDBusInterfaceStub))
#define G_DBUS_INTERFACE_STUB_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_DBUS_INTERFACE_STUB, GDBusInterfaceStubClass))
#define G_DBUS_INTERFACE_STUB_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_DBUS_INTERFACE_STUB, GDBusInterfaceStubClass))
#define G_IS_DBUS_INTERFACE_STUB(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_DBUS_INTERFACE_STUB))
#define G_IS_DBUS_INTERFACE_STUB_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_DBUS_INTERFACE_STUB))

typedef struct _GDBusInterfaceStubClass   GDBusInterfaceStubClass;
typedef struct _GDBusInterfaceStubPrivate GDBusInterfaceStubPrivate;

/**
 * GDBusInterfaceStub:
 *
 * The #GDBusInterfaceStub structure contains private data and should
 * only be accessed using the provided API.
 *
 * Since: 2.30
 */
struct _GDBusInterfaceStub
{
  /*< private >*/
  GObject parent_instance;
  GDBusInterfaceStubPrivate *priv;
};

/**
 * GDBusInterfaceStubClass:
 * @parent_class: The parent class.
 * @get_info: Returns a #GDBusInterfaceInfo. See g_dbus_interface_stub_get_info() for details.
 * @get_vtable: Returns a #GDBusInterfaceVTable. See g_dbus_interface_stub_get_vtable() for details.
 * @get_properties: Returns a new, floating, #GVariant with all properties. See g_dbus_interface_stub_get_properties().
 * @flush: Emits outstanding changes, if any. See g_dbus_interface_stub_flush().
 * @g_authorize_method: Signal class handler for the #GDBusInterfaceStub::g-authorize-method signal.
 *
 * Class structure for #GDBusInterfaceStub.
 *
 * Since: 2.30
 */
struct _GDBusInterfaceStubClass
{
  GObjectClass parent_class;

  /* Virtual Functions */
  GDBusInterfaceInfo   *(*get_info)       (GDBusInterfaceStub  *stub);
  GDBusInterfaceVTable *(*get_vtable)     (GDBusInterfaceStub  *stub);
  GVariant             *(*get_properties) (GDBusInterfaceStub  *stub);
  void                  (*flush)          (GDBusInterfaceStub  *stub);

  /*< private >*/
  gpointer vfunc_padding[8];
  /*< public >*/

  /* Signals */
  gboolean (*g_authorize_method) (GDBusInterfaceStub    *stub,
                                  GDBusMethodInvocation *invocation);

  /*< private >*/
  gpointer signal_padding[8];
};

GType                    g_dbus_interface_stub_get_type       (void) G_GNUC_CONST;
GDBusInterfaceStubFlags  g_dbus_interface_stub_get_flags      (GDBusInterfaceStub      *stub);
void                     g_dbus_interface_stub_set_flags      (GDBusInterfaceStub      *stub,
                                                               GDBusInterfaceStubFlags  flags);
GDBusInterfaceInfo      *g_dbus_interface_stub_get_info       (GDBusInterfaceStub      *stub);
GDBusInterfaceVTable    *g_dbus_interface_stub_get_vtable     (GDBusInterfaceStub      *stub);
GVariant                *g_dbus_interface_stub_get_properties (GDBusInterfaceStub      *stub);
void                     g_dbus_interface_stub_flush          (GDBusInterfaceStub      *stub);

gboolean                 g_dbus_interface_stub_export          (GDBusInterfaceStub      *stub,
                                                                GDBusConnection         *connection,
                                                                const gchar             *object_path,
                                                                GError                 **error);
void                     g_dbus_interface_stub_unexport        (GDBusInterfaceStub      *stub);
GDBusConnection         *g_dbus_interface_stub_get_connection  (GDBusInterfaceStub      *stub);
const gchar             *g_dbus_interface_stub_get_object_path (GDBusInterfaceStub      *stub);

G_END_DECLS

#endif /* __G_DBUS_INTERFACE_STUB_H */
