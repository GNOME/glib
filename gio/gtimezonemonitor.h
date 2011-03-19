/*
 * Copyright © 2011 Codethink Limited
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
 * Authors: Ryan Lortie <desrt@desrt.ca>
 */

#if !defined (__GIO_GIO_H_INSIDE__) && !defined (GIO_COMPILATION)
#error "Only <gio/gio.h> can be included directly."
#endif

#ifndef __G_TIME_ZONE_MONITOR_H__
#define __G_TIME_ZONE_MONITOR_H__

#include "gactiongroup.h"

G_BEGIN_DECLS

#define G_TYPE_TIME_ZONE_MONITOR                          (g_time_zone_monitor_get_type ())
#define G_TIME_ZONE_MONITOR(inst)                         (G_TYPE_CHECK_INSTANCE_CAST ((inst),                     \
                                                             G_TYPE_TIME_ZONE_MONITOR, GTimeZoneMonitor))
#define G_IS_TIME_ZONE_MONITOR(inst)                      (G_TYPE_CHECK_INSTANCE_TYPE ((inst),                     \
                                                             G_TYPE_TIME_ZONE_MONITOR))

typedef struct _GTimeZoneMonitor                          GTimeZoneMonitor;

GType                   g_time_zone_monitor_get_type                    (void) G_GNUC_CONST;

GTimeZoneMonitor *      g_time_zone_monitor_get                         (void);

G_END_DECLS

#endif /* __G_TIME_ZONE_MONITOR_H__ */
