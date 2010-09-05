/* gdatetime.c
 *
 * Copyright (C) 2009-2010 Christian Hergert <chris@dronelabs.com>
 * Copyright (C) 2010 Thiago Santos <thiago.sousa.santos@collabora.co.uk>
 * Copyright (C) 2010 Emmanuele Bassi <ebassi@linux.intel.com>
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

/* Algorithms within this file are based on the Calendar FAQ by
 * Claus Tondering.  It can be found at
 * http://www.tondering.dk/claus/cal/calendar29.txt
 *
 * Copyright and disclaimer
 * ------------------------
 *   This document is Copyright (C) 2008 by Claus Tondering.
 *   E-mail: claus@tondering.dk. (Please include the word
 *   "calendar" in the subject line.)
 *   The document may be freely distributed, provided this
 *   copyright notice is included and no money is charged for
 *   the document.
 *
 *   This document is provided "as is". No warranties are made as
 *   to its correctness.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef G_OS_WIN32
#include <sys/time.h>
#include <time.h>
#endif /* !G_OS_WIN32 */

#include "gdatetime.h"

#include "gatomic.h"
#include "gfileutils.h"
#include "gmain.h"
#include "gstrfuncs.h"
#include "gtestutils.h"
#include "glibintl.h"

/**
 * SECTION:date-time
 * @title: GDateTime
 * @short_description: A structure representing Date and Time
 *
 * #GDateTime is a structure that combines a date and time into a single
 * structure. It provides many conversion and methods to manipulate dates
 * and times. Time precision is provided down to microseconds.
 *
 * #GDateTime is an immutable object: once it has been created it cannot be
 * modified further. All modifiers will create a new #GDateTime.
 *
 * #GDateTime is reference counted: the reference count is increased by calling
 * g_date_time_ref() and decreased by calling g_date_time_unref(). When the
 * reference count drops to 0, the resources allocated by the #GDateTime
 * structure are released.
 *
 * Internally, #GDateTime uses the Proleptic Gregorian Calendar, the first
 * representable date is 0001-01-01. However, the public API uses the
 * internationally accepted Gregorian Calendar.
 *
 * #GDateTime is available since GLib 2.26.
 */

#define UNIX_EPOCH_START     719163

#define DAYS_IN_4YEARS    1461    /* days in 4 years */
#define DAYS_IN_100YEARS  36524   /* days in 100 years */
#define DAYS_IN_400YEARS  146097  /* days in 400 years  */

#define USEC_PER_SECOND      (G_GINT64_CONSTANT (1000000))
#define USEC_PER_MINUTE      (G_GINT64_CONSTANT (60000000))
#define USEC_PER_HOUR        (G_GINT64_CONSTANT (3600000000))
#define USEC_PER_MILLISECOND (G_GINT64_CONSTANT (1000))
#define USEC_PER_DAY         (G_GINT64_CONSTANT (86400000000))
#define SEC_PER_DAY          (G_GINT64_CONSTANT (86400))

#define GREGORIAN_LEAP(y)    ((((y) % 4) == 0) && (!((((y) % 100) == 0) && (((y) % 400) != 0))))
#define JULIAN_YEAR(d)       ((d)->julian / 365.25)
#define DAYS_PER_PERIOD      (G_GINT64_CONSTANT (2914695))

#define GET_AMPM(d,l)         ((g_date_time_get_hour (d) < 12)  \
                                ? (l ? C_("GDateTime", "am") : C_("GDateTime", "AM"))       \
                                : (l ? C_("GDateTime", "pm") : C_("GDateTime", "PM")))

#define WEEKDAY_ABBR(d)       (get_weekday_name_abbr (g_date_time_get_day_of_week (datetime)))
#define WEEKDAY_FULL(d)       (get_weekday_name (g_date_time_get_day_of_week (datetime)))

#define MONTH_ABBR(d)         (get_month_name_abbr (g_date_time_get_month (datetime)))
#define MONTH_FULL(d)         (get_month_name (g_date_time_get_month (datetime)))

/* Translators: this is the preferred format for expressing the date */
#define GET_PREFERRED_DATE(d) (g_date_time_printf ((d), C_("GDateTime", "%m/%d/%y")))

/* Translators: this is the preferred format for expressing the time */
#define GET_PREFERRED_TIME(d) (g_date_time_printf ((d), C_("GDateTime", "%H:%M:%S")))

typedef struct _GTimeZone       GTimeZone;

struct _GDateTime
{
  /* 1 is 0001-01-01 in Proleptic Gregorian */
  guint32 days;
  /* Microsecond timekeeping within Day */
  guint64 usec;

  /* TimeZone information, NULL is UTC */
  GTimeZone *tz;

  volatile gint ref_count;
};

struct _GTimeZone
{
  /* TZ abbreviation (e.g. PST) */
  gchar  *name;

  gint64 offset;

  guint is_dst : 1;
};

static const guint16 days_in_months[2][13] =
{
  { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
  { 0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

static const guint16 days_in_year[2][13] =
{
  {  0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
  {  0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
};

static const gchar *
get_month_name (gint month)
{
  switch (month)
    {
    case 1:
      return C_("GDateTime", "January");
    case 2:
      return C_("GDateTime", "February");
    case 3:
      return C_("GDateTime", "March");
    case 4:
      return C_("GDateTime", "April");
    case 5:
      return C_("GDateTime", "May");
    case 6:
      return C_("GDateTime", "June");
    case 7:
      return C_("GDateTime", "July");
    case 8:
      return C_("GDateTime", "August");
    case 9:
      return C_("GDateTime", "September");
    case 10:
      return C_("GDateTime", "October");
    case 11:
      return C_("GDateTime", "November");
    case 12:
      return C_("GDateTime", "December");

    default:
      g_warning ("Invalid month number %d", month);
    }

  return NULL;
}

static const gchar *
get_month_name_abbr (gint month)
{
  switch (month)
    {
    case 1:
      return C_("GDateTime", "Jan");
    case 2:
      return C_("GDateTime", "Feb");
    case 3:
      return C_("GDateTime", "Mar");
    case 4:
      return C_("GDateTime", "Apr");
    case 5:
      return C_("GDateTime", "May");
    case 6:
      return C_("GDateTime", "Jun");
    case 7:
      return C_("GDateTime", "Jul");
    case 8:
      return C_("GDateTime", "Aug");
    case 9:
      return C_("GDateTime", "Sep");
    case 10:
      return C_("GDateTime", "Oct");
    case 11:
      return C_("GDateTime", "Nov");
    case 12:
      return C_("GDateTime", "Dec");

    default:
      g_warning ("Invalid month number %d", month);
    }

  return NULL;
}

static const gchar *
get_weekday_name (gint day)
{
  switch (day)
    {
    case 1:
      return C_("GDateTime", "Monday");
    case 2:
      return C_("GDateTime", "Tuesday");
    case 3:
      return C_("GDateTime", "Wednesday");
    case 4:
      return C_("GDateTime", "Thursday");
    case 5:
      return C_("GDateTime", "Friday");
    case 6:
      return C_("GDateTime", "Saturday");
    case 7:
      return C_("GDateTime", "Sunday");

    default:
      g_warning ("Invalid week day number %d", day);
    }

  return NULL;
}

static const gchar *
get_weekday_name_abbr (gint day)
{
  switch (day)
    {
    case 1:
      return C_("GDateTime", "Mon");
    case 2:
      return C_("GDateTime", "Tue");
    case 3:
      return C_("GDateTime", "Wed");
    case 4:
      return C_("GDateTime", "Thu");
    case 5:
      return C_("GDateTime", "Fri");
    case 6:
      return C_("GDateTime", "Sat");
    case 7:
      return C_("GDateTime", "Sun");

    default:
      g_warning ("Invalid week day number %d", day);
    }

  return NULL;
}

static inline gint
date_to_proleptic_gregorian (gint year,
                gint month,
                gint day)
{
  gint64 days;

  days = (year - 1) * 365 + ((year - 1) / 4) - ((year - 1) / 100)
      + ((year - 1) / 400);

  days += days_in_year[0][month - 1];
  if (GREGORIAN_LEAP (year) && month > 2)
    day++;

  days += day;

  return days;
}

static inline void
g_date_time_add_days_internal (GDateTime *datetime,
                               gint64     days)
{
  datetime->days += days;
}

static inline void
g_date_time_add_usec (GDateTime *datetime,
                      gint64     usecs)
{
  gint64 u = datetime->usec + usecs;
  gint d = u / USEC_PER_DAY;

  if (u < 0)
    d -= 1;

  if (d != 0)
    g_date_time_add_days_internal (datetime, d);

  if (u < 0)
    datetime->usec = USEC_PER_DAY + (u % USEC_PER_DAY);
  else
    datetime->usec = u % USEC_PER_DAY;
}

/*< internal >
 * g_date_time_add_ymd:
 * @datetime: a #GDateTime
 * @years: years to add, in the Gregorian calendar
 * @months: months to add, in the Gregorian calendar
 * @days: days to add, in the Gregorian calendar
 *
 * Updates @datetime by adding @years, @months and @days to it
 *
 * This function modifies the passed #GDateTime so public accessors
 * should make always pass a copy
 */
static inline void
g_date_time_add_ymd (GDateTime *datetime,
                     gint       years,
                     gint       months,
                     gint       days)
{
  gint y = g_date_time_get_year (datetime);
  gint m = g_date_time_get_month (datetime);
  gint d = g_date_time_get_day_of_month (datetime);
  gint step, i;
  const guint16 *max_days;

  y += years;

  /* subtract one day for leap years */
  if (GREGORIAN_LEAP (y) && m == 2)
    {
      if (d == 29)
        d -= 1;
    }

  /* add months */
  step = months > 0 ? 1 : -1;
  for (i = 0; i < ABS (months); i++)
    {
      m += step;

      if (m < 1)
        {
          y -= 1;
          m = 12;
        }
      else if (m > 12)
        {
          y += 1;
          m = 1;
        }
    }

  /* clamp the days */
  max_days = days_in_months[GREGORIAN_LEAP (y) ? 1 : 0];
  if (max_days[m] < d)
    d = max_days[m];

  datetime->days = date_to_proleptic_gregorian (y, m, d);
  g_date_time_add_days_internal (datetime, days);
}

#define ZONEINFO_DIR            "zoneinfo"
#define TZ_MAGIC                "TZif"
#define TZ_MAGIC_LEN            (strlen (TZ_MAGIC))
#define TZ_HEADER_SIZE          44
#define TZ_TIMECNT_OFFSET       32
#define TZ_TYPECNT_OFFSET       36
#define TZ_TRANSITIONS_OFFSET   44

#define TZ_TTINFO_SIZE          6
#define TZ_TTINFO_GMTOFF_OFFSET 0
#define TZ_TTINFO_ISDST_OFFSET  4
#define TZ_TTINFO_NAME_OFFSET   5

static gchar *
get_tzdata_path (const gchar *tz_name)
{
  gchar *retval;

  if (tz_name != NULL)
    {
      const gchar *tz_dir = g_getenv ("TZDIR");

      if (tz_dir != NULL)
        retval = g_build_filename (tz_dir, tz_name, NULL);
      else
        retval = g_build_filename ("/", "usr", "share", ZONEINFO_DIR, tz_name, NULL);
    }
  else
    {
      /* an empty tz_name means "the current timezone file". tzset(3) defines
       * it to be /usr/share/zoneinfo/localtime, and it also allows an
       * /etc/localtime as a symlink to the localtime file under
       * /usr/share/zoneinfo or to the correct timezone file. Fedora does not
       * have /usr/share/zoneinfo/localtime, but it does have a real
       * /etc/localtime.
       *
       * in any case, this path should resolve correctly.
       */
      retval = g_build_filename ("/", "etc", "localtime", NULL);
    }

  return retval;
}

/*
 * Parses tzdata database times to get timezone info.
 *
 * @tz_name: Olson database name for the timezone
 * @start: Time offset from epoch we want to know the timezone
 * @_is_dst: Returns if this time in the timezone is in DST
 * @_offset: Returns the offset from UTC for this timezone
 * @_name: Returns the abreviated name for thie timezone
 */
static gboolean
parse_tzdata (const gchar  *tz_name,
              gint64        start,
              gboolean      is_utc,
	      gboolean     *_is_dst,
              gint64       *_offset,
              gchar       **_name)
{
  gchar *filename, *contents;
  gsize length;
  guint32 timecnt, typecnt;
  gint transitions_size, ttinfo_map_size;
  guint8 *ttinfo_map, *ttinfos;
  gint start_transition = -1;
  guint32 *transitions;
  gint32 offset;
  guint8 isdst;
  guint8 name_offset;
  gint i;
  GError *error;

  filename = get_tzdata_path (tz_name);

  /* XXX: should we be caching this in memory for faster access?
   * and if so, how do we expire the cache?
   */
  error = NULL;
  if (!g_file_get_contents (filename, &contents, &length, &error))
    {
      g_free (filename);
      return FALSE;
    }

  g_free (filename);

  if (length < TZ_HEADER_SIZE ||
      (strncmp (contents, TZ_MAGIC, TZ_MAGIC_LEN) != 0))
    {
      g_free (contents);
      return FALSE;
    }

  timecnt = GUINT32_FROM_BE (*(guint32 *)(contents + TZ_TIMECNT_OFFSET));
  typecnt = GUINT32_FROM_BE (*(guint32 *)(contents + TZ_TYPECNT_OFFSET));

  transitions = (guint32 *)(contents + TZ_TRANSITIONS_OFFSET);
  transitions_size = timecnt * sizeof (*transitions);
  ttinfo_map = (void *)(contents + TZ_TRANSITIONS_OFFSET + transitions_size);
  ttinfo_map_size = timecnt;
  ttinfos = (void *)(ttinfo_map + ttinfo_map_size);

  /*
   * Find the first transition that contains the 'start' time
   */
  for (i = 1; i < timecnt && start_transition == -1; i++)
    {
      gint32 transition_time = GINT32_FROM_BE (transitions[i]);

      /* if is_utc is not set, we need to add this time offset to compare with
       * start, because it is already on the timezone time */
      if (!is_utc)
        {
          gint32 off;

          off = *(gint32 *)(ttinfos + ttinfo_map[i] * TZ_TTINFO_SIZE +
                TZ_TTINFO_GMTOFF_OFFSET);
          off = GINT32_FROM_BE (off);

          transition_time += off;
        }

      if (transition_time > start)
        {
          start_transition = ttinfo_map[i - 1];
          break;
        }
    }

  if (start_transition == -1)
    {
      if (timecnt)
        start_transition = ttinfo_map[timecnt - 1];
      else
        start_transition = 0;
    }

  /* Copy the data out of the corresponding ttinfo structs */
  offset = *(gint32 *)(ttinfos + start_transition * TZ_TTINFO_SIZE +
           TZ_TTINFO_GMTOFF_OFFSET);
  offset = GINT32_FROM_BE (offset);
  isdst = *(ttinfos + start_transition * TZ_TTINFO_SIZE +
          TZ_TTINFO_ISDST_OFFSET);
  name_offset = *(ttinfos + start_transition * TZ_TTINFO_SIZE +
                TZ_TTINFO_NAME_OFFSET);

  if (_name != NULL)
    *_name = g_strdup ((gchar*) (ttinfos + TZ_TTINFO_SIZE * typecnt + name_offset));

  g_free (contents);

  if (_offset)
    *_offset = offset;

  if (_is_dst)
    *_is_dst = isdst;

  return TRUE;
}

/*< internal >
 * g_time_zone_new_from_epoc:
 * @tz_name: The Olson's database timezone name
 * @epoch: The epoch offset
 * @is_utc: If the @epoch is in UTC or already in the @tz_name timezone
 *
 * Creates a new timezone
 */
static GTimeZone *
g_time_zone_new_from_epoch (const gchar *tz_name,
                            gint64       epoch,
                            gboolean     is_utc)
{
  GTimeZone *tz = NULL;
  gint64 offset;
  gboolean is_dst;
  gchar *name = NULL;

  if (parse_tzdata (tz_name, epoch, is_utc, &is_dst, &offset, &name))
    {
      tz = g_slice_new (GTimeZone);
      tz->is_dst = is_dst;
      tz->offset = offset;
      tz->name = name;
    }

  return tz;
}

#define SECS_PER_MINUTE (60)
#define SECS_PER_HOUR   (60 * SECS_PER_MINUTE)
#define SECS_PER_DAY    (24 * SECS_PER_HOUR)
#define SECS_PER_YEAR   (365 * SECS_PER_DAY)
#define SECS_PER_JULIAN (DAYS_PER_PERIOD * SECS_PER_DAY)

static gint64
g_date_time_secs_offset (const GDateTime * dt)
{
  gint64 secs;
  gint d, y, h, m, s;
  gint i, leaps;

  y = g_date_time_get_year (dt);
  d = g_date_time_get_day_of_year (dt);
  h = g_date_time_get_hour (dt);
  m = g_date_time_get_minute (dt);
  s = g_date_time_get_second (dt);

  leaps = GREGORIAN_LEAP (y) ? 1 : 0;
  for (i = 1970; i < y; i++)
    {
      if (GREGORIAN_LEAP (i))
        leaps += 1;
    }

  secs = 0;
  secs += (y - 1970) * SECS_PER_YEAR;
  secs += d * SECS_PER_DAY;
  secs += (leaps - 1) * SECS_PER_DAY;
  secs += h * SECS_PER_HOUR;
  secs += m * SECS_PER_MINUTE;
  secs += s;

  if (dt->tz != NULL)
    secs -= dt->tz->offset;

  return secs;
}

/*< internal >
 * g_date_time_create_time_zone:
 * @dt: a #GDateTime
 * @tz_name: the name of the timezone
 *
 * Creates a timezone from a #GDateTime (disregarding its own timezone).
 * This function transforms the #GDateTime into seconds since the epoch
 * and creates a timezone for it in the @tz_name zone.
 *
 * Return value: a newly created #GTimeZone
 */
static GTimeZone *
g_date_time_create_time_zone (GDateTime   *dt,
                              const gchar *tz_name)
{
  gint64 secs;

  secs = g_date_time_secs_offset (dt);

  return g_time_zone_new_from_epoch (tz_name, secs, FALSE);
}

static GDateTime *
g_date_time_new (void)
{
  GDateTime *datetime;

  datetime = g_slice_new0 (GDateTime);
  datetime->ref_count = 1;

  return datetime;
}

static GTimeZone *
g_time_zone_copy (const GTimeZone *time_zone)
{
  GTimeZone *tz;

  if (G_UNLIKELY (time_zone == NULL))
    return NULL;

  tz = g_slice_new (GTimeZone);
  memcpy (tz, time_zone, sizeof (GTimeZone));

  tz->name = g_strdup (time_zone->name);

  return tz;
}

static void
g_time_zone_free (GTimeZone *time_zone)
{
  if (G_LIKELY (time_zone != NULL))
    {
      g_free (time_zone->name);
      g_slice_free (GTimeZone, time_zone);
    }
}

static void
g_date_time_free (GDateTime *datetime)
{
  if (G_UNLIKELY (datetime == NULL))
    return;

  if (datetime->tz)
    g_time_zone_free (datetime->tz);

  g_slice_free (GDateTime, datetime);
}

static void
g_date_time_get_week_number (const GDateTime *datetime,
                             gint            *week_number,
                             gint            *day_of_week,
                             gint            *day_of_year)
{
  gint a, b, c, d, e, f, g, n, s, month, day, year;

  g_date_time_get_dmy (datetime, &day, &month, &year);

  if (month <= 2)
    {
      a = g_date_time_get_year (datetime) - 1;
      b = (a / 4) - (a / 100) + (a / 400);
      c = ((a - 1) / 4) - ((a - 1) / 100) + ((a - 1) / 400);
      s = b - c;
      e = 0;
      f = day - 1 + (31 * (month - 1));
    }
  else
    {
      a = year;
      b = (a / 4) - (a / 100) + (a / 400);
      c = ((a - 1) / 4) - ((a - 1) / 100) + ((a - 1) / 400);
      s = b - c;
      e = s + 1;
      f = day + (((153 * (month - 3)) + 2) / 5) + 58 + s;
    }

  g = (a + b) % 7;
  d = (f + g - e) % 7;
  n = f + 3 - d;

  if (week_number)
    {
      if (n < 0)
        *week_number = 53 - ((g - s) / 5);
      else if (n > 364 + s)
        *week_number = 1;
      else
        *week_number = (n / 7) + 1;
    }

  if (day_of_week)
    *day_of_week = d + 1;

  if (day_of_year)
    *day_of_year = f + 1;
}

/**
 * g_date_time_add:
 * @datetime: a #GDateTime
 * @timespan: a #GTimeSpan
 *
 * Creates a copy of @datetime and adds the specified timespan to the copy.
 *
 * Return value: the newly created #GDateTime which should be freed with
 *   g_date_time_unref().
 *
 * Since: 2.26
 */
GDateTime*
g_date_time_add (const GDateTime *datetime,
                 GTimeSpan        timespan)
{
  GDateTime *dt;

  g_return_val_if_fail (datetime != NULL, NULL);

  dt = g_date_time_copy (datetime);
  g_date_time_add_usec (dt, timespan);

  return dt;
}

/**
 * g_date_time_add_years:
 * @datetime: a #GDateTime
 * @years: the number of years
 *
 * Creates a copy of @datetime and adds the specified number of years to the
 * copy.
 *
 * Return value: the newly created #GDateTime which should be freed with
 *   g_date_time_unref().
 *
 * Since: 2.26
 */
GDateTime*
g_date_time_add_years (const GDateTime *datetime,
                       gint             years)
{
  GDateTime *dt;

  g_return_val_if_fail (datetime != NULL, NULL);

  dt = g_date_time_copy (datetime);
  g_date_time_add_ymd (dt, years, 0, 0);

  return dt;
}

/**
 * g_date_time_add_months:
 * @datetime: a #GDateTime
 * @months: the number of months
 *
 * Creates a copy of @datetime and adds the specified number of months to the
 * copy.
 *
 * Return value: the newly created #GDateTime which should be freed with
 *   g_date_time_unref().
 *
 * Since: 2.26
 */
GDateTime*
g_date_time_add_months (const GDateTime *datetime,
                        gint             months)
{
  GDateTime *dt;

  g_return_val_if_fail (datetime != NULL, NULL);

  dt = g_date_time_copy (datetime);
  g_date_time_add_ymd (dt, 0, months, 0);

  return dt;
}

/**
 * g_date_time_add_weeks:
 * @datetime: a #GDateTime
 * @weeks: the number of weeks
 *
 * Creates a copy of @datetime and adds the specified number of weeks to the
 * copy.
 *
 * Return value: the newly created #GDateTime which should be freed with
 *   g_date_time_unref().
 *
 * Since: 2.26
 */
GDateTime*
g_date_time_add_weeks (const GDateTime *datetime,
                       gint             weeks)
{
  g_return_val_if_fail (datetime != NULL, NULL);

  return g_date_time_add_days (datetime, weeks * 7);
}

/**
 * g_date_time_add_days:
 * @datetime: a #GDateTime
 * @days: the number of days
 *
 * Creates a copy of @datetime and adds the specified number of days to the
 * copy.
 *
 * Return value: the newly created #GDateTime which should be freed with
 *   g_date_time_unref().
 *
 * Since: 2.26
 */
GDateTime*
g_date_time_add_days (const GDateTime *datetime,
                      gint             days)
{
  GDateTime *dt;

  g_return_val_if_fail (datetime != NULL, NULL);

  dt = g_date_time_copy (datetime);
  g_date_time_add_ymd (dt, 0, 0, days);

  return dt;
}

/**
 * g_date_time_add_hours:
 * @datetime: a #GDateTime
 * @hours: the number of hours to add
 *
 * Creates a copy of @datetime and adds the specified number of hours
 *
 * Return value: the newly created #GDateTime which should be freed with
 *   g_date_time_unref().
 *
 * Since: 2.26
 */
GDateTime*
g_date_time_add_hours (const GDateTime *datetime,
                       gint             hours)
{
  GDateTime *dt;

  g_return_val_if_fail (datetime != NULL, NULL);

  dt = g_date_time_copy (datetime);
  g_date_time_add_usec (dt, (gint64) hours * USEC_PER_HOUR);

  return dt;
}

/**
 * g_date_time_add_seconds:
 * @datetime: a #GDateTime
 * @seconds: the number of seconds to add
 *
 * Creates a copy of @datetime and adds the specified number of seconds.
 *
 * Return value: the newly created #GDateTime which should be freed with
 *   g_date_time_unref().
 *
 * Since: 2.26
 */
GDateTime*
g_date_time_add_seconds (const GDateTime *datetime,
                         gint             seconds)
{
  GDateTime *dt;

  g_return_val_if_fail (datetime != NULL, NULL);

  dt = g_date_time_copy (datetime);
  g_date_time_add_usec (dt, (gint64) seconds * USEC_PER_SECOND);

  return dt;
}

/**
 * g_date_time_add_milliseconds:
 * @datetime: a #GDateTime
 * @milliseconds: the number of milliseconds to add
 *
 * Creates a copy of @datetime adding the specified number of milliseconds.
 *
 * Return value: the newly created #GDateTime which should be freed with
 *   g_date_time_unref().
 *
 * Since: 2.26
 */
GDateTime*
g_date_time_add_milliseconds (const GDateTime *datetime,
                              gint             milliseconds)
{
  GDateTime *dt;

  g_return_val_if_fail (datetime != NULL, NULL);

  dt = g_date_time_copy (datetime);
  g_date_time_add_usec (dt, (gint64) milliseconds * USEC_PER_MILLISECOND);

  return dt;
}

/**
 * g_date_time_add_minutes:
 * @datetime: a #GDateTime
 * @minutes: the number of minutes to add
 *
 * Creates a copy of @datetime adding the specified number of minutes.
 *
 * Return value: the newly created #GDateTime which should be freed with
 *   g_date_time_unref().
 *
 * Since: 2.26
 */
GDateTime*
g_date_time_add_minutes (const GDateTime *datetime,
                         gint             minutes)
{
  GDateTime *dt;

  g_return_val_if_fail (datetime != NULL, NULL);

  dt = g_date_time_copy (datetime);
  g_date_time_add_usec (dt, (gint64) minutes * USEC_PER_MINUTE);

  return dt;
}

/**
 * g_date_time_add_full:
 * @datetime: a #GDateTime
 * @years: the number of years to add
 * @months: the number of months to add
 * @days: the number of days to add
 * @hours: the number of hours to add
 * @minutes: the number of minutes to add
 * @seconds: the number of seconds to add
 *
 * Creates a new #GDateTime adding the specified values to the current date and
 * time in @datetime.
 *
 * Return value: the newly created #GDateTime that should be freed with
 *   g_date_time_unref().
 *
 * Since: 2.26
 */
GDateTime *
g_date_time_add_full (const GDateTime *datetime,
                      gint             years,
                      gint             months,
                      gint             days,
                      gint             hours,
                      gint             minutes,
                      gint             seconds)
{
  GDateTime *dt;
  gint64 usecs;

  g_return_val_if_fail (datetime != NULL, NULL);

  dt = g_date_time_copy (datetime);

  /* add date */
  g_date_time_add_ymd (dt, years, months, days);

  /* add time */
  usecs = (hours   * USEC_PER_HOUR)
        + (minutes * USEC_PER_MINUTE)
        + (seconds * USEC_PER_SECOND);
  g_date_time_add_usec (dt, usecs);

  return dt;
}

/**
 * g_date_time_compare:
 * @dt1: first #GDateTime to compare
 * @dt2: second #GDateTime to compare
 *
 * qsort()-style comparison for #GDateTime<!-- -->'s. Both #GDateTime<-- -->'s
 * must be non-%NULL.
 *
 * Return value: 0 for equal, less than zero if dt1 is less than dt2, greater
 *   than zero if dt2 is greator than dt1.
 *
 * Since: 2.26
 */
gint
g_date_time_compare (gconstpointer dt1,
                     gconstpointer dt2)
{
  const GDateTime *a, *b;

  a = dt1;
  b = dt2;

  if ((a->days == b->days )&&
      (a->usec == b->usec))
    {
      return 0;
    }
  else if ((a->days > b->days) ||
           ((a->days == b->days) && a->usec > b->usec))
    {
      return 1;
    }
  else
    return -1;
}

/**
 * g_date_time_copy:
 * @datetime: a #GDateTime
 *
 * Creates a copy of @datetime.
 *
 * Return value: the newly created #GDateTime which should be freed with
 *   g_date_time_unref().
 *
 * Since: 2.26
 */
GDateTime*
g_date_time_copy (const GDateTime *datetime)
{
  GDateTime *copied;

  g_return_val_if_fail (datetime != NULL, NULL);

  copied = g_date_time_new ();
  copied->days = datetime->days;
  copied->usec = datetime->usec;
  copied->tz = g_time_zone_copy (datetime->tz);

  return copied;
}

/**
 * g_date_time_day:
 * @datetime: a #GDateTime
 *
 * Creates a new #GDateTime at Midnight on the date represented by @datetime.
 *
 * Return value: the newly created #GDateTime which should be freed with
 *   g_date_time_unref().
 *
 * Since: 2.26
 */
GDateTime*
g_date_time_day (const GDateTime *datetime)
{
  GDateTime *date;

  g_return_val_if_fail (datetime != NULL, NULL);

  date = g_date_time_copy (datetime);
  date->usec = 0;

  return date;
}

/**
 * g_date_time_difference:
 * @begin: a #GDateTime
 * @end: a #GDateTime
 *
 * Calculates the known difference in time between @begin and @end.
 *
 * Since the exact precision cannot always be known due to incomplete
 * historic information, an attempt is made to calculate the difference.
 *
 * Return value: the difference between the two #GDateTime, as a time
 *   span expressed in microseconds.
 *
 * Since: 2.26
 */
GTimeSpan
g_date_time_difference (const GDateTime *begin,
                        const GDateTime *end)
{
  g_return_val_if_fail (begin != NULL, 0);
  g_return_val_if_fail (end != NULL, 0);

  return (GTimeSpan) (((gint64) end->days - (gint64) begin->days)
      * USEC_PER_DAY) + ((gint64) end->usec - (gint64) begin->usec);
}

/**
 * g_date_time_equal:
 * @dt1: a #GDateTime
 * @dt2: a #GDateTime
 *
 * Checks to see if @dt1 and @dt2 are equal.
 *
 * Equal here means that they represent the same moment after converting
 * them to the same timezone.
 *
 * Return value: %TRUE if @dt1 and @dt2 are equal
 *
 * Since: 2.26
 */
gboolean
g_date_time_equal (gconstpointer dt1,
                   gconstpointer dt2)
{
  const GDateTime *a, *b;
  GDateTime *a_utc, *b_utc;
  gint64 a_epoch, b_epoch;

  a = dt1;
  b = dt2;

  a_utc = g_date_time_to_utc (a);
  b_utc = g_date_time_to_utc (b);

  a_epoch = g_date_time_to_epoch (a_utc);
  b_epoch = g_date_time_to_epoch (b_utc);

  g_date_time_unref (a_utc);
  g_date_time_unref (b_utc);

  return a_epoch == b_epoch;
}

/**
 * g_date_time_get_day_of_week:
 * @datetime: a #GDateTime
 *
 * Retrieves the day of the week represented by @datetime within the gregorian
 * calendar. 1 is Sunday, 2 is Monday, etc.
 *
 * Return value: the day of the week
 *
 * Since: 2.26
 */
gint
g_date_time_get_day_of_week (const GDateTime *datetime)
{
  gint a, y, m,
       year  = 0,
       month = 0,
       day   = 0,
       dow;

  g_return_val_if_fail (datetime != NULL, 0);

  /*
   * See Calendar FAQ Section 2.6 for algorithm information
   * http://www.tondering.dk/claus/cal/calendar29.txt
   */

  g_date_time_get_dmy (datetime, &day, &month, &year);
  a = (14 - month) / 12;
  y = year - a;
  m = month + (12 * a) - 2;
  dow = ((day + y + (y / 4) - (y / 100) + (y / 400) + (31 * m) / 12) % 7);

  /* 1 is Monday and 7 is Sunday */
  return (dow == 0) ? 7 : dow;
}

/**
 * g_date_time_get_day_of_month:
 * @datetime: a #GDateTime
 *
 * Retrieves the day of the month represented by @datetime in the gregorian
 * calendar.
 *
 * Return value: the day of the month
 *
 * Since: 2.26
 */
gint
g_date_time_get_day_of_month (const GDateTime *datetime)
{
  gint           day_of_year,
                 i;
  const guint16 *days;
  guint16        last = 0;

  g_return_val_if_fail (datetime != NULL, 0);

  days = days_in_year[g_date_time_is_leap_year (datetime) ? 1 : 0];
  g_date_time_get_week_number (datetime, NULL, NULL, &day_of_year);

  for (i = 1; i <= 12; i++)
    {
      if (days [i] >= day_of_year)
        return day_of_year - last;
      last = days [i];
    }

  g_warn_if_reached ();
  return 0;
}

/**
 * g_date_time_get_day_of_year:
 * @datetime: a #GDateTime
 *
 * Retrieves the day of the year represented by @datetime in the Gregorian
 * calendar.
 *
 * Return value: the day of the year
 *
 * Since: 2.26
 */
gint
g_date_time_get_day_of_year (const GDateTime *datetime)
{
  gint doy = 0;

  g_return_val_if_fail (datetime != NULL, 0);

  g_date_time_get_week_number (datetime, NULL, NULL, &doy);
  return doy;
}

/**
 * g_date_time_get_dmy:
 * @datetime: a #GDateTime.
 * @day: (out): the return location for the day of the month, or %NULL.
 * @month: (out): the return location for the monty of the year, or %NULL.
 * @year: (out): the return location for the gregorian year, or %NULL.
 *
 * Retrieves the Gregorian day, month, and year of a given #GDateTime.
 *
 * Since: 2.26
 */
void
g_date_time_get_dmy (const GDateTime *datetime,
                     gint            *day,
                     gint            *month,
                     gint            *year)
{
  gint the_year;
  gint the_month;
  gint the_day;
  gint remaining_days;
  gint y100_cycles;
  gint y4_cycles;
  gint y1_cycles;
  gint preceding;
  gboolean leap;

  g_return_if_fail (datetime != NULL);

  remaining_days = datetime->days;

  /*
   * We need to convert an offset in days to its year/month/day representation.
   * Leap years makes this a little trickier than it should be, so we use
   * 400, 100 and 4 years cycles here to get to the correct year.
   */

  /* Our days offset starts sets 0001-01-01 as day 1, if it was day 0 our
   * math would be simpler, so let's do it */
  remaining_days--;

  the_year = (remaining_days / DAYS_IN_400YEARS) * 400 + 1;
  remaining_days = remaining_days % DAYS_IN_400YEARS;

  y100_cycles = remaining_days / DAYS_IN_100YEARS;
  remaining_days = remaining_days % DAYS_IN_100YEARS;
  the_year += y100_cycles * 100;

  y4_cycles = remaining_days / DAYS_IN_4YEARS;
  remaining_days = remaining_days % DAYS_IN_4YEARS;
  the_year += y4_cycles * 4;

  y1_cycles = remaining_days / 365;
  the_year += y1_cycles;
  remaining_days = remaining_days % 365;

  if (y1_cycles == 4 || y100_cycles == 4) {
    g_assert (remaining_days == 0);

    /* special case that indicates that the date is actually one year before,
     * in the 31th of December */
    the_year--;
    the_month = 12;
    the_day = 31;
    goto end;
  }

  /* now get the month and the day */
  leap = y1_cycles == 3 && (y4_cycles != 24 || y100_cycles == 3);

  g_assert (leap == GREGORIAN_LEAP(the_year));

  the_month = (remaining_days + 50) >> 5;
  preceding = (days_in_year[0][the_month - 1] + (the_month > 2 && leap));
  if (preceding > remaining_days) {
    /* estimate is too large */
    the_month -= 1;
    preceding -= leap ? days_in_months[1][the_month] :
        days_in_months[0][the_month];
  }
  remaining_days -= preceding;
  g_assert(0 <= remaining_days);

  the_day = remaining_days + 1;

end:
  if (year)
    *year = the_year;
  if (month)
    *month = the_month;
  if (day)
    *day = the_day;
}

/**
 * g_date_time_get_hour:
 * @datetime: a #GDateTime
 *
 * Retrieves the hour of the day represented by @datetime
 *
 * Return value: the hour of the day
 *
 * Since: 2.26
 */
gint
g_date_time_get_hour (const GDateTime *datetime)
{
  g_return_val_if_fail (datetime != NULL, 0);

  return (datetime->usec / USEC_PER_HOUR);
}

/**
 * g_date_time_get_julian:
 * @datetime: a #GDateTime
 * @period: (out): a location for the Julian period
 * @julian: (out): a location for the day in the Julian period
 * @hour: (out): a location for the hour of the day
 * @minute: (out): a location for the minute of the hour
 * @second: (out): a location for hte second of the minute
 *
 * Retrieves the Julian period, day, hour, minute, and second which @datetime
 * represents in the Julian calendar.
 *
 * Since: 2.26
 */
void
g_date_time_get_julian (const GDateTime *datetime,
                        gint            *period,
                        gint            *julian,
                        gint            *hour,
                        gint            *minute,
                        gint            *second)
{
  gint y, m, d, a, b, c, e, f, j;
  g_return_if_fail (datetime != NULL);

  g_date_time_get_dmy (datetime, &d, &m, &y);

  /* FIXME: This is probably not optimal and doesn't handle the fact that the
   * julian calendar has its 0 hour on midday */

  a = y / 100;
  b = a / 4;
  c = 2 - a + b;
  e = 365.25 * (y + 4716);
  f = 30.6001 * (m + 1);
  j = c + d + e + f - 1524;

  if (period)
    *period = 0;

  if (julian)
    *julian = j;

  if (hour)
    *hour = (datetime->usec / USEC_PER_HOUR);

  if (minute)
    *minute = (datetime->usec % USEC_PER_HOUR) / USEC_PER_MINUTE;

  if (second)
    *second = (datetime->usec % USEC_PER_MINUTE) / USEC_PER_SECOND;
}

/**
 * g_date_time_get_microsecond:
 * @datetime: a #GDateTime
 *
 * Retrieves the microsecond of the date represented by @datetime
 *
 * Return value: the microsecond of the second
 *
 * Since: 2.26
 */
gint
g_date_time_get_microsecond (const GDateTime *datetime)
{
  g_return_val_if_fail (datetime != NULL, 0);

  return (datetime->usec % USEC_PER_SECOND);
}

/**
 * g_date_time_get_millisecond:
 * @datetime: a #GDateTime
 *
 * Retrieves the millisecond of the date represented by @datetime
 *
 * Return value: the millisecond of the second
 *
 * Since: 2.26
 */
gint
g_date_time_get_millisecond (const GDateTime *datetime)
{
  g_return_val_if_fail (datetime != NULL, 0);

  return (datetime->usec % USEC_PER_SECOND) / USEC_PER_MILLISECOND;
}

/**
 * g_date_time_get_minute:
 * @datetime: a #GDateTime
 *
 * Retrieves the minute of the hour represented by @datetime
 *
 * Return value: the minute of the hour
 *
 * Since: 2.26
 */
gint
g_date_time_get_minute (const GDateTime *datetime)
{
  g_return_val_if_fail (datetime != NULL, 0);

  return (datetime->usec % USEC_PER_HOUR) / USEC_PER_MINUTE;
}

/**
 * g_date_time_get_month:
 * @datetime: a #GDateTime
 *
 * Retrieves the month of the year represented by @datetime in the Gregorian
 * calendar.
 *
 * Return value: the month represented by @datetime
 *
 * Since: 2.26
 */
gint
g_date_time_get_month (const GDateTime *datetime)
{
  gint month;

  g_return_val_if_fail (datetime != NULL, 0);

  g_date_time_get_dmy (datetime, NULL, &month, NULL);

  return month;
}

/**
 * g_date_time_get_second:
 * @datetime: a #GDateTime
 *
 * Retrieves the second of the minute represented by @datetime
 *
 * Return value: the second represented by @datetime
 *
 * Since: 2.26
 */
gint
g_date_time_get_second (const GDateTime *datetime)
{
  g_return_val_if_fail (datetime != NULL, 0);

  return (datetime->usec % USEC_PER_MINUTE) / USEC_PER_SECOND;
}

/**
 * g_date_time_get_utc_offset:
 * @datetime: a #GDateTime
 *
 * Retrieves the offset from UTC that the local timezone specified by
 * @datetime represents.
 *
 * If @datetime represents UTC time, then the offset is zero.
 *
 * Return value: the offset, expressed as a time span expressed in
 *   microseconds.
 *
 * Since: 2.26
 */
GTimeSpan
g_date_time_get_utc_offset (const GDateTime *datetime)
{
  gint offset = 0;

  g_return_val_if_fail (datetime != NULL, 0);

  if (datetime->tz != NULL)
    offset = datetime->tz->offset;

  return (gint64) offset * USEC_PER_SECOND;
}

/**
 * g_date_time_get_timezone_name:
 * @datetime: a #GDateTime
 *
 * Retrieves the Olson's database timezone name of the timezone specified
 * by @datetime.
 *
 * Return value: (transfer none): the name of the timezone. The returned
 *   string is owned by the #GDateTime and it should not be modified or
 *   freed
 *
 * Since: 2.26
 */
G_CONST_RETURN gchar *
g_date_time_get_timezone_name (const GDateTime *datetime)
{
  g_return_val_if_fail (datetime != NULL, NULL);

  if (datetime->tz != NULL)
    return datetime->tz->name;

  return "UTC";
}

/**
 * g_date_time_get_year:
 * @datetime: A #GDateTime
 *
 * Retrieves the year represented by @datetime in the Gregorian calendar.
 *
 * Return value: the year represented by @datetime
 *
 * Since: 2.26
 */
gint
g_date_time_get_year (const GDateTime *datetime)
{
  gint year;

  g_return_val_if_fail (datetime != NULL, 0);

  g_date_time_get_dmy (datetime, NULL, NULL, &year);

  return year;
}

/**
 * g_date_time_hash:
 * @datetime: a #GDateTime
 *
 * Hashes @datetime into a #guint, suitable for use within #GHashTable.
 *
 * Return value: a #guint containing the hash
 *
 * Since: 2.26
 */
guint
g_date_time_hash (gconstpointer datetime)
{
  return (guint) (*((guint64 *) datetime));
}

/**
 * g_date_time_is_leap_year:
 * @datetime: a #GDateTime
 *
 * Determines if @datetime represents a date known to fall within
 * a leap year in the Gregorian calendar.
 *
 * Return value: %TRUE if @datetime is a leap year.
 *
 * Since: 2.26
 */
gboolean
g_date_time_is_leap_year (const GDateTime *datetime)
{
  gint year;

  g_return_val_if_fail (datetime != NULL, FALSE);

  year = g_date_time_get_year (datetime);

  return GREGORIAN_LEAP (year);
}

/**
 * g_date_time_is_daylight_savings:
 * @datetime: a #GDateTime
 *
 * Determines if @datetime represents a date known to fall within daylight
 * savings time in the gregorian calendar.
 *
 * Return value: %TRUE if @datetime falls within daylight savings time.
 *
 * Since: 2.26
 */
gboolean
g_date_time_is_daylight_savings (const GDateTime *datetime)
{
  g_return_val_if_fail (datetime != NULL, FALSE);

  if (!datetime->tz)
    return FALSE;

  return datetime->tz->is_dst;
}

/**
 * g_date_time_new_from_date:
 * @year: the Gregorian year
 * @month: the Gregorian month
 * @day: the day in the Gregorian month
 *
 * Creates a new #GDateTime using the specified date within the Gregorian
 * calendar.
 *
 * Return value: the newly created #GDateTime or %NULL if it is outside of
 *   the representable range.
 *
 * Since: 2.26
 */
GDateTime *
g_date_time_new_from_date (gint year,
                           gint month,
                           gint day)
{
  GDateTime *dt;

  g_return_val_if_fail (year > -4712 && year <= 3268, NULL);
  g_return_val_if_fail (month > 0 && month <= 12, NULL);
  g_return_val_if_fail (day > 0 && day <= 31, NULL);

  dt = g_date_time_new ();

  dt->days = date_to_proleptic_gregorian (year, month, day);
  dt->tz = g_date_time_create_time_zone (dt, NULL);

  return dt;
}

/**
 * g_date_time_new_from_epoch:
 * @t: seconds from the Unix epoch
 *
 * Creates a new #GDateTime using the time since Jan 1, 1970 specified by @t.
 *
 * Return value: the newly created #GDateTime
 *
 * Since: 2.26
 */
GDateTime*
g_date_time_new_from_epoch (gint64 t) /* IN */
{
  struct tm tm;
  time_t tt;

  tt = (time_t) t;
  memset (&tm, 0, sizeof (tm));

  /* XXX: GLib should probably have a wrapper for this */
#ifdef HAVE_LOCALTIME_R
  localtime_r (&tt, &tm);
#else
  {
    struct tm *ptm = localtime (&tt);

    if (ptm == NULL)
      {
        /* Happens at least in Microsoft's C library if you pass a
         * negative time_t. Use 2000-01-01 as default date.
         */
#ifndef G_DISABLE_CHECKS
        g_return_if_fail_warning (G_LOG_DOMAIN, G_STRFUNC, "ptm != NULL");
#endif

        tm.tm_mon = 0;
        tm.tm_mday = 1;
        tm.tm_year = 100;
      }
    else
      memcpy ((void *) &tm, (void *) ptm, sizeof (struct tm));
  }
#endif /* HAVE_LOCALTIME_R */

  return g_date_time_new_full (tm.tm_year + 1900,
                               tm.tm_mon  + 1,
                               tm.tm_mday,
                               tm.tm_hour,
                               tm.tm_min,
                               tm.tm_sec,
                               NULL);
}

/**
 * g_date_time_new_from_timeval:
 * @tv: #GTimeVal
 *
 * Creates a new #GDateTime using the date and time specified by #GTimeVal.
 *
 * Return value: the newly created #GDateTime
 *
 * Since: 2.26
 */
GDateTime *
g_date_time_new_from_timeval (GTimeVal *tv)
{
  GDateTime *datetime;

  g_return_val_if_fail (tv != NULL, NULL);

  datetime = g_date_time_new_from_epoch (tv->tv_sec);
  datetime->usec += tv->tv_usec;
  datetime->tz = g_date_time_create_time_zone (datetime, NULL);

  return datetime;
}

/**
 * g_date_time_new_full:
 * @year: the Gregorian year
 * @month: the Gregorian month
 * @day: the day of the Gregorian month
 * @hour: the hour of the day
 * @minute: the minute of the hour
 * @second: the second of the minute
 * @time_zone: (allow-none): the Olson's database timezone name, or %NULL
 *   for local (e.g. America/New_York)
 *
 * Creates a new #GDateTime using the date and times in the Gregorian calendar.
 *
 * Return value: the newly created #GDateTime
 *
 * Since: 2.26
 */
GDateTime *
g_date_time_new_full (gint         year,
                      gint         month,
                      gint         day,
                      gint         hour,
                      gint         minute,
                      gint         second,
                      const gchar *time_zone)
{
  GDateTime *dt;

  g_return_val_if_fail (hour >= 0 && hour < 24, NULL);
  g_return_val_if_fail (minute >= 0 && minute < 60, NULL);
  g_return_val_if_fail (second >= 0 && second <= 60, NULL);

  if ((dt = g_date_time_new_from_date (year, month, day)) == NULL)
    return NULL;

  dt->usec = (hour   * USEC_PER_HOUR)
           + (minute * USEC_PER_MINUTE)
           + (second * USEC_PER_SECOND);

  dt->tz = g_date_time_create_time_zone (dt, time_zone);
  if (time_zone != NULL && dt->tz == NULL)
    {
      /* timezone creation failed */
      g_date_time_unref (dt);
      dt = NULL;
    }

  return dt;
}

/**
 * g_date_time_new_now:
 *
 * Creates a new #GDateTime representing the current date and time.
 *
 * Return value: the newly created #GDateTime which should be freed with
 *   g_date_time_unref().
 *
 * Since: 2.26
 */
GDateTime*
g_date_time_new_now (void)
{
  GTimeVal tv;

  g_get_current_time (&tv);

  return g_date_time_new_from_timeval (&tv);
}

/**
 * g_date_time_printf:
 * @datetime: A #GDateTime
 * @format: a valid UTF-8 string, containing the format for the #GDateTime
 *
 * Creates a newly allocated string representing the requested @format.
 *
 * The following format specifiers are supported:
 *
 * %%a  The abbreviated weekday name according to the current locale.
 * %%A  The full weekday name according to the current locale.
 * %%b  The abbreviated month name according to the current locale.
 * %%B  The full month name according to the current locale.
 * %%d  The day of the month as a decimal number (range 01 to 31).
 * %%e  The day of the month as a decimal number (range  1 to 31).
 * %%F  Equivalent to %Y-%m-%d (the ISO 8601 date format).
 * %%h  Equivalent to %b.
 * %%H  The hour as a decimal number using a 24-hour clock (range 00 to 23).
 * %%I  The hour as a decimal number using a 12-hour clock (range 01 to 12).
 * %%j  The day of the year as a decimal number (range 001 to 366).
 * %%k  The hour (24-hour clock) as a decimal number (range 0 to 23);
 *      single digits are preceded by a blank.
 * %%l  The hour (12-hour clock) as a decimal number (range 1 to 12);
 *      single digits are preceded by a blank.
 * %%m  The month as a decimal number (range 01 to 12).
 * %%M  The minute as a decimal number (range 00 to 59).
 * %%N  The micro-seconds as a decimal number.
 * %%p  Either "AM" or "PM" according to the given time  value, or the
 *      corresponding  strings  for the current locale.  Noon is treated
 *      as "PM" and midnight as "AM".
 * %%P  Like %%p but lowercase: "am" or "pm" or a corresponding string for
 *      the current locale.
 * %%r  The time in a.m. or p.m. notation.
 * %%R  The time in 24-hour notation (%H:%M).
 * %%s  The number of seconds since the Epoch, that is, since 1970-01-01
 *      00:00:00 UTC.
 * %%S  The second as a decimal number (range 00 to 60).
 * %%t  A tab character.
 * %%u  The day of the week as a decimal, range 1 to 7, Monday being 1.
 * %%W  The week number of the current year as a decimal number.
 * %%x  The preferred date representation for the current locale without
 *      the time.
 * %%X  The preferred date representation for the current locale without
 *      the date.
 * %%y  The year as a decimal number without the century.
 * %%Y  The year as a decimal number including the century.
 * %%Z  Alphabetic time zone abbreviation (e.g. EDT).
 * %%%  A literal %% character.
 *
 * Return value: a newly allocated string formatted to the requested format or
 *   %NULL in the case that there was an error.  The string should be freed
 *   with g_free().
 *
 * Since: 2.26
 */
gchar *
g_date_time_printf (const GDateTime *datetime,
                    const gchar     *format)
{
  GString  *outstr;
  gchar    *tmp;
  gunichar  c;
  glong     utf8len;
  gboolean  in_mod;

  g_return_val_if_fail (datetime != NULL, NULL);
  g_return_val_if_fail (format != NULL, NULL);
  g_return_val_if_fail (g_utf8_validate (format, -1, NULL), NULL);

  outstr = g_string_sized_new (strlen (format) * 2);
  utf8len = g_utf8_strlen (format, -1);
  in_mod = FALSE;

  for (; *format; format = g_utf8_next_char(format))
    {
      c = g_utf8_get_char (format);

      switch (c)
        {
        case '%':
          if (!in_mod)
            {
              in_mod = TRUE;
              break;
            }
            /* Fall through */
        default:
          if (in_mod)
            {
              switch (c)
                {
                case 'a':
                  g_string_append (outstr, WEEKDAY_ABBR (datetime));
                  break;
                case 'A':
                  g_string_append (outstr, WEEKDAY_FULL (datetime));
                  break;
                case 'b':
                  g_string_append (outstr, MONTH_ABBR (datetime));
                  break;
                case 'B':
                  g_string_append (outstr, MONTH_FULL (datetime));
                  break;
                case 'd':
                  g_string_append_printf (outstr, "%02d", g_date_time_get_day_of_month (datetime));
                  break;
                case 'e':
                  g_string_append_printf (outstr, "%2d", g_date_time_get_day_of_month (datetime));
                  break;
                case 'F':
                  g_string_append_printf (outstr, "%d-%02d-%02d",
                                          g_date_time_get_year (datetime),
                                          g_date_time_get_month (datetime),
                                          g_date_time_get_day_of_month (datetime));
                  break;
                case 'h':
                  g_string_append (outstr, MONTH_ABBR (datetime));
                  break;
                case 'H':
                  g_string_append_printf (outstr, "%02d", g_date_time_get_hour (datetime));
                  break;
                case 'I':
                  if (g_date_time_get_hour (datetime) == 0)
                    g_string_append (outstr, "12");
                  else
                    g_string_append_printf (outstr, "%02d", g_date_time_get_hour (datetime) % 12);
                  break;
                case 'j':
                  g_string_append_printf (outstr, "%03d", g_date_time_get_day_of_year (datetime));
                  break;
                case 'k':
                  g_string_append_printf (outstr, "%2d", g_date_time_get_hour (datetime));
                  break;
                case 'l':
                  if (g_date_time_get_hour (datetime) == 0)
                    g_string_append (outstr, "12");
                  else
                    g_string_append_printf (outstr, "%2d", g_date_time_get_hour (datetime) % 12);
                  break;
                case 'm':
                  g_string_append_printf (outstr, "%02d", g_date_time_get_month (datetime));
                  break;
                case 'M':
                  g_string_append_printf (outstr, "%02d", g_date_time_get_minute (datetime));
                  break;
                case 'N':
                  g_string_append_printf (outstr, "%"G_GUINT64_FORMAT, datetime->usec % USEC_PER_SECOND);
                  break;
                case 'p':
                  g_string_append (outstr, GET_AMPM (datetime, FALSE));
                  break;
                case 'P':
                  g_string_append (outstr, GET_AMPM (datetime, TRUE));
                  break;
                case 'r':
                  {
                    gint hour = g_date_time_get_hour (datetime) % 12;
                    if (hour == 0)
                      hour = 12;
                    g_string_append_printf (outstr, "%02d:%02d:%02d %s",
                                            hour,
                                            g_date_time_get_minute (datetime),
                                            g_date_time_get_second (datetime),
                                            GET_AMPM (datetime, FALSE));
                  }
                  break;
                case 'R':
                  g_string_append_printf (outstr, "%02d:%02d",
                                          g_date_time_get_hour (datetime),
                                          g_date_time_get_minute (datetime));
                  break;
                case 's':
                  g_string_append_printf (outstr, "%" G_GINT64_FORMAT, g_date_time_to_epoch (datetime));
                  break;
                case 'S':
                  g_string_append_printf (outstr, "%02d", g_date_time_get_second (datetime));
                  break;
                case 't':
                  g_string_append_c (outstr, '\t');
                  break;
                case 'u':
                  g_string_append_printf (outstr, "%d", g_date_time_get_day_of_week (datetime));
                  break;
                case 'W':
                  g_string_append_printf (outstr, "%d", g_date_time_get_day_of_year (datetime) / 7);
                  break;
                case 'x':
                  {
                    tmp = GET_PREFERRED_DATE (datetime);
                    g_string_append (outstr, tmp);
                    g_free (tmp);
                  }
                  break;
                case 'X':
                  {
                    tmp = GET_PREFERRED_TIME (datetime);
                    g_string_append (outstr, tmp);
                    g_free (tmp);
                  }
                  break;
                case 'y':
                  g_string_append_printf (outstr, "%02d", g_date_time_get_year (datetime) % 100);
                  break;
                case 'Y':
                  g_string_append_printf (outstr, "%d", g_date_time_get_year (datetime));
                  break;
                case 'Z':
                  if (datetime->tz != NULL)
                    g_string_append_printf (outstr, "%s", datetime->tz->name);
                  else
                    g_string_append_printf (outstr, "UTC");
                  break;
                case '%':
                  g_string_append_c (outstr, '%');
                  break;
                case 'n':
                  g_string_append_c (outstr, '\n');
                  break;
                default:
                  goto bad_format;
                }
              in_mod = FALSE;
            }
          else
            g_string_append_unichar (outstr, c);
        }
    }

  return g_string_free (outstr, FALSE);

bad_format:
  g_string_free (outstr, TRUE);
  return NULL;
}

/**
 * g_date_time_ref:
 * @datetime: a #GDateTime
 *
 * Atomically increments the reference count of @datetime by one.
 *
 * Return value: the #GDateTime with the reference count increased
 *
 * Since: 2.26
 */
GDateTime *
g_date_time_ref (GDateTime *datetime)
{
  g_return_val_if_fail (datetime != NULL, NULL);
  g_return_val_if_fail (datetime->ref_count > 0, NULL);

  g_atomic_int_inc (&datetime->ref_count);

  return datetime;
}

/**
 * g_date_time_unref:
 * @datetime: a #GDateTime
 *
 * Atomically decrements the reference count of @datetime by one.
 *
 * When the reference count reaches zero, the resources allocated by
 * @datetime are freed
 *
 * Since: 2.26
 */
void
g_date_time_unref (GDateTime *datetime)
{
  g_return_if_fail (datetime != NULL);
  g_return_if_fail (datetime->ref_count > 0);

  if (g_atomic_int_dec_and_test (&datetime->ref_count))
    g_date_time_free (datetime);
}

/**
 * g_date_time_to_local:
 * @datetime: a #GDateTime
 *
 * Creates a new #GDateTime with @datetime converted to local time.
 *
 * Return value: the newly created #GDateTime
 *
 * Since: 2.26
 */
GDateTime *
g_date_time_to_local (const GDateTime *datetime)
{
  GDateTime *dt;

  g_return_val_if_fail (datetime != NULL, NULL);

  dt = g_date_time_copy (datetime);
  if (dt->tz == NULL)
    {
      dt->tz = g_date_time_create_time_zone (dt, NULL);
      if (dt->tz == NULL)
        return dt;

      g_date_time_add_usec (dt, dt->tz->offset * USEC_PER_SECOND);
    }

  return dt;
}

/**
 * g_date_time_to_epoch:
 * @datetime: a #GDateTime
 *
 * Converts @datetime into an integer representing seconds since the
 * Unix epoch
 *
 * Return value: @datetime as seconds since the Unix epoch
 *
 * Since: 2.26
 */
gint64
g_date_time_to_epoch (const GDateTime *datetime)
{
  gint64 result;

  g_return_val_if_fail (datetime != NULL, 0);

  if (datetime->days <= 0)
    return G_MININT64;

  result = (datetime->days - UNIX_EPOCH_START) * SEC_PER_DAY
         + datetime->usec / USEC_PER_SECOND
         - g_date_time_get_utc_offset (datetime) / 1000000;

  return result;
}

/**
 * g_date_time_to_timeval:
 * @datetime: a #GDateTime
 * @tv: A #GTimeVal
 *
 * Converts @datetime into a #GTimeVal and stores the result into @timeval.
 *
 * Since: 2.26
 */
void
g_date_time_to_timeval (const GDateTime *datetime,
                        GTimeVal        *tv)
{
  g_return_if_fail (datetime != NULL);

  tv->tv_sec = 0;
  tv->tv_usec = 0;

  tv->tv_sec = g_date_time_to_epoch (datetime);
  tv->tv_usec = datetime->usec % USEC_PER_SECOND;
}

/**
 * g_date_time_to_utc:
 * @datetime: a #GDateTime
 *
 * Creates a new #GDateTime that reprents @datetime in Universal coordinated
 * time.
 *
 * Return value: the newly created #GDateTime which should be freed with
 *   g_date_time_unref().
 *
 * Since: 2.26
 */
GDateTime *
g_date_time_to_utc (const GDateTime *datetime)
{
  GDateTime *dt;
  GTimeSpan  ts;

  g_return_val_if_fail (datetime != NULL, NULL);

  ts = g_date_time_get_utc_offset (datetime) * -1;
  dt = g_date_time_add (datetime, ts);
  dt->tz = NULL;

  return dt;
}

/**
 * g_date_time_new_today:
 *
 * Createsa new #GDateTime that represents Midnight on the current day.
 *
 * Return value: the newly created #GDateTime which should be freed with
 *   g_date_time_unref().
 *
 * Since: 2.26
 */
GDateTime*
g_date_time_new_today (void)
{
  GDateTime *dt;

  dt = g_date_time_new_now ();
  dt->usec = 0;

  return dt;
}

/**
 * g_date_time_new_utc_now:
 *
 * Creates a new #GDateTime that represents the current instant in Universal
 * Coordinated Time (UTC).
 *
 * Return value: the newly created #GDateTime which should be freed with
 *   g_date_time_unref().
 *
 * Since: 2.26
 */
GDateTime *
g_date_time_new_utc_now (void)
{
  GDateTime *utc, *now;

  now = g_date_time_new_now ();
  utc = g_date_time_to_utc (now);
  g_date_time_unref (now);

  return utc;
}

/**
 * g_date_time_get_week_of_year:
 *
 * Returns the numeric week of the respective year.
 *
 * Return value: the week of the year
 *
 * Since: 2.26
 */
gint
g_date_time_get_week_of_year (const GDateTime *datetime)
{
  gint weeknum;

  g_return_val_if_fail (datetime != NULL, 0);

  g_date_time_get_week_number (datetime, &weeknum, NULL, NULL);

  return weeknum;
}
