/* gdatetime.h
 *
 * Copyright (C) 2009-2010 Christian Hergert <chris@dronelabs.com>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#if defined(G_DISABLE_SINGLE_INCLUDES) && !defined (__GLIB_H_INSIDE__) && !defined (GLIB_COMPILATION)
#error "Only <glib.h> can be included directly."
#endif

#ifndef __G_DATE_TIME_H__
#define __G_DATE_TIME_H__

#include <time.h>
#include <glib/gtypes.h>

G_BEGIN_DECLS

/**
 * G_TIME_SPAN_DAY:
 *
 * Evaluates to a time span of one day.
 *
 * Since: 2.26
 */
#define G_TIME_SPAN_DAY                 (G_GINT64_CONSTANT (86400000000))

/**
 * G_TIME_SPAN_HOUR:
 *
 * Evaluates to a time span of one hour.
 *
 * Since: 2.26
 */
#define G_TIME_SPAN_HOUR                (G_GINT64_CONSTANT (3600000000))

/**
 * G_TIME_SPAN_MINUTE:
 *
 * Evaluates to a time span of one minute.
 *
 * Since: 2.26
 */
#define G_TIME_SPAN_MINUTE              (G_GINT64_CONSTANT (60000000))

/**
 * G_TIME_SPAN_SECOND:
 *
 * Evaluates to a time span of one second.
 *
 * Since: 2.26
 */
#define G_TIME_SPAN_SECOND              (G_GINT64_CONSTANT (1000000))

/**
 * G_TIME_SPAN_MILLISECOND:
 *
 * Evaluates to a time span of one millisecond.
 *
 * Since: 2.26
 */
#define G_TIME_SPAN_MILLISECOND         (G_GINT64_CONSTANT (1000))

/**
 * GDateTime:
 *
 * <structname>GDateTime</structname> is an opaque structure whose members
 * cannot be accessed directly.
 *
 * Since: 2.26
 */
typedef struct _GDateTime GDateTime;

/**
 * GTimeSpan:
 *
 * A value representing an interval of time, in microseconds.
 *
 * Since: 2.26
 */
typedef gint64 GTimeSpan;

GDateTime *           g_date_time_new_now                (void);
GDateTime *           g_date_time_new_today              (void);
GDateTime *           g_date_time_new_utc_now            (void);
GDateTime *           g_date_time_new_from_date          (gint             year,
                                                          gint             month,
                                                          gint             day);
GDateTime *           g_date_time_new_from_epoch         (gint64           secs);
GDateTime *           g_date_time_new_from_timeval       (GTimeVal        *tv);
GDateTime *           g_date_time_new_full               (gint             year,
                                                          gint             month,
                                                          gint             day,
                                                          gint             hour,
                                                          gint             minute,
                                                          gint             second,
                                                          const gchar     *timezone);

GDateTime *           g_date_time_copy                   (const GDateTime *datetime);
GDateTime *           g_date_time_ref                    (GDateTime       *datetime);
void                  g_date_time_unref                  (GDateTime       *datetime);

GDateTime *           g_date_time_add                    (const GDateTime *datetime,
                                                          GTimeSpan        timespan);
GDateTime *           g_date_time_add_days               (const GDateTime *datetime,
                                                          gint             days);
GDateTime *           g_date_time_add_hours              (const GDateTime *datetime,
                                                          gint             hours);
GDateTime *           g_date_time_add_milliseconds       (const GDateTime *datetime,
                                                          gint             milliseconds);
GDateTime *           g_date_time_add_minutes            (const GDateTime *datetime,
                                                          gint             minutes);
GDateTime *           g_date_time_add_months             (const GDateTime *datetime,
                                                          gint             months);
GDateTime *           g_date_time_add_seconds            (const GDateTime *datetime,
                                                          gint             seconds);
GDateTime *           g_date_time_add_weeks              (const GDateTime *datetime,
                                                          gint             weeks);
GDateTime *           g_date_time_add_years              (const GDateTime *datetime,
                                                          gint             years);
GDateTime *           g_date_time_add_full               (const GDateTime *datetime,
                                                          gint             years,
                                                          gint             months,
                                                          gint             days,
                                                          gint             hours,
                                                          gint             minutes,
                                                          gint             seconds);

GDateTime *           g_date_time_day                    (const GDateTime  *datetime);

gint                  g_date_time_compare                (gconstpointer   dt1,
                                                          gconstpointer   dt2);
gboolean              g_date_time_equal                  (gconstpointer   dt1,
                                                          gconstpointer   dt2);
guint                 g_date_time_hash                   (gconstpointer   datetime);

GTimeSpan             g_date_time_difference             (const GDateTime *begin,
                                                          const GDateTime *end);

void                  g_date_time_get_julian             (const GDateTime *datetime,
                                                          gint            *period,
                                                          gint            *julian,
                                                          gint            *hour,
                                                          gint            *minute,
                                                          gint            *second);
gint                  g_date_time_get_hour               (const GDateTime *datetime);
gint                  g_date_time_get_minute             (const GDateTime *datetime);
gint                  g_date_time_get_second             (const GDateTime *datetime);
gint                  g_date_time_get_millisecond        (const GDateTime *datetime);
gint                  g_date_time_get_microsecond        (const GDateTime *datetime);
gint                  g_date_time_get_day_of_week        (const GDateTime *datetime);
gint                  g_date_time_get_day_of_month       (const GDateTime *datetime);
gint                  g_date_time_get_week_of_year       (const GDateTime *datetime);
gint                  g_date_time_get_day_of_year        (const GDateTime *datetime);
gint                  g_date_time_get_month              (const GDateTime *datetime);
gint                  g_date_time_get_year               (const GDateTime *datetime);
void                  g_date_time_get_dmy                (const GDateTime *datetime,
                                                          gint            *day,
                                                          gint            *month,
                                                          gint            *year);

GTimeSpan             g_date_time_get_utc_offset         (const GDateTime *datetime);
G_CONST_RETURN gchar *g_date_time_get_timezone_name      (const GDateTime *datetime);

gboolean              g_date_time_is_leap_year           (const GDateTime *datetime);
gboolean              g_date_time_is_daylight_savings    (const GDateTime *datetime);

GDateTime *           g_date_time_to_local               (const GDateTime *datetime);
gint64                g_date_time_to_epoch               (const GDateTime *datetime);
void                  g_date_time_to_timeval             (const GDateTime *datetime,
                                                          GTimeVal        *tv);
GDateTime *           g_date_time_to_utc                 (const GDateTime *datetime);
gchar *               g_date_time_printf                 (const GDateTime *datetime,
                                                          const gchar     *format) G_GNUC_MALLOC;

G_END_DECLS

#endif /* __G_DATE_TIME_H__ */
