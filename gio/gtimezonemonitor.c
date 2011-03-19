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

#include "gtimezonemonitor.h"
#include "gfile.h"

/**
 * SECTION:gtimezonemonitor
 * @title: GTimeZoneMonitor
 * @short_description: Monitor the local timezone
 *
 * #GTimeZoneMonitor is a utility class to monitor the local timezone for
 * changes (ie: in response to the user manually changing the timezone
 * to that of a different locale).
 *
 * You must use this class in order for your program to notice changes
 * to the local timezone.  It works by monitoring the /etc/localtime
 * file.  When the timezone is found to have changed,
 * g_time_zone_refresh_local() is called and the "changed" signal is
 * emitted on the #GTimeZoneMonitor (in that order).
 *
 * Windows support is not presently working.
 **/

/**
 * GTimeZoneMonitor:
 *
 * This is an opaque structure type.
 **/

typedef GObjectClass GTimeZoneMonitorClass;

struct _GTimeZoneMonitor
{
  GObject parent_instance;

  GFileMonitor *monitor;
};

G_DEFINE_TYPE (GTimeZoneMonitor, g_time_zone_monitor, G_TYPE_OBJECT)

static guint g_time_zone_monitor_changed_signal;

static void
etc_localtime_changed (GFileMonitor      *monitor,
                       GFile             *file,
                       GFile             *other_file,
                       GFileMonitorEvent  event_type,
                       gpointer           user_data)
{
  GTimeZoneMonitor *tzm = user_data;

  if (event_type != G_FILE_MONITOR_EVENT_CREATED)
    return;

  g_time_zone_refresh_local ();

  g_signal_emit (tzm, g_time_zone_monitor_changed_signal, 0);
}

static void
g_time_zone_monitor_finalize (GObject *object)
{
  g_assert_not_reached ();
}

static void
g_time_zone_monitor_init (GTimeZoneMonitor *tzm)
{
  GFile *etc_localtime;

  etc_localtime = g_file_new_for_path ("/etc/localtime");
  tzm->monitor = g_file_monitor_file (etc_localtime, 0, NULL, NULL);
  g_object_unref (etc_localtime);

  g_signal_connect (tzm->monitor, "changed",
                    G_CALLBACK (etc_localtime_changed), tzm);
}

static void
g_time_zone_monitor_class_init (GTimeZoneMonitorClass *class)
{
  class->finalize = g_time_zone_monitor_finalize;

  /**
   * GTimeZoneMonitor::changed
   * @monitor: the #GTimeZoneMonitor
   *
   * Indicates that the local timezone has changed.
   *
   * The g_time_zone_refresh_local() function is called just before this
   * signal is emitted, so any new #GTimeZone or #GDateTime instances
   * created from signal handlers will be as per the new timezone.
   *
   * Note that this signal is not emitted in response to entering or
   * exiting daylight savings time within a given timezone.  It's only
   * for when the user has changed the timezone to that of a different
   * location.
   **/
  g_time_zone_monitor_changed_signal =
    g_signal_new ("changed", G_TYPE_TIME_ZONE_MONITOR,
                  G_SIGNAL_RUN_FIRST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

/**
 * g_time_zone_monitor_get:
 *
 * Gets the singleton instance of the #GTimeZoneMonitor class, creating
 * it if required.
 *
 * You should call g_object_unref() on the result when you no longer
 * need it.  Be aware, though, that this will not destroy the instance,
 * so if you connected to the changed signal, you are required to
 * disconnect from it for yourself.
 *
 * There is only one instance of #GTimeZoneMonitor and it dispatches its
 * signals via the default #GMainContext.  There is no way to create an
 * instance that will dispatch signals using a different context.
 *
 * Returns: a reference to the #GTimeZoneMonitor.
 **/
GTimeZoneMonitor *
g_time_zone_monitor_get (void)
{
  static gsize instance;

  if (g_once_init_enter (&instance))
    {
      GTimeZoneMonitor *monitor;

      monitor = g_object_new (G_TYPE_TIME_ZONE_MONITOR, NULL);

      g_once_init_leave (&instance, (gsize) monitor);
    }

  return g_object_ref ((void *) instance);
}
