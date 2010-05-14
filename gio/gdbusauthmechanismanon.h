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

#if !defined (GIO_COMPILATION)
#error "gdbusauthmechanismanon.h is a private header file."
#endif

#ifndef __G_DBUS_AUTH_MECHANISM_ANON_H__
#define __G_DBUS_AUTH_MECHANISM_ANON_H__

#include <gio/giotypes.h>
#include <gio/gdbusauthmechanism.h>

G_BEGIN_DECLS

#define G_TYPE_DBUS_AUTH_MECHANISM_ANON         (_g_dbus_auth_mechanism_anon_get_type ())
#define G_DBUS_AUTH_MECHANISM_ANON(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_DBUS_AUTH_MECHANISM_ANON, GDBusAuthMechanismAnon))
#define G_DBUS_AUTH_MECHANISM_ANON_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_DBUS_AUTH_MECHANISM_ANON, GDBusAuthMechanismAnonClass))
#define G_DBUS_AUTH_MECHANISM_ANON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_DBUS_AUTH_MECHANISM_ANON, GDBusAuthMechanismAnonClass))
#define G_IS_DBUS_AUTH_MECHANISM_ANON(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_DBUS_AUTH_MECHANISM_ANON))
#define G_IS_DBUS_AUTH_MECHANISM_ANON_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_DBUS_AUTH_MECHANISM_ANON))

typedef struct _GDBusAuthMechanismAnon        GDBusAuthMechanismAnon;
typedef struct _GDBusAuthMechanismAnonClass   GDBusAuthMechanismAnonClass;
typedef struct _GDBusAuthMechanismAnonPrivate GDBusAuthMechanismAnonPrivate;

struct _GDBusAuthMechanismAnonClass
{
  /*< private >*/
  GDBusAuthMechanismClass parent_class;

  /*< private >*/
  /* Padding for future expansion */
  void (*_g_reserved1) (void);
  void (*_g_reserved2) (void);
  void (*_g_reserved3) (void);
  void (*_g_reserved4) (void);
  void (*_g_reserved5) (void);
  void (*_g_reserved6) (void);
  void (*_g_reserved7) (void);
  void (*_g_reserved8) (void);
  void (*_g_reserved9) (void);
  void (*_g_reserved10) (void);
  void (*_g_reserved11) (void);
  void (*_g_reserved12) (void);
  void (*_g_reserved13) (void);
  void (*_g_reserved14) (void);
  void (*_g_reserved15) (void);
  void (*_g_reserved16) (void);
};

struct _GDBusAuthMechanismAnon
{
  GDBusAuthMechanism parent_instance;
  GDBusAuthMechanismAnonPrivate *priv;
};

GType _g_dbus_auth_mechanism_anon_get_type (void) G_GNUC_CONST;


G_END_DECLS

#endif /* __G_DBUS_AUTH_MECHANISM_ANON_H__ */
