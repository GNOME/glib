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

#include "glocalemonitor.h"
#include "gsettings.h"

/**
 * SECTION:glocalemonitor
 * @title GLocaleMonitor
 * @short_description: Monitor locale settings
 *
 * #GLocaleMonitor is a utility class to monitor the locale settings for
 * changes (ie: in response to the user manually changing the locale).
 *
 * You must use this class in order for your program to notice changes to
 * the locale settings (language, numbers and dates formats, etc). It works
 * by monitoring the settings stored under org.gnome.system.locale. When any
 * of the settings are changed, the "changed" signal is emitted, so that
 * applications can listen to this signal and change the language of the
 * messages shown in the application or the format of the dates
 * and numbers being displayed in the application UI.
 *
 * When displaying formatted numbers, you should use the printf-style
 * formatting in functions like printf, #g_strdup_printf, etc. For dates,
 * use #g_date_strftime and/or #g_date_time_format_string with the correct
 * string format used to represent dates and times with the current locale.
 */

/**
 * GLocaleMonitor:
 *
 * This is an opaque structure type.
 **/

typedef GObjectClass GLocaleMonitorClass;

struct _GLocaleMonitor
{
  GObject parent;

  GSettings *locale_settings;
};

G_DEFINE_TYPE(GLocaleMonitor, g_locale_monitor, G_TYPE_OBJECT)

static guint g_locale_monitor_changed_signal;

static void
g_locale_monitor_finalize (GObject *object)
{
  g_assert_not_reached ();
}

static void
locale_settings_changed (GSettings   *settings,
			 const gchar *key,
			 gpointer     user_data)
{
  GLocaleMonitor *monitor = G_LOCALE_MONITOR (user_data);

  if (g_str_equal (key, "region"))
    {
      /* FIXME: call setlocale here? */
      g_signal_emit (monitor, g_locale_monitor_changed_signal, 0);
    }
}

static void
g_locale_monitor_init (GLocaleMonitor *monitor)
{
  monitor->locale_settings = g_settings_new ("org.gnome.system.locale");
  g_signal_connect (G_OBJECT (monitor->locale_settings), "changed",
		    G_CALLBACK (locale_settings_changed), monitor);
}

static void
g_locale_monitor_class_init (GLocaleMonitorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = g_locale_monitor_finalize;

  /**
   * GLocaleMonitor::changed
   * @monitor: the #GLocaleMonitor
   *
   * Indicates that the locale settings have changed.
   *
   **/
  g_locale_monitor_changed_signal =
    g_signal_new ("changed", G_TYPE_LOCALE_MONITOR,
                  G_SIGNAL_RUN_FIRST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

/**
 * g_locale_monitor_get:
 *
 * Gets the singleton instance of the #GLocaleMonitor class, creating
 * it if required.
 *
 * You should call #g_object_unref() on the result when you no longer
 * need it. Be aware, though, that this will not destroy the instance,
 * so if you connected to the changed signal, you are required to
 * disconnect from it for yourself.
 *
 * There is only one instance of #LocaleMonitor and it dispatches its
 * signals via the default #GMainContext.  There is no way to create an
 * instance that will dispatch signals using a different context.
 *
 * Returns: a reference to the #GLocaleMonitor.
 */
GLocaleMonitor *
g_locale_monitor_get (void)
{
  static gsize instance;

  if (g_once_init_enter (&instance))
    {
      GLocaleMonitor *monitor;

      monitor = g_object_new (G_TYPE_LOCALE_MONITOR, NULL);

      g_once_init_leave (&instance, (gsize) monitor);
    }

  return g_object_ref ((void *) instance);
}
