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

#endif

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
 * Valid RFC3339 time offsets are <literal>"Z"</literal> (for UTC) or
 * <literal>"±hh:mm"</literal>.  ISO 8601 additionally specifies
 * <literal>"±hhmm"</literal> and <literal>"±hh"</literal>.
 *
 * The <varname>TZ</varname> environment variable typically corresponds
 * to the name of a file in the zoneinfo database, but there are many
 * other possibilities.  Note that those other possibilities are not
 * currently implemented, but are planned.
 *
 * g_time_zone_new_local() calls this function with the value of the
 * <varname>TZ</varname> environment variable.  This function itself is
 * independent of the value of <varname>TZ</varname>, but if @identifier
 * is %NULL then <filename>/etc/localtime</filename> will be consulted
 * to discover the correct timezone.
 *
 * See <ulink
 * url='http://tools.ietf.org/html/rfc3339#section-5.6'>RFC3339
 * §5.6</ulink> for a precise definition of valid RFC3339 time offsets
 * (the <varname>time-offset</varname> expansion) and ISO 8601 for the
 * full list of valid time offsets.  See <ulink
 * url='http://www.gnu.org/s/libc/manual/html_node/TZ-Variable.html'>The
 * GNU C Library manual</ulink> for an explanation of the possible
 * values of the <varname>TZ</varname> environment variable.
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
#elif defined G_OS_WIN32
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
