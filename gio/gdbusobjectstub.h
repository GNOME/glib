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

#ifndef __G_DBUS_OBJECT_STUB_H__
#define __G_DBUS_OBJECT_STUB_H__

#include <gio/giotypes.h>

G_BEGIN_DECLS

#define G_TYPE_DBUS_OBJECT_STUB         (g_dbus_object_stub_get_type ())
#define G_DBUS_OBJECT_STUB(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_DBUS_OBJECT_STUB, GDBusObjectStub))
#define G_DBUS_OBJECT_STUB_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_DBUS_OBJECT_STUB, GDBusObjectStubClass))
#define G_DBUS_OBJECT_STUB_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_DBUS_OBJECT_STUB, GDBusObjectStubClass))
#define G_IS_DBUS_OBJECT_STUB(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_DBUS_OBJECT_STUB))
#define G_IS_DBUS_OBJECT_STUB_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_DBUS_OBJECT_STUB))

typedef struct _GDBusObjectStubClass   GDBusObjectStubClass;
typedef struct _GDBusObjectStubPrivate GDBusObjectStubPrivate;

/**
 * GDBusObjectStub:
 *
 * The #GDBusObjectStub structure contains private data and should only be
 * accessed using the provided API.
 *
 * Since: 2.30
 */
struct _GDBusObjectStub
{
  /*< private >*/
  GObject parent_instance;
  GDBusObjectStubPrivate *priv;
};

/**
 * GDBusObjectStubClass:
 * @parent_class: The parent class.
 * @authorize_method: Signal class handler for the #GDBusObjectStub::authorize-method signal.
 *
 * Class structure for #GDBusObjectStub.
 *
 * Since: 2.30
 */
struct _GDBusObjectStubClass
{
  GObjectClass parent_class;

  /* Signals */
  gboolean (*authorize_method) (GDBusObjectStub       *stub,
                                GDBusInterfaceStub    *interface_stub,
                                GDBusMethodInvocation *invocation);

  /*< private >*/
  gpointer padding[8];
};

GType                g_dbus_object_stub_get_type                  (void) G_GNUC_CONST;
GDBusObjectStub     *g_dbus_object_stub_new                       (const gchar        *object_path);
void                 g_dbus_object_stub_flush                     (GDBusObjectStub    *object);
void                 g_dbus_object_stub_add_interface             (GDBusObjectStub    *object,
                                                                   GDBusInterfaceStub *interface_);
void                 g_dbus_object_stub_remove_interface          (GDBusObjectStub    *object,
                                                                   GDBusInterfaceStub *interface_);
void                 g_dbus_object_stub_remove_interface_by_name  (GDBusObjectStub    *object,
                                                                   const gchar        *interface_name);
void                 g_dbus_object_stub_set_object_path           (GDBusObjectStub    *object,
                                                                   const gchar        *object_path);

G_END_DECLS

#endif /* __G_DBUS_OBJECT_STUB_H */
