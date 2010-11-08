/*
 * Copyright © 2010 Codethink Limited
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

#if !defined (__GIO_GIO_H_INSIDE__) && !defined (GIO_COMPILATION)
#error "Only <gio/gio.h> can be included directly."
#endif

#ifndef __G_PERIODIC_H__
#define __G_PERIODIC_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define G_TYPE_PERIODIC                                     (g_periodic_get_type ())
#define G_PERIODIC(inst)                                    (G_TYPE_CHECK_INSTANCE_CAST ((inst),                     \
                                                             G_TYPE_PERIODIC, GPeriodic))
#define G_IS_PERIODIC(inst)                                 (G_TYPE_CHECK_INSTANCE_TYPE ((inst), G_TYPE_PERIODIC))

typedef struct _GPeriodic                                   GPeriodic;

typedef void         (* GPeriodicTickFunc)                              (GPeriodic           *periodic,
                                                                         gint64               timestamp,
                                                                         gpointer             user_data);

GType                   g_periodic_get_type                             (void);
GPeriodic *             g_periodic_new                                  (guint                hz,
                                                                         gint                 high_priority,
                                                                         gint                 low_priority);
guint                   g_periodic_get_hz                               (GPeriodic           *periodic);
gint                    g_periodic_get_high_priority                    (GPeriodic           *periodic);
gint                    g_periodic_get_low_priority                     (GPeriodic           *periodic);

guint                   g_periodic_add                                  (GPeriodic           *periodic,
                                                                         GPeriodicTickFunc    callback,
                                                                         gpointer             user_data,
                                                                         GDestroyNotify       notify);
void                    g_periodic_remove                               (GPeriodic           *periodic,
                                                                         guint                tag);

void                    g_periodic_block                                (GPeriodic           *periodic);
void                    g_periodic_unblock                              (GPeriodic           *periodic,
                                                                         gint64               unblock_time);

void                    g_periodic_damaged                              (GPeriodic           *periodic);

G_END_DECLS

#endif /* __G_PERIODIC_H__ */
