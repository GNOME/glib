/*
 * Copyright © 2011 Rodrigo Moya
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
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
 * Authors: Rodrigo Moya <rodrigo@gnome.org>
 */

#if !defined (__GIO_GIO_H_INSIDE__) && !defined (GIO_COMPILATION)
#error "Only <gio/gio.h> can be included directly."
#endif

#ifndef __G_LOCALE_MONITOR_H__
#define __G_LOCALE_MONITOR_H__

#include "gactiongroup.h"

G_BEGIN_DECLS

#define G_TYPE_LOCALE_MONITOR                          (g_locale_monitor_get_type ())
#define G_LOCALE_MONITOR(inst)                         (G_TYPE_CHECK_INSTANCE_CAST ((inst), G_TYPE_LOCALE_MONITOR, GLocaleMonitor))
#define G_IS_LOCALE_MONITOR(inst)                      (G_TYPE_CHECK_INSTANCE_TYPE ((inst), G_TYPE_LOCALE_MONITOR))

typedef struct _GLocaleMonitor      GLocaleMonitor;

GType           g_locale_monitor_get_type (void) G_GNUC_CONST;
GLocaleMonitor *g_locale_monitor_get      (void);

G_END_DECLS

#endif /* __G_LOCALE_MONITOR_H__ */
