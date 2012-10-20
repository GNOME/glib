/*
 * Copyright © 2010 Codethink Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

/* Prologue {{{1 */

#include "config.h"

#include "gtimezone.h"

#include <string.h>
#include <stdlib.h>
#include <signal.h>

#include "gmappedfile.h"
#include "gtestutils.h"
#include "gfileutils.h"
#include "gstrfuncs.h"
#include "ghash.h"
#include "gthread.h"
#include "gbytes.h"
#include "gslice.h"
#include "gdatetime.h"
#include "gdate.h"

#ifdef G_OS_WIN32
#define STRICT
#include <windows.h>
#endif

/**
 * SECTION:timezone
 * @title: GTimeZone
 * @short_description: a structure representing a time zone
 * @see_also: #GDateTime
 *
 * #GTimeZone is a structure that represents a time zone, at no
 * particular point in time.  It is refcounted and immutable.
 *
 * A time zone contains a number of intervals.  Each interval has
 * an abbreviation to describe it, an offet to UTC and a flag indicating
 * if the daylight savings time is in effect during that interval.  A
 * time zone always has at least one interval -- interval 0.
 *
 * Every UTC time is contained within exactly one interval, but a given
 * local time may be contained within zero, one or two intervals (due to
 * incontinuities associated with daylight savings time).
 *
 * An interval may refer to a specific period of time (eg: the duration
 * of daylight savings time during 2010) or it may refer to many periods
 * of time that share the same properties (eg: all periods of daylight
 * savings time).  It is also possible (usually for political reasons)
 * that some properties (like the abbreviation) change between intervals
 * without other properties changing.
 *
 * #GTimeZone is available since GLib 2.26.
 */

/**
 * GTimeZone:
 *
 * #GDateTime is an opaque structure whose members cannot be accessed
 * directly.
 *
 * Since: 2.26
 **/

/* zoneinfo file format {{{1 */

/* unaligned */
typedef struct { gchar bytes[8]; } gint64_be;
typedef struct { gchar bytes[4]; } gint32_be;
typedef struct { gchar bytes[4]; } guint32_be;

static inline gint64 gint64_from_be (const gint64_be be) {
  gint64 tmp; memcpy (&tmp, &be, sizeof tmp); return GINT64_FROM_BE (tmp);
}

static inline gint32 gint32_from_be (const gint32_be be) {
  gint32 tmp; memcpy (&tmp, &be, sizeof tmp); return GINT32_FROM_BE (tmp);
}

static inline guint32 guint32_from_be (const guint32_be be) {
  guint32 tmp; memcpy (&tmp, &be, sizeof tmp); return GUINT32_FROM_BE (tmp);
}

struct tzhead
{
  gchar      tzh_magic[4];
  gchar      tzh_version;
  guchar     tzh_reserved[15];

  guint32_be tzh_ttisgmtcnt;
  guint32_be tzh_ttisstdcnt;
  guint32_be tzh_leapcnt;
  guint32_be tzh_timecnt;
  guint32_be tzh_typecnt;
  guint32_be tzh_charcnt;
};

struct ttinfo
{
  gint32_be tt_gmtoff;
  guint8    tt_isdst;
  guint8    tt_abbrind;
};

typedef struct
{
  gint32     gmt_offset;
  gboolean   is_dst;
  gboolean   is_standard;
  gboolean   is_gmt;
  gchar     *abbrev;
} TransitionInfo;

typedef struct
{
  gint64 time;
  gint   info_index;
} Transition;

typedef struct
{
  gint     year;
  gint     mon;
  gint     mday;
  gint     wday;
  gint     week;
  gint     hour;
  gint     min;
  gint     sec;
  gboolean isstd;
  gboolean isgmt;
} TimeZoneDate;

typedef struct
{
  gint         start_year;
  gint32       std_offset;
  gint32       dlt_offset;
  TimeZoneDate dlt_start;
  TimeZoneDate dlt_end;
  const gchar *std_name;
  const gchar *dlt_name;
} TimeZoneRule;


/* GTimeZone structure and lifecycle {{{1 */
struct _GTimeZone
{
  gchar   *name;
  GArray  *t_info;
  GArray  *transitions;
  gint     ref_count;
};

G_LOCK_DEFINE_STATIC (time_zones);
static GHashTable/*<string?, GTimeZone>*/ *time_zones;

#define MIN_TZYEAR 1900
#define MAX_TZYEAR 2038

/**
 * g_time_zone_unref:
 * @tz: a #GTimeZone
 *
 * Decreases the reference count on @tz.
 *
 * Since: 2.26
 **/
void
g_time_zone_unref (GTimeZone *tz)
{
  int ref_count;

again:
  ref_count = g_atomic_int_get (&tz->ref_count);

  g_assert (ref_count > 0);

  if (ref_count == 1)
    {
      if (tz->name != NULL)
        {
          G_LOCK(time_zones);

          /* someone else might have grabbed a ref in the meantime */
          if G_UNLIKELY (g_atomic_int_get (&tz->ref_count) != 1)
            {
              G_UNLOCK(time_zones);
              goto again;
            }

          g_hash_table_remove (time_zones, tz->name);
          G_UNLOCK(time_zones);
        }

      g_array_free (tz->t_info, TRUE);
      if (tz->transitions != NULL)
        g_array_free (tz->transitions, TRUE);
      g_free (tz->name);

      g_slice_free (GTimeZone, tz);
    }

  else if G_UNLIKELY (!g_atomic_int_compare_and_exchange (&tz->ref_count,
                                                          ref_count,
                                                          ref_count - 1))
    goto again;
}

/**
 * g_time_zone_ref:
 * @tz: a #GTimeZone
 *
 * Increases the reference count on @tz.
 *
 * Returns: a new reference to @tz.
 *
 * Since: 2.26
 **/
GTimeZone *
g_time_zone_ref (GTimeZone *tz)
{
  g_assert (tz->ref_count > 0);

  g_atomic_int_inc (&tz->ref_count);

  return tz;
}

/* fake zoneinfo creation (for RFC3339/ISO 8601 timezones) {{{1 */
/*
 * parses strings of the form h or hh[[:]mm[[[:]ss]]] where:
 *  - h[h] is 0 to 23
 *  - mm is 00 to 59
 *  - ss is 00 to 59
 */
static gboolean
parse_time (const gchar *time_,
            gint32      *offset)
{
  if (*time_ < '0' || '9' < *time_)
    return FALSE;

  *offset = 60 * 60 * (*time_++ - '0');

  if (*time_ == '\0')
    return TRUE;

  if (*time_ != ':')
    {
      if (*time_ < '0' || '9' < *time_)
        return FALSE;

      *offset *= 10;
      *offset += 60 * 60 * (*time_++ - '0');

      if (*offset > 23 * 60 * 60)
        return FALSE;

      if (*time_ == '\0')
        return TRUE;
    }

  if (*time_ == ':')
    time_++;

  if (*time_ < '0' || '5' < *time_)
    return FALSE;

  *offset += 10 * 60 * (*time_++ - '0');

  if (*time_ < '0' || '9' < *time_)
    return FALSE;

  *offset += 60 * (*time_++ - '0');

  if (*time_ == '\0')
    return TRUE;

  if (*time_ == ':')
    time_++;

  if (*time_ < '0' || '5' < *time_)
    return FALSE;

  *offset += 10 * (*time_++ - '0');

  if (*time_ < '0' || '9' < *time_)
    return FALSE;

  *offset += *time_++ - '0';

  return *time_ == '\0';
}

static gboolean
parse_constant_offset (const gchar *name,
                       gint32      *offset)
{
  if (g_strcmp0 (name, "UTC") == 0)
    {
      *offset = 0;
      return TRUE;
    }

  if (*name >= '0' && '9' >= *name)
    return parse_time (name, offset);

  switch (*name++)
    {
    case 'Z':
      *offset = 0;
      return !*name;

    case '+':
      return parse_time (name, offset);

    case '-':
      if (parse_time (name, offset))
        {
          *offset = -*offset;
          return TRUE;
        }

    default:
      return FALSE;
    }
}

static void
zone_for_constant_offset (GTimeZone *gtz, const gchar *name)
{
  gint32 offset;
  TransitionInfo info;

  if (name == NULL || !parse_constant_offset (name, &offset))
    return;

  info.gmt_offset = offset;
  info.is_dst = FALSE;
  info.is_standard = TRUE;
  info.is_gmt = TRUE;
  info.abbrev =  g_strdup (name);


  gtz->t_info = g_array_sized_new (FALSE, TRUE, sizeof (TransitionInfo), 1);
  g_array_append_val (gtz->t_info, info);

  /* Constant offset, no transitions */
  gtz->transitions = NULL;
}

#ifdef G_OS_UNIX
static GBytes*
zone_info_unix (const gchar *identifier)
{
  gchar *filename;
  GMappedFile *file = NULL;
  GBytes *zoneinfo = NULL;

  /* identifier can be a relative or absolute path name;
     if relative, it is interpreted starting from /usr/share/zoneinfo
     while the POSIX standard says it should start with :,
     glibc allows both syntaxes, so we should too */
  if (identifier != NULL)
    {
      const gchar *tzdir;

      tzdir = getenv ("TZDIR");
      if (tzdir == NULL)
        tzdir = "/usr/share/zoneinfo";

      if (*identifier == ':')
        identifier ++;

      if (g_path_is_absolute (identifier))
        filename = g_strdup (identifier);
      else
        filename = g_build_filename (tzdir, identifier, NULL);
    }
  else
    filename = g_strdup ("/etc/localtime");

  file = g_mapped_file_new (filename, FALSE, NULL);
  if (file != NULL)
    {
      zoneinfo = g_bytes_new_with_free_func (g_mapped_file_get_contents (file),
                                             g_mapped_file_get_length (file),
                                             (GDestroyNotify)g_mapped_file_unref,
                                             g_mapped_file_ref (file));
      g_mapped_file_unref (file);
    }
  g_free (filename);
  return zoneinfo;
}

static void
init_zone_from_iana_info (GTimeZone *gtz, GBytes *zoneinfo)
{
  gsize size;
  guint index;
  guint32 time_count, type_count, leap_count, isgmt_count;
  guint32  isstd_count, char_count ;
  gpointer tz_transitions, tz_type_index, tz_ttinfo;
  gpointer  tz_leaps, tz_isgmt, tz_isstd;
  gchar* tz_abbrs;
  guint timesize = sizeof (gint32), countsize = sizeof (gint32);
  const struct tzhead *header = g_bytes_get_data (zoneinfo, &size);

  g_return_if_fail (size >= sizeof (struct tzhead) &&
                    memcmp (header, "TZif", 4) == 0);

  if (header->tzh_version == '2')
      {
        /* Skip ahead to the newer 64-bit data if it's available. */
        header = (const struct tzhead *)
          (((const gchar *) (header + 1)) +
           guint32_from_be(header->tzh_ttisgmtcnt) +
           guint32_from_be(header->tzh_ttisstdcnt) +
           8 * guint32_from_be(header->tzh_leapcnt) +
           5 * guint32_from_be(header->tzh_timecnt) +
           6 * guint32_from_be(header->tzh_typecnt) +
           guint32_from_be(header->tzh_charcnt));
        timesize = sizeof (gint64);
      }
  time_count = guint32_from_be(header->tzh_timecnt);
  type_count = guint32_from_be(header->tzh_typecnt);
  leap_count = guint32_from_be(header->tzh_leapcnt);
  isgmt_count = guint32_from_be(header->tzh_ttisgmtcnt);
  isstd_count = guint32_from_be(header->tzh_ttisstdcnt);
  char_count = guint32_from_be(header->tzh_charcnt);

  g_assert (type_count == isgmt_count);
  g_assert (type_count == isstd_count);

  tz_transitions = (gpointer)(header + 1);
  tz_type_index = tz_transitions + timesize * time_count;
  tz_ttinfo = tz_type_index + time_count;
  tz_abbrs = tz_ttinfo + sizeof (struct ttinfo) * type_count;
  tz_leaps = tz_abbrs + char_count;
  tz_isstd = tz_leaps + (timesize + countsize) * leap_count;
  tz_isgmt = tz_isstd + isstd_count;

  gtz->t_info = g_array_sized_new (FALSE, TRUE, sizeof (TransitionInfo),
                                   type_count);
  gtz->transitions = g_array_sized_new (FALSE, TRUE, sizeof (Transition),
                                        time_count);

  for (index = 0; index < type_count; index++)
    {
      TransitionInfo t_info;
      struct ttinfo info = ((struct ttinfo*)tz_ttinfo)[index];
      t_info.gmt_offset = gint32_from_be (info.tt_gmtoff);
      t_info.is_dst = info.tt_isdst ? TRUE : FALSE;
      t_info.is_standard = ((guint8*)tz_isstd)[index] ? TRUE : FALSE;
      t_info.is_gmt = ((guint8*)tz_isgmt)[index] ? TRUE : FALSE;
      t_info.abbrev = g_strdup (&tz_abbrs[info.tt_abbrind]);
      g_array_append_val (gtz->t_info, t_info);
    }

  for (index = 0; index < time_count; index++)
    {
      Transition trans;
      if (header->tzh_version == '2')
        trans.time = gint64_from_be (((gint64_be*)tz_transitions)[index]);
      else
        trans.time = gint32_from_be (((gint32_be*)tz_transitions)[index]);
      trans.info_index = ((guint8*)tz_type_index)[index];
      g_assert (trans.info_index >= 0);
      g_assert (trans.info_index < gtz->t_info->len);
      g_array_append_val (gtz->transitions, trans);
    }
  g_bytes_unref (zoneinfo);
}

#elif defined (G_OS_WIN32)

/* UTC = local time + bias while local time = UTC + offset */
static void
rule_from_windows_time_zone_info (TimeZoneRule *rules,
                                  LONG          Bias,
                                  LONG          StandardBias,
                                  LONG          DaylightBias,
                                  SYSTEMTIME    StandardDate,
                                  SYSTEMTIME    DaylightDate)
{
  /* Set offset */
  if (StandardDate.wMonth)
    {
      rules->std_offset = -(Bias + StandardBias) * 60;
      rules->dlt_offset = -(Bias + DaylightBias) * 60;

      rules->dlt_start.sec = DaylightDate.wSecond;
      rules->dlt_start.min = DaylightDate.wMinute;
      rules->dlt_start.hour = DaylightDate.wHour;
      rules->dlt_start.mon = DaylightDate.wMonth;
      rules->dlt_start.year = DaylightDate.wYear;
      rules->dlt_start.wday = DaylightDate.wDayOfWeek? DaylightDate.wDayOfWeek : 7;

      if (DaylightDate.wYear)
        {
          rules->dlt_start.mday = DaylightDate.wDay;
          rules->dlt_start.wday = 0;
        }
      else
        rules->dlt_start.week = DaylightDate.wDay;

      rules->dlt_start.isstd = FALSE;
      rules->dlt_start.isgmt = FALSE;

      rules->dlt_end.sec = StandardDate.wSecond;
      rules->dlt_end.min = StandardDate.wMinute;
      rules->dlt_end.hour = StandardDate.wHour;
      rules->dlt_end.mday = StandardDate.wDay;
      rules->dlt_end.mon = StandardDate.wMonth;
      rules->dlt_end.year = StandardDate.wYear;
      rules->dlt_end.wday = StandardDate.wDayOfWeek? StandardDate.wDayOfWeek : 7;

      if (StandardDate.wYear)
        {
          rules->dlt_end.mday = StandardDate.wDay;
          rules->dlt_end.wday = 0;
        }
      else
        rules->dlt_end.week = StandardDate.wDay;

      rules->dlt_end.isstd = FALSE;
      rules->dlt_end.isgmt = FALSE;
    }

  else
    {
      rules->std_offset = -Bias * 60;

      rules->dlt_start.mon = 0;
    }
}

static gboolean
rules_from_windows_time_zone (const gchar   *identifier,
                              TimeZoneRule **rules,
                              gint          *rules_num,
                              gchar        **std_name,
                              gchar        **dlt_name)
{
  HKEY key;
  gchar *subkey, *subkey_dynamic;
  gchar *key_name;

  /* REG_TZI_FORMAT */
  struct {
    LONG Bias;
    LONG StandardBias;
    LONG DaylightBias;
    SYSTEMTIME StandardDate;
    SYSTEMTIME DaylightDate;
  } tzi, tzi_prev;
  DWORD size;

  *rules = NULL;
  *rules_num = 0;
  *std_name = NULL;
  *dlt_name = NULL;

  key_name = NULL;

  if (!identifier)
    {
      subkey = "SYSTEM\\CurrentControlSet\\Control\\TimeZoneInformation";

      if (RegOpenKeyExA (HKEY_LOCAL_MACHINE, subkey, 0,
                         KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
        {
          size = 0;
          if (RegQueryValueExA (key, "TimeZoneKeyName", NULL, NULL,
                                NULL, &size) == ERROR_SUCCESS)
            {
              key_name = g_malloc (size);

              if (RegQueryValueExA (key, "TimeZoneKeyName", NULL, NULL,
                                    (LPBYTE) key_name, &size) != ERROR_SUCCESS)
                {
                  g_free (key_name);
                  key_name = NULL;
                }
            }

          RegCloseKey (key);
        }
    }
  else
    key_name = g_strdup (identifier);

  if (!key_name)
    return FALSE;

  subkey = g_strconcat ("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Time Zones\\",
                        key_name,
                        NULL);

  subkey_dynamic = g_strconcat (subkey, "\\Dynamic DST", NULL);

  if (RegOpenKeyExA (HKEY_LOCAL_MACHINE, subkey_dynamic, 0,
                     KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
    {
      DWORD first, last;
      int year, i;
      gchar *s;

      size = sizeof first;
      if (RegQueryValueExA (key, "FirstEntry", NULL, NULL,
                            (LPBYTE) &first, &size) != ERROR_SUCCESS)
        goto failed;

      size = sizeof last;
      if (RegQueryValueExA (key, "LastEntry", NULL, NULL,
                            (LPBYTE) &last, &size) != ERROR_SUCCESS)
        goto failed;

      *rules_num = last - first + 2;
      *rules = g_new0 (TimeZoneRule, *rules_num);

      for (year = first, i = 0; year <= last; year++)
        {
          s = g_strdup_printf ("%d", year);

          size = sizeof tzi;
          if (RegQueryValueExA (key, s, NULL, NULL,
                            (LPBYTE) &tzi, &size) != ERROR_SUCCESS)
            {
              g_free (*rules);
              *rules = NULL;
              break;
            }

          g_free (s);

          if (year > first && memcmp (&tzi_prev, &tzi, sizeof tzi) == 0)
              continue;
          else
            memcpy (&tzi_prev, &tzi, sizeof tzi);

          rule_from_windows_time_zone_info (&(*rules)[i], tzi.Bias,
                                            tzi.StandardBias, tzi.DaylightBias,
                                            tzi.StandardDate, tzi.DaylightDate);

          (*rules)[i++].start_year = year;
        }

      *rules_num = i + 1;

failed:
      RegCloseKey (key);
    }
  else if (RegOpenKeyExA (HKEY_LOCAL_MACHINE, subkey, 0,
                          KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
    {
      size = sizeof tzi;
      if (RegQueryValueExA (key, "TZI", NULL, NULL,
                            (LPBYTE) &tzi, &size) == ERROR_SUCCESS)
        {
          *rules_num = 2;
          *rules = g_new0 (TimeZoneRule, 2);

          rule_from_windows_time_zone_info (&(*rules)[0], tzi.Bias,
                                            tzi.StandardBias, tzi.DaylightBias,
                                            tzi.StandardDate, tzi.DaylightDate);
        }

      RegCloseKey (key);
    }

  g_free (subkey_dynamic);
  g_free (subkey);
  g_free (key_name);

  if (*rules)
    {
      (*rules)[0].start_year = MIN_TZYEAR;
      if ((*rules)[*rules_num - 2].start_year < MAX_TZYEAR)
        (*rules)[*rules_num - 1].start_year = MAX_TZYEAR;
      else
        (*rules)[*rules_num - 1].start_year = (*rules)[*rules_num - 2].start_year + 1;

      return TRUE;
    }
  else
    return FALSE;
}

#endif

static void
find_relative_date (TimeZoneDate *buffer,
                    GTimeZone    *tz)
{
  GDateTime *dt;
  gint wday;

  wday = buffer->wday;

  /* Get last day if last is needed, first day otherwise */
  dt = g_date_time_new (tz,
                        buffer->year,
                        buffer->mon + (buffer->week < 5? 0 : 1),
                        buffer->week < 5? 1 : 0,
                        buffer->hour, buffer->min, buffer->sec);

  buffer->wday = g_date_time_get_day_of_week (dt);
  buffer->mday = g_date_time_get_day_of_month (dt);

  if (buffer->week < 5)
    {
      if (wday < buffer->wday)
        buffer->wday -= 7;

      buffer->mday += (buffer->week - 1) * 7;
    }

  else if (wday > buffer->wday)
    buffer->wday += 7;

  buffer->mday += wday - buffer->wday;
  buffer->wday = wday;

  g_date_time_unref (dt);
}

/* Offset is previous offset of local time */
static gint64
boundary_for_year (TimeZoneDate *boundary,
                   gint          year,
                   gint32        prev_offset,
                   gint32        std_offset)
{
  TimeZoneDate buffer;
  GDateTime *dt;
  GTimeZone *tz;
  gint64 t;
  gint32 offset;
  gchar *identifier;

  buffer = *boundary;

  if (boundary->isgmt)
    offset = 0;
  else if (boundary->isstd)
    offset = std_offset;
  else
    offset = prev_offset;

  G_UNLOCK (time_zones);

  identifier = g_strdup_printf ("%+03d:%02d:%02d",
                                (int) offset / 3600,
                                (int) abs (offset / 60) % 60,
                                (int) abs (offset) % 3600);
  tz = g_time_zone_new (identifier);
  g_free (identifier);

  if (boundary->year == 0)
    {
      buffer.year = year;

      if (buffer.wday)
        find_relative_date (&buffer, tz);
    }

  g_assert (buffer.year == year);

  dt = g_date_time_new (tz,
                        buffer.year, buffer.mon, buffer.mday,
                        buffer.hour, buffer.min, buffer.sec);
  t = g_date_time_to_unix (dt);
  g_date_time_unref (dt);

  g_time_zone_unref (tz);

  G_LOCK (time_zones);

  return t;
}

static void
init_zone_from_rules (GTimeZone    *gtz,
                      TimeZoneRule *rules,
                      gint          rules_num)
{
  TransitionInfo info[2];
  Transition trans;
  gint type_count, trans_count;
  gint year, i, x, y;
  gint32 last_offset;

  type_count = 0;
  trans_count = 0;

  /* Last rule only contains max year */
  for (i = 0; i < rules_num - 1; i++)
    {
      if (rules[i].dlt_start.mon)
        {
          type_count += 2;
          trans_count += 2 * (rules[i+1].start_year - rules[i].start_year);
        }
      else
        type_count++;
    }

  x = 0;
  y = 0;

  /* If standard time happens before daylight time in first rule
   * with daylight, skip first transition so the minimum is in
   * standard time and the first transition is in daylight time */
  for (i = 0; i < rules_num - 1 && rules[0].dlt_start.mon == 0; i++);

  if (i < rules_num -1 && rules[i].dlt_start.mon > 0 &&
      rules[i].dlt_start.mon > rules[i].dlt_end.mon)
    {
      trans_count--;
      x = -1;
    }

  gtz->t_info = g_array_sized_new (FALSE, TRUE, sizeof (TransitionInfo), type_count);
  gtz->transitions = g_array_sized_new (FALSE, TRUE, sizeof (Transition), trans_count);

  last_offset = rules[0].std_offset;

  for (i = 0; i < rules_num - 1; i++)
    {
      if (rules[i].dlt_start.mon)
        {
          /* Standard */
          info[0].gmt_offset = rules[i].std_offset;
          info[0].is_dst = FALSE;
          info[0].is_standard = rules[i].dlt_end.isstd;
          info[0].is_gmt = rules[i].dlt_end.isgmt;

          if (rules[i].std_name)
            info[0].abbrev = g_strdup (rules[i].std_name);

          else
            info[0].abbrev = g_strdup_printf ("%+03d%02d",
                                              (int) rules[i].std_offset / 3600,
                                              (int) abs (rules[i].std_offset / 60) % 60);


          /* Daylight */
          info[1].gmt_offset = rules[i].dlt_offset;
          info[1].is_dst = TRUE;
          info[1].is_standard = rules[i].dlt_start.isstd;
          info[1].is_gmt = rules[i].dlt_start.isgmt;

          if (rules[i].dlt_name)
            info[1].abbrev = g_strdup (rules[i].dlt_name);

          else
            info[1].abbrev = g_strdup_printf ("%+03d%02d",
                                              (int) rules[i].dlt_offset / 3600,
                                              (int) abs (rules[i].dlt_offset / 60) % 60);

          if (rules[i].dlt_start.mon < rules[i].dlt_end.mon)
            {
              g_array_append_val (gtz->t_info, info[1]);
              g_array_append_val (gtz->t_info, info[0]);
            }
          else
            {
              g_array_append_val (gtz->t_info, info[0]);
              g_array_append_val (gtz->t_info, info[1]);
            }

          /* Transition dates */
          for (year = rules[i].start_year; year < rules[i+1].start_year; year++)
            {
              if (rules[i].dlt_start.mon < rules[i].dlt_end.mon)
                {
                  /* Daylight Data */
                  trans.info_index = y;
                  trans.time = boundary_for_year (&rules[i].dlt_start, year,
                                                  last_offset, rules[i].std_offset);
                  g_array_insert_val (gtz->transitions, x++, trans);
                  last_offset = rules[i].dlt_offset;

                  /* Standard Data */
                  trans.info_index = y+1;
                  trans.time = boundary_for_year (&rules[i].dlt_end, year,
                                                  last_offset, rules[i].std_offset);
                  g_array_insert_val (gtz->transitions, x++, trans);
                  last_offset = rules[i].std_offset;
                }
              else
                {
                  /* Standard Data */
                  trans.info_index = y;
                  trans.time = boundary_for_year (&rules[i].dlt_end, year,
                                                  last_offset, rules[i].std_offset);
                  if (x >= 0)
                    g_array_insert_val (gtz->transitions, x++, trans);
                  else
                    x++;
                  last_offset = rules[i].std_offset;

                  /* Daylight Data */
                  trans.info_index = y+1;
                  trans.time = boundary_for_year (&rules[i].dlt_start, year,
                                                  last_offset, rules[i].std_offset);
                  g_array_insert_val (gtz->transitions, x++, trans);
                  last_offset = rules[i].dlt_offset;
                }
            }

          y += 2;
        }
      else
        {
          /* Standard */
          info[0].gmt_offset = rules[i].std_offset;
          info[0].is_dst = FALSE;
          info[0].is_standard = FALSE;
          info[0].is_gmt = FALSE;

          if (rules[i].std_name)
            info[0].abbrev = g_strdup (rules[i].std_name);

          else
            info[0].abbrev = g_strdup_printf ("%+03d%02d",
                                              (int) rules[i].std_offset / 3600,
                                              (int) abs (rules[i].std_offset / 60) % 60);

          g_array_append_val (gtz->t_info, info[0]);

          last_offset = rules[i].std_offset;

          y++;
        }
    }
}

/*
 * parses date[/time] for parsing TZ environment variable
 *
 * date is either Mm.w.d, Jn or N
 * - m is 1 to 12
 * - w is 1 to 5
 * - d is 0 to 6
 * - n is 1 to 365
 * - N is 0 to 365
 *
 * time is either h or hh[[:]mm[[[:]ss]]]
 *  - h[h] is 0 to 23
 *  - mm is 00 to 59
 *  - ss is 00 to 59
 */
static gboolean
parse_tz_boundary (const gchar  *identifier,
                   TimeZoneDate *boundary)
{
  const gchar *pos;
  gint month, week, day;
  GDate *date;

  pos = identifier;

  if (*pos == 'M')                      /* Relative date */
    {
      pos++;

      if (*pos == '\0' || *pos < '0' || '9' < *pos)
        return FALSE;

      month = *pos++ - '0';

      if ((month == 1 && *pos >= '0' && '2' >= *pos) ||
          (month == 0 && *pos >= '0' && '9' >= *pos))
        {
          month *= 10;
          month += *pos++ - '0';
        }

      if (*pos++ != '.' || month == 0)
        return FALSE;

      if (*pos == '\0' || *pos < '1' || '5' < *pos)
        return FALSE;

      week = *pos++ - '0';

      if (*pos++ != '.')
        return FALSE;

      if (*pos == '\0' || *pos < '0' || '6' < *pos)
        return FALSE;

      day = *pos++ - '0';

      if (!day)
        day += 7;

      boundary->year = 0;
      boundary->mon = month;
      boundary->week = week;
      boundary->wday = day;
    }

  else if (*pos == 'J')                 /* Julian day */
    {
      pos++;

      day = 0;
      while (*pos >= '0' && '9' >= *pos)
        {
          day *= 10;
          day += *pos++ - '0';
        }

      if (day < 1 || 365 < day)
        return FALSE;

      date = g_date_new_julian (day);
      boundary->year = 0;
      boundary->mon = (int) g_date_get_month (date);
      boundary->mday = (int) g_date_get_day (date);
      boundary->wday = 0;
      g_date_free (date);
    }

  else if (*pos >= '0' && '9' >= *pos)  /* Zero-based Julian day */
    {
      day = 0;
      while (*pos >= '0' && '9' >= *pos)
        {
          day *= 10;
          day += *pos++ - '0';
        }

      if (day < 0 || 365 < day)
        return FALSE;

      date = g_date_new_julian (day >= 59? day : day + 1);
      boundary->year = 0;
      boundary->mon = (int) g_date_get_month (date);
      boundary->mday = (int) g_date_get_day (date);
      boundary->wday = 0;
      g_date_free (date);

      /* February 29 */
      if (day == 59)
        boundary->mday++;
    }

  else
    return FALSE;

  /* Time */
  boundary->isstd = FALSE;
  boundary->isgmt = FALSE;

  if (*pos == '/')
    {
      gint32 offset;

      if (!parse_time (++pos, &offset))
        return FALSE;

      boundary->hour = offset / 3600;
      boundary->min = (offset / 60) % 60;
      boundary->sec = offset % 3600;

      return TRUE;
    }

  else
    {
      boundary->hour = 2;
      boundary->min = 0;
      boundary->sec = 0;

      return *pos == '\0';
    }
}

/*
 * Creates an array of TimeZoneRule from a TZ environment variable
 * type of identifier.  Should free rules, std_name and dlt_name
 * afterwards
 */
static gboolean
rules_from_identifier (const gchar   *identifier,
                       TimeZoneRule **rules,
                       gint          *rules_num,
                       gchar        **std_name,
                       gchar        **dlt_name)
{
  const gchar *std_name_pos, *std_offset_pos;
  const gchar *dlt_name_pos, *dlt_offset_pos;

  const gchar *start_date_pos, *end_date_pos;

  const gchar *pos;
  gchar *buffer;
  gboolean ret;

  gint32 std_offset, dlt_offset;
  TimeZoneDate dlt_start, dlt_end;

  if (!identifier)
    return FALSE;

  pos = identifier;

  std_name_pos = pos;

  /* Name is alpha */
  while ((*pos >= 'a' && 'z' >= *pos) || (*pos >= 'A' && 'Z' >= *pos))
    pos++;

  /* Offset for standard required (format 1) */
  if (*pos == '\0')
    return FALSE;

  /* Name should be three or more alphabetic characters */
  if (pos - identifier < 3)
    return FALSE;

  std_offset_pos = pos;

  /* Standard offset */
  while (*pos == '+' || *pos == '-' || *pos == ':' || (*pos >= '0' && '9' >= *pos))
    pos++;

  buffer = g_strndup (std_offset_pos, pos - std_offset_pos);
  ret = parse_constant_offset (buffer, &std_offset);
  g_free (buffer);

  if (!ret)
    return FALSE;

  dlt_name_pos = pos;
  dlt_offset_pos = NULL;

  /* Format 2 */
  if (*pos != '\0')
    {
      /* Name is alpha */
      while ((*pos >= 'a' && 'z' >= *pos) || (*pos >= 'A' && 'Z' >= *pos))
        pos++;

      /* Name should be three or more alphabetic characters */
      if (pos - identifier < 3)
        return FALSE;

      dlt_offset_pos = pos;

#ifndef G_OS_WIN32
      /* Start and end required (format 2) */
      if (*pos == '\0')
        return FALSE;
#else
      if (*pos != '\0')
        {
#endif

      /* Default offset is 1 hour less from standard offset */
      if (*pos++ == ',')
        dlt_offset = std_offset - 60 * 60;

      else
        {
          /* Daylight offset */
          while (*pos == '+' || *pos == '-' || *pos == ':' || (*pos >= '0' && '9' >= *pos))
            pos++;

          buffer = g_strndup (dlt_offset_pos, pos - dlt_offset_pos);
          ret = parse_constant_offset (buffer, &dlt_offset);
          g_free (buffer);

          if (!ret)
            return FALSE;

          /* Start and end required (format 2) */
          if (*pos++ != ',')
            return FALSE;
        }

      /* Start date */
      start_date_pos = pos;

      while (*pos != ',' && *pos != '\0')
        pos++;

      /* End required (format 2) */
      if (*pos == '\0')
        return FALSE;

      buffer = g_strndup (start_date_pos, pos++ - start_date_pos);
      ret = parse_tz_boundary (buffer, &dlt_start);
      g_free (buffer);

      if (!ret)
        return FALSE;

      /* End date */
      end_date_pos = pos;

      while (*pos != '\0')
        pos++;

      buffer = g_strndup (end_date_pos, pos - end_date_pos);
      ret = parse_tz_boundary (buffer, &dlt_end);
      g_free (buffer);

      if (!ret)
        return FALSE;

#ifdef G_OS_WIN32
      }
#endif
    }


  *std_name = g_strndup (std_name_pos, std_offset_pos - std_name_pos);

  if (dlt_name_pos != pos)
    *dlt_name = g_strndup (dlt_name_pos, dlt_offset_pos - dlt_name_pos);
  else
    {
      *dlt_name = NULL;
      dlt_start.mon = 0;
    }


#ifdef G_OS_WIN32
  /* If doesn't have offset for daylight then it is Windows format */
  if (dlt_offset_pos == pos)
    {
      int i;

      /* Use US rules, Windows' default is Pacific Standard Time */
      dlt_offset = std_offset - 60 * 60;

      if (rules_from_windows_time_zone ("Pacific Standard Time", rules, rules_num, NULL, NULL))
        {
          for (i = 0; i < *rules_num - 1; i++)
            {
              (*rules)[i].std_offset = -std_offset;
              (*rules)[i].dlt_offset = -dlt_offset;
              (*rules)[i].std_name = *std_name;
              (*rules)[i].dlt_name = *dlt_name;
            }

          return TRUE;
        }
      else
        return FALSE;
    }
#endif


  *rules_num = 2;
  *rules = g_new0 (TimeZoneRule, 2);

  (*rules)[0].start_year = MIN_TZYEAR;
  (*rules)[1].start_year = MAX_TZYEAR;

  (*rules)[0].std_offset = -std_offset;
  (*rules)[0].dlt_offset = -dlt_offset;
  (*rules)[0].dlt_start  = dlt_start;
  (*rules)[0].dlt_end = dlt_end;
  (*rules)[0].std_name = *std_name;
  (*rules)[0].dlt_name = *dlt_name;

  return TRUE;
}

/* Construction {{{1 */
/**
 * g_time_zone_new:
 * @identifier: (allow-none): a timezone identifier
 *
 * Creates a #GTimeZone corresponding to @identifier.
 *
 * @identifier can either be an RFC3339/ISO 8601 time offset or
 * something that would pass as a valid value for the
 * <varname>TZ</varname> environment variable (including %NULL).
 *
 * In Windows, @identifier can also be the unlocalized name of a time
 * zone for standard time, for example "Pacific Standard Time".
 *
 * Valid RFC3339 time offsets are <literal>"Z"</literal> (for UTC) or
 * <literal>"±hh:mm"</literal>.  ISO 8601 additionally specifies
 * <literal>"±hhmm"</literal> and <literal>"±hh"</literal>.  Offsets are
 * time values to be added to Coordinated Universal Time (UTC) to get
 * the local time.
 *
 * In Unix, the <varname>TZ</varname> environment variable typically
 * corresponds to the name of a file in the zoneinfo database, or
 * string in "std offset [dst [offset],start[/time],end[/time]]"
 * (POSIX) format.  There  are  no spaces in the specification.  The
 * name of standard and daylight savings time zone must be three or more
 * alphabetic characters.  Offsets are time values to be added to local
 * time to get Coordinated Universal Time (UTC) and should be
 * <literal>"[±]hh[[:]mm[:ss]]"</literal>.  Dates are either
 * <literal>"Jn"</literal> (Julian day with n between 1 and 365, leap
 * years not counted), <literal>"n"</literal> (zero-based Julian day
 * with n between 0 and 365) or <literal>"Mm.w.d"</literal> (day d
 * (0 <= d <= 6) of week w (1 <= w <= 5) of month m (1 <= m <= 12), day
 * 0 is a Sunday).  Times are in local wall clock time, the default is
 * 02:00:00.
 *
 * In Windows, the "tzn[+|–]hh[:mm[:ss]][dzn]" format is used, but also
 * accepts POSIX format.  The Windows format uses US rules for all time
 * zones; daylight savings time is 60 minutes behind the standard time
 * with date and time of change taken from Pacific Standard Time.
 * Offsets are time values to be added to the local time to get
 * Coordinated Universal Time (UTC).
 *
 * g_time_zone_new_local() calls this function with the value of the
 * <varname>TZ</varname> environment variable.  This function itself is
 * independent of the value of <varname>TZ</varname>, but if @identifier
 * is %NULL then <filename>/etc/localtime</filename> will be consulted
 * to discover the correct time zone on Unix and the registry will be
 * consulted or GetTimeZoneInformation() will be used to get the local
 * time zone on Windows.
 *
 * If intervals are not available, only time zone rules from
 * <varname>TZ</varname> environment variable or other means, then they
 * will be computed from year 1900 to 2037.  If the maximum year for the
 * rules is available and it is greater than 2037, then it will followed
 * instead.
 *
 * See <ulink
 * url='http://tools.ietf.org/html/rfc3339#section-5.6'>RFC3339
 * §5.6</ulink> for a precise definition of valid RFC3339 time offsets
 * (the <varname>time-offset</varname> expansion) and ISO 8601 for the
 * full list of valid time offsets.  See <ulink
 * url='http://www.gnu.org/s/libc/manual/html_node/TZ-Variable.html'>The
 * GNU C Library manual</ulink> for an explanation of the possible
 * values of the <varname>TZ</varname> environment variable.  See <ulink
 * url='http://msdn.microsoft.com/en-us/library/ms912391%28v=winembedded.11%29.aspx'>
 * Microsoft Time Zone Index Values</ulink> for the list of time zones
 * on Windows.
 *
 * You should release the return value by calling g_time_zone_unref()
 * when you are done with it.
 *
 * Returns: the requested timezone
 *
 * Since: 2.26
 **/
GTimeZone *
g_time_zone_new (const gchar *identifier)
{
  GTimeZone *tz = NULL;
  TimeZoneRule *rules;
  gint rules_num;
  gchar *std_name, *dlt_name;

  G_LOCK (time_zones);
  if (time_zones == NULL)
    time_zones = g_hash_table_new (g_str_hash, g_str_equal);

  if (identifier)
    {
      tz = g_hash_table_lookup (time_zones, identifier);
      if (tz)
        {
          g_atomic_int_inc (&tz->ref_count);
          G_UNLOCK (time_zones);
          return tz;
        }
    }

  tz = g_slice_new0 (GTimeZone);
  tz->name = g_strdup (identifier);
  tz->ref_count = 0;

  zone_for_constant_offset (tz, identifier);

  if (tz->t_info == NULL &&
      rules_from_identifier (identifier,
                             &rules, &rules_num,
                             &std_name, &dlt_name))
    {
      init_zone_from_rules (tz, rules, rules_num);
      g_free (rules);
      g_free (std_name);
      g_free (dlt_name);
    }

  if (tz->t_info == NULL)
    {
#ifdef G_OS_UNIX
      GBytes *zoneinfo = zone_info_unix (identifier);
      if (!zoneinfo)
        zone_for_constant_offset (tz, "UTC");
      else
        {
          init_zone_from_iana_info (tz, zoneinfo);
          g_bytes_unref (zoneinfo);
        }
#elif defined (G_OS_WIN32)
      if (rules_from_windows_time_zone (identifier,
                                        &rules, &rules_num,
                                        &std_name, &dlt_name))
        {
          init_zone_from_rules (tz, rules, rules_num);
          g_free (rules);
          g_free (std_name);
          g_free (dlt_name);
        }
    }

  if (tz->t_info == NULL)
    {
      if (identifier)
        zone_for_constant_offset (tz, "UTC");
      else
        {
          TIME_ZONE_INFORMATION tzi;

          if (GetTimeZoneInformation (&tzi) != TIME_ZONE_ID_INVALID)
            {
              rules = g_new0 (TimeZoneRule, 2);

              rule_from_windows_time_zone_info (&rules[0], tzi.Bias,
                                                tzi.StandardBias, tzi.DaylightBias,
                                                tzi.StandardDate, tzi.DaylightDate);

              rules[0].std_name = NULL;
              rules[0].dlt_name = NULL;

              rules[0].start_year = MIN_TZYEAR;
              rules[1].start_year = MAX_TZYEAR;

              init_zone_from_rules (tz, rules, 2);

              g_free (rules);
            }
        }
#endif
    }

  if (tz->t_info != NULL)
    {
      if (identifier)
        g_hash_table_insert (time_zones, tz->name, tz);
    }
  g_atomic_int_inc (&tz->ref_count);
  G_UNLOCK (time_zones);

  return tz;
}

/**
 * g_time_zone_new_utc:
 *
 * Creates a #GTimeZone corresponding to UTC.
 *
 * This is equivalent to calling g_time_zone_new() with a value like
 * "Z", "UTC", "+00", etc.
 *
 * You should release the return value by calling g_time_zone_unref()
 * when you are done with it.
 *
 * Returns: the universal timezone
 *
 * Since: 2.26
 **/
GTimeZone *
g_time_zone_new_utc (void)
{
  return g_time_zone_new ("UTC");
}

/**
 * g_time_zone_new_local:
 *
 * Creates a #GTimeZone corresponding to local time.  The local time
 * zone may change between invocations to this function; for example,
 * if the system administrator changes it.
 *
 * This is equivalent to calling g_time_zone_new() with the value of the
 * <varname>TZ</varname> environment variable (including the possibility
 * of %NULL).
 *
 * You should release the return value by calling g_time_zone_unref()
 * when you are done with it.
 *
 * Returns: the local timezone
 *
 * Since: 2.26
 **/
GTimeZone *
g_time_zone_new_local (void)
{
  return g_time_zone_new (getenv ("TZ"));
}

#define TRANSITION(n)         g_array_index (tz->transitions, Transition, n)
#define TRANSITION_INFO(n)    g_array_index (tz->t_info, TransitionInfo, n)

/* Internal helpers {{{1 */
/* Note that interval 0 is *before* the first transition time, so
 * interval 1 gets transitions[0].
 */
inline static const TransitionInfo*
interval_info (GTimeZone *tz,
               guint      interval)
{
  guint index;
  g_return_val_if_fail (tz->t_info != NULL, NULL);
  if (interval && tz->transitions && interval <= tz->transitions->len)
    index = (TRANSITION(interval - 1)).info_index;
  else
    index = 0;
  return &(TRANSITION_INFO(index));
}

inline static gint64
interval_start (GTimeZone *tz,
                guint      interval)
{
  if (!interval || tz->transitions == NULL || tz->transitions->len == 0)
    return G_MININT64;
  if (interval > tz->transitions->len)
    interval = tz->transitions->len;
  return (TRANSITION(interval - 1)).time;
}

inline static gint64
interval_end (GTimeZone *tz,
              guint      interval)
{
  if (tz->transitions && interval < tz->transitions->len)
    return (TRANSITION(interval)).time - 1;
  return G_MAXINT64;
}

inline static gint32
interval_offset (GTimeZone *tz,
                 guint      interval)
{
  g_return_val_if_fail (tz->t_info != NULL, 0);
  return interval_info (tz, interval)->gmt_offset;
}

inline static gboolean
interval_isdst (GTimeZone *tz,
                guint      interval)
{
  g_return_val_if_fail (tz->t_info != NULL, 0);
  return interval_info (tz, interval)->is_dst;
}


inline static gboolean
interval_isgmt (GTimeZone *tz,
                guint      interval)
{
  g_return_val_if_fail (tz->t_info != NULL, 0);
  return interval_info (tz, interval)->is_gmt;
}

inline static gboolean
interval_isstandard (GTimeZone *tz,
                guint      interval)
{
  return interval_info (tz, interval)->is_standard;
}

inline static gchar*
interval_abbrev (GTimeZone *tz,
                  guint      interval)
{
  g_return_val_if_fail (tz->t_info != NULL, 0);
  return interval_info (tz, interval)->abbrev;
}

inline static gint64
interval_local_start (GTimeZone *tz,
                      guint      interval)
{
  if (interval)
    return interval_start (tz, interval) + interval_offset (tz, interval);

  return G_MININT64;
}

inline static gint64
interval_local_end (GTimeZone *tz,
                    guint      interval)
{
  if (tz->transitions && interval < tz->transitions->len)
    return interval_end (tz, interval) + interval_offset (tz, interval);

  return G_MAXINT64;
}

static gboolean
interval_valid (GTimeZone *tz,
                guint      interval)
{
  if ( tz->transitions == NULL)
    return interval == 0;
  return interval <= tz->transitions->len;
}

/* g_time_zone_find_interval() {{{1 */

/**
 * g_time_zone_adjust_time:
 * @tz: a #GTimeZone
 * @type: the #GTimeType of @time_
 * @time_: a pointer to a number of seconds since January 1, 1970
 *
 * Finds an interval within @tz that corresponds to the given @time_,
 * possibly adjusting @time_ if required to fit into an interval.
 * The meaning of @time_ depends on @type.
 *
 * This function is similar to g_time_zone_find_interval(), with the
 * difference that it always succeeds (by making the adjustments
 * described below).
 *
 * In any of the cases where g_time_zone_find_interval() succeeds then
 * this function returns the same value, without modifying @time_.
 *
 * This function may, however, modify @time_ in order to deal with
 * non-existent times.  If the non-existent local @time_ of 02:30 were
 * requested on March 14th 2010 in Toronto then this function would
 * adjust @time_ to be 03:00 and return the interval containing the
 * adjusted time.
 *
 * Returns: the interval containing @time_, never -1
 *
 * Since: 2.26
 **/
gint
g_time_zone_adjust_time (GTimeZone *tz,
                         GTimeType  type,
                         gint64    *time_)
{
  gint i;
  guint intervals;

  if (tz->transitions == NULL)
    return 0;

  intervals = tz->transitions->len;

  /* find the interval containing *time UTC
   * TODO: this could be binary searched (or better) */
  for (i = 0; i <= intervals; i++)
    if (*time_ <= interval_end (tz, i))
      break;

  g_assert (interval_start (tz, i) <= *time_ && *time_ <= interval_end (tz, i));

  if (type != G_TIME_TYPE_UNIVERSAL)
    {
      if (*time_ < interval_local_start (tz, i))
        /* if time came before the start of this interval... */
        {
          i--;

          /* if it's not in the previous interval... */
          if (*time_ > interval_local_end (tz, i))
            {
              /* it doesn't exist.  fast-forward it. */
              i++;
              *time_ = interval_local_start (tz, i);
            }
        }

      else if (*time_ > interval_local_end (tz, i))
        /* if time came after the end of this interval... */
        {
          i++;

          /* if it's not in the next interval... */
          if (*time_ < interval_local_start (tz, i))
            /* it doesn't exist.  fast-forward it. */
            *time_ = interval_local_start (tz, i);
        }

      else if (interval_isdst (tz, i) != type)
        /* it's in this interval, but dst flag doesn't match.
         * check neighbours for a better fit. */
        {
          if (i && *time_ <= interval_local_end (tz, i - 1))
            i--;

          else if (i < intervals &&
                   *time_ >= interval_local_start (tz, i + 1))
            i++;
        }
    }

  return i;
}

/**
 * g_time_zone_find_interval:
 * @tz: a #GTimeZone
 * @type: the #GTimeType of @time_
 * @time_: a number of seconds since January 1, 1970
 *
 * Finds an the interval within @tz that corresponds to the given @time_.
 * The meaning of @time_ depends on @type.
 *
 * If @type is %G_TIME_TYPE_UNIVERSAL then this function will always
 * succeed (since universal time is monotonic and continuous).
 *
 * Otherwise @time_ is treated is local time.  The distinction between
 * %G_TIME_TYPE_STANDARD and %G_TIME_TYPE_DAYLIGHT is ignored except in
 * the case that the given @time_ is ambiguous.  In Toronto, for example,
 * 01:30 on November 7th 2010 occurred twice (once inside of daylight
 * savings time and the next, an hour later, outside of daylight savings
 * time).  In this case, the different value of @type would result in a
 * different interval being returned.
 *
 * It is still possible for this function to fail.  In Toronto, for
 * example, 02:00 on March 14th 2010 does not exist (due to the leap
 * forward to begin daylight savings time).  -1 is returned in that
 * case.
 *
 * Returns: the interval containing @time_, or -1 in case of failure
 *
 * Since: 2.26
 */
gint
g_time_zone_find_interval (GTimeZone *tz,
                           GTimeType  type,
                           gint64     time_)
{
  gint i;
  guint intervals;

  if (tz->transitions == NULL)
    return 0;
  intervals = tz->transitions->len;
  for (i = 0; i <= intervals; i++)
    if (time_ <= interval_end (tz, i))
      break;

  if (type == G_TIME_TYPE_UNIVERSAL)
    return i;

  if (time_ < interval_local_start (tz, i))
    {
      if (time_ > interval_local_end (tz, --i))
        return -1;
    }

  else if (time_ > interval_local_end (tz, i))
    {
      if (time_ < interval_local_start (tz, ++i))
        return -1;
    }

  else if (interval_isdst (tz, i) != type)
    {
      if (i && time_ <= interval_local_end (tz, i - 1))
        i--;

      else if (i < intervals && time_ >= interval_local_start (tz, i + 1))
        i++;
    }

  return i;
}

/* Public API accessors {{{1 */

/**
 * g_time_zone_get_abbreviation:
 * @tz: a #GTimeZone
 * @interval: an interval within the timezone
 *
 * Determines the time zone abbreviation to be used during a particular
 * @interval of time in the time zone @tz.
 *
 * For example, in Toronto this is currently "EST" during the winter
 * months and "EDT" during the summer months when daylight savings time
 * is in effect.
 *
 * Returns: the time zone abbreviation, which belongs to @tz
 *
 * Since: 2.26
 **/
const gchar *
g_time_zone_get_abbreviation (GTimeZone *tz,
                              gint       interval)
{
  g_return_val_if_fail (interval_valid (tz, (guint)interval), NULL);

  return interval_abbrev (tz, (guint)interval);
}

/**
 * g_time_zone_get_offset:
 * @tz: a #GTimeZone
 * @interval: an interval within the timezone
 *
 * Determines the offset to UTC in effect during a particular @interval
 * of time in the time zone @tz.
 *
 * The offset is the number of seconds that you add to UTC time to
 * arrive at local time for @tz (ie: negative numbers for time zones
 * west of GMT, positive numbers for east).
 *
 * Returns: the number of seconds that should be added to UTC to get the
 *          local time in @tz
 *
 * Since: 2.26
 **/
gint32
g_time_zone_get_offset (GTimeZone *tz,
                        gint       interval)
{
  g_return_val_if_fail (interval_valid (tz, (guint)interval), 0);

  return interval_offset (tz, (guint)interval);
}

/**
 * g_time_zone_is_dst:
 * @tz: a #GTimeZone
 * @interval: an interval within the timezone
 *
 * Determines if daylight savings time is in effect during a particular
 * @interval of time in the time zone @tz.
 *
 * Returns: %TRUE if daylight savings time is in effect
 *
 * Since: 2.26
 **/
gboolean
g_time_zone_is_dst (GTimeZone *tz,
                    gint       interval)
{
  g_return_val_if_fail (interval_valid (tz, interval), FALSE);

  if (tz->transitions == NULL)
    return FALSE;

  return interval_isdst (tz, (guint)interval);
}

/* Epilogue {{{1 */
/* vim:set foldmethod=marker: */
