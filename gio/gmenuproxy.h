/*
 * Copyright © 2011 Canonical Ltd.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#ifndef __G_MENU_PROXY_H__
#define __G_MENU_PROXY_H__

#include <gio/gdbusconnection.h>

G_BEGIN_DECLS

#define G_TYPE_MENU_PROXY     (g_menu_proxy_get_type ())
#define G_MENU_PROXY(inst)    (G_TYPE_CHECK_INSTANCE_CAST ((inst), \
                               G_TYPE_MENU_PROXY, GMenuProxy))
#define G_IS_MENU_PROXY(inst) (G_TYPE_CHECK_INSTANCE_TYPE ((inst), \
                               G_TYPE_MENU_PROXY))

typedef struct _GMenuProxy GMenuProxy;

GType        g_menu_proxy_get_type (void) G_GNUC_CONST;
GMenuProxy * g_menu_proxy_get      (GDBusConnection *connection,
                                    const gchar     *bus_name,
                                    const gchar     *object_path);

G_END_DECLS

#endif /* __G_MENU_PROXY_H__ */
