/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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
 */

/*
 * Modified by the GLib Team 2003.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "glib.h"

#include <stdio.h>
#include <string.h>

static gboolean any_failed = FALSE;
static gboolean failed = FALSE;

#define	TEST(message,cond)	G_STMT_START { failed = !(cond); \
if (failed) \
  { if (!message) \
      g_print ("(%s:%d) failed for: %s\n", __FILE__, __LINE__, ( # cond )); \
    else \
      g_print ("(%s:%d) failed for: %s: (%s)\n", __FILE__, __LINE__, ( # cond ), message ? (gchar*)message : ""); \
    fflush (stdout); \
    any_failed; \
  } \
} G_STMT_END

#define TEST_FAILED(message) \
  G_STMT_START { g_print ("Error: "); g_print message; g_print ("\n"); any_failed = TRUE; } G_STMT_END

static gboolean
same_value (const gchar *actual, 
            const gchar *expected)
{
  gdouble actual_value, expected_value;

  actual_value = g_ascii_strtod (actual, NULL);
  expected_value = g_ascii_strtod (expected, NULL);

  return actual_value == expected_value;
}

int
main (int   argc,
      char *argv[])
{
  gchar buf[128];
  int i;
  long l;
  
  /* truncation and return value */
  TEST (NULL, g_snprintf (buf, 0, "abc") == 3);
  TEST (NULL, g_snprintf (NULL, 0, "abc") == 3);
  TEST (NULL, g_snprintf (buf, 5, "abc") == 3);
  TEST (NULL, g_snprintf (buf, 1, "abc") == 3 && buf[0] == '\0' && !strcmp (buf, ""));
  TEST (NULL, g_snprintf (buf, 2, "abc") == 3 && buf[1] == '\0' && !strcmp (buf, "a"));
  TEST (NULL, g_snprintf (buf, 3, "abc") == 3 && buf[2] == '\0' && !strcmp (buf, "ab"));
  TEST (NULL, g_snprintf (buf, 4, "abc") == 3 && buf[3] == '\0' && !strcmp (buf, "abc"));
  TEST (NULL, g_snprintf (buf, 5, "abc") == 3 && buf[3] == '\0' && !strcmp (buf, "abc"));

  /* %d, basic formatting */
  TEST (NULL, g_snprintf (buf, 128, "%d", 5) == 1 && !strcmp (buf, "5"));
  TEST (NULL, g_snprintf (buf, 128, "%d", 0) == 1 && !strcmp (buf, "0"));
  TEST (NULL, g_snprintf (buf, 128, "%.0d", 0) == 0 && !strcmp (buf, ""));
  TEST (NULL, g_snprintf (buf, 128, "%.0d", 1) == 1 && !strcmp (buf, "1"));
  TEST (NULL, g_snprintf (buf, 128, "%d", -1) == 2 && !strcmp (buf, "-1"));
  TEST (NULL, g_snprintf (buf, 128, "%.3d", 5) == 3 && !strcmp (buf, "005"));
  TEST (NULL, g_snprintf (buf, 128, "%.3d", -5) == 4 && !strcmp (buf, "-005"));
  TEST (NULL, g_snprintf (buf, 128, "%5.3d", 5) == 5 && !strcmp (buf, "  005"));
  TEST (NULL, g_snprintf (buf, 128, "%-5.3d", -5) == 5 && !strcmp (buf, "-005 "));
  /* %d, length modifiers */
  TEST (NULL, g_snprintf (buf, 128, "%" G_GINT16_FORMAT, (gint16)-5) == 2 && !strcmp (buf, "-5"));
  TEST (NULL, g_snprintf (buf, 128, "%" G_GUINT16_FORMAT, (guint16)5) == 1 && !strcmp (buf, "5"));
  TEST (NULL, g_snprintf (buf, 128, "%" G_GINT32_FORMAT, (gint32)-5) == 2 && !strcmp (buf, "-5"));
  TEST (NULL, g_snprintf (buf, 128, "%" G_GUINT32_FORMAT, (guint32)5) == 1 && !strcmp (buf, "5"));
  TEST (NULL, g_snprintf (buf, 128, "%" G_GINT64_FORMAT, (gint64)-5) == 2 && !strcmp (buf, "-5"));
  TEST (NULL, g_snprintf (buf, 128, "%" G_GUINT64_FORMAT, (guint64)5) == 1 && !strcmp (buf, "5"));
  TEST (NULL, g_snprintf (buf, 128, "%" G_GSSIZE_FORMAT, (gssize)-5) == 2 && !strcmp (buf, "-5"));
  TEST (NULL, g_snprintf (buf, 128, "%" G_GSIZE_FORMAT, (gsize)5) == 1 && !strcmp (buf, "5"));
  /* %d, flags */
  TEST (NULL, g_snprintf (buf, 128, "%-d", 5) == 1 && !strcmp (buf, "5"));
  TEST (NULL, g_snprintf (buf, 128, "%-+d", 5) == 2 && !strcmp (buf, "+5"));
  TEST (NULL, g_snprintf (buf, 128, "%+-d", 5) == 2 && !strcmp (buf, "+5"));
  TEST (NULL, g_snprintf (buf, 128, "%+d", -5) == 2 && !strcmp (buf, "-5"));
  TEST (NULL, g_snprintf (buf, 128, "% d", 5) == 2 && !strcmp (buf, " 5"));
  TEST (NULL, g_snprintf (buf, 128, "% .0d", 0) == 1 && !strcmp (buf, " "));
  TEST (NULL, g_snprintf (buf, 128, "% +d", 5) == 2 && !strcmp (buf, "+5"));
  TEST (NULL, g_snprintf (buf, 128, "%03d", 5) == 3 && !strcmp (buf, "005"));
  TEST (NULL, g_snprintf (buf, 128, "%-03d", -5) == 3 && !strcmp (buf, "-5 "));
  TEST (NULL, g_snprintf (buf, 128, "%03d", -5) == 3 && !strcmp (buf, "-05"));

  /* %o, basic formatting */
  TEST (NULL, g_snprintf (buf, 128, "%o", 5) == 1 && !strcmp (buf, "5"));
  TEST (NULL, g_snprintf (buf, 128, "%o", 8) == 2 && !strcmp (buf, "10"));
  TEST (NULL, g_snprintf (buf, 128, "%o", 0) == 1 && !strcmp (buf, "0"));
  TEST (NULL, g_snprintf (buf, 128, "%.0o", 0) == 0 && !strcmp (buf, ""));
  TEST (NULL, g_snprintf (buf, 128, "%.0o", 1) == 1 && !strcmp (buf, "1"));
  TEST (NULL, g_snprintf (buf, 128, "%.3o", 5) == 3 && !strcmp (buf, "005"));
  TEST (NULL, g_snprintf (buf, 128, "%.3o", 8) == 3 && !strcmp (buf, "010"));
  TEST (NULL, g_snprintf (buf, 128, "%5.3o", 5) == 5 && !strcmp (buf, "  005"));

  /* %u, basic formatting */
  TEST (NULL, g_snprintf (buf, 128, "%u", 5) == 1 && !strcmp (buf, "5"));
  TEST (NULL, g_snprintf (buf, 128, "%u", 0) == 1 && !strcmp (buf, "0"));
  TEST (NULL, g_snprintf (buf, 128, "%.0u", 0) == 0 && !strcmp (buf, ""));
  TEST (NULL, g_snprintf (buf, 128, "%.0u", 1) == 1 && !strcmp (buf, "1"));
  TEST (NULL, g_snprintf (buf, 128, "%.3u", 5) == 3 && !strcmp (buf, "005"));
  TEST (NULL, g_snprintf (buf, 128, "%5.3u", 5) == 5 && !strcmp (buf, "  005"));

  /* %x, basic formatting */
  TEST (NULL, g_snprintf (buf, 128, "%x", 5) == 1 && !strcmp (buf, "5"));
  TEST (buf, g_snprintf (buf, 128, "%x", 31) == 2 && !strcmp (buf, "1f"));
  TEST (NULL, g_snprintf (buf, 128, "%x", 0) == 1 && !strcmp (buf, "0"));
  TEST (NULL, g_snprintf (buf, 128, "%.0x", 0) == 0 && !strcmp (buf, ""));
  TEST (NULL, g_snprintf (buf, 128, "%.0x", 1) == 1 && !strcmp (buf, "1"));
  TEST (NULL, g_snprintf (buf, 128, "%.3x", 5) == 3 && !strcmp (buf, "005"));
  TEST (NULL, g_snprintf (buf, 128, "%.3x", 31) == 3 && !strcmp (buf, "01f"));
  TEST (NULL, g_snprintf (buf, 128, "%5.3x", 5) == 5 && !strcmp (buf, "  005"));
  /* %x, flags */
  TEST (NULL, g_snprintf (buf, 128, "%-x", 5) == 1 && !strcmp (buf, "5"));
  TEST (NULL, g_snprintf (buf, 128, "%03x", 5) == 3 && !strcmp (buf, "005"));
  TEST (NULL, g_snprintf (buf, 128, "%#x", 31) == 4 && !strcmp (buf, "0x1f"));
  TEST (NULL, g_snprintf (buf, 128, "%#x", 0) == 1 && !strcmp (buf, "0"));

  /* %X, basic formatting */
  TEST (NULL, g_snprintf (buf, 128, "%X", 5) == 1 && !strcmp (buf, "5"));
  TEST (buf, g_snprintf (buf, 128, "%X", 31) == 2 && !strcmp (buf, "1F"));
  TEST (NULL, g_snprintf (buf, 128, "%X", 0) == 1 && !strcmp (buf, "0"));
  TEST (NULL, g_snprintf (buf, 128, "%.0X", 0) == 0 && !strcmp (buf, ""));
  TEST (NULL, g_snprintf (buf, 128, "%.0X", 1) == 1 && !strcmp (buf, "1"));
  TEST (NULL, g_snprintf (buf, 128, "%.3X", 5) == 3 && !strcmp (buf, "005"));
  TEST (NULL, g_snprintf (buf, 128, "%.3X", 31) == 3 && !strcmp (buf, "01F"));
  TEST (NULL, g_snprintf (buf, 128, "%5.3X", 5) == 5 && !strcmp (buf, "  005"));
  /* %X, flags */
  TEST (NULL, g_snprintf (buf, 128, "%-X", 5) == 1 && !strcmp (buf, "5"));
  TEST (NULL, g_snprintf (buf, 128, "%03X", 5) == 3 && !strcmp (buf, "005"));
  TEST (NULL, g_snprintf (buf, 128, "%#X", 31) == 4 && !strcmp (buf, "0X1F"));
  TEST (NULL, g_snprintf (buf, 128, "%#X", 0) == 1 && !strcmp (buf, "0"));

  /* %f, basic formattting */
  TEST (NULL, g_snprintf (buf, 128, "%f", G_PI) == 8 && !strncmp (buf, "3.14159", 7));
  TEST (NULL, g_snprintf (buf, 128, "%.8f", G_PI) == 10 && !strncmp (buf, "3.1415926", 9));
  TEST (NULL, g_snprintf (buf, 128, "%.0f", G_PI) == 1 && !strcmp (buf, "3"));
  /* %f, flags */
  TEST (NULL, g_snprintf (buf, 128, "%+f", G_PI) == 9 && !strncmp (buf, "+3.14159", 8));
  TEST (NULL, g_snprintf (buf, 128, "% f", G_PI) == 9 && !strncmp (buf, " 3.14159", 8));
  TEST (NULL, g_snprintf (buf, 128, "%#.0f", G_PI) == 2 && !strcmp (buf, "3."));
  TEST (NULL, g_snprintf (buf, 128, "%05.2f", G_PI) == 5 && !strcmp (buf, "03.14"));

  /* %e, basic formatting */
  /* for %e we can't expect to reproduce exact strings and lengths, since SUS
   * only guarantees that the exponent shall always contain at least two 
   * digits. On Windows, it seems to be at least three digits long.
   * Therefore, we compare the results of parsing the expected result and the
   * actual result.
   */
  TEST (buf, g_snprintf (buf, 128, "%e", G_PI) >= 12 && same_value (buf, "3.141593e+00"));
  TEST (buf, g_snprintf (buf, 128, "%.8e", G_PI) >= 14 && same_value (buf, "3.14159265e+00"));
  TEST (buf, g_snprintf (buf, 128, "%.0e", G_PI) >= 5 && same_value (buf, "3e+00"));
  TEST (buf, g_snprintf (buf, 128, "%.1e", 0.0) >= 7 && same_value (buf, "0.0e+00"));
  TEST (buf, g_snprintf (buf, 128, "%.1e", 0.00001) >= 7 && same_value (buf, "1.0e-05"));
  TEST (buf, g_snprintf (buf, 128, "%.1e", 10000.0) >= 7 && same_value (buf, "1.0e+04"));
  /* %e, flags */
  TEST (buf, g_snprintf (buf, 128, "%+e", G_PI) >= 13 && same_value (buf, "+3.141593e+00"));
  TEST (buf, g_snprintf (buf, 128, "% e", G_PI) >= 13 && same_value (buf, " 3.141593e+00"));
  TEST (buf, g_snprintf (buf, 128, "%#.0e", G_PI) >= 6 && same_value (buf, "3.e+00"));
  TEST (buf, g_snprintf (buf, 128, "%09.2e", G_PI) >= 9 && same_value (buf, "03.14e+00"));

  /* %c */
  TEST (NULL, g_snprintf (buf, 128, "%c", 'a') == 1 && !strcmp (buf, "a"));

  /* %s */
  TEST (NULL, g_snprintf (buf, 128, "%.2s", "abc") == 2 && !strcmp (buf, "ab"));
  TEST (NULL, g_snprintf (buf, 128, "%.6s", "abc") == 3 && !strcmp (buf, "abc"));
  TEST (NULL, g_snprintf (buf, 128, "%5s", "abc") == 5 && !strcmp (buf, "  abc"));
  TEST (NULL, g_snprintf (buf, 128, "%-5s", "abc") == 5 && !strcmp (buf, "abc  "));
  TEST (NULL, g_snprintf (buf, 128, "%5.2s", "abc") == 5 && !strcmp (buf, "   ab"));
  TEST (NULL, g_snprintf (buf, 128, "%*s", 5, "abc") == 5 && !strcmp (buf, "  abc"));
#if 0 /* HP-UX doesn't get this right */
  TEST (NULL, g_snprintf (buf, 128, "%*s", -5, "abc") == 5 && !strcmp (buf, "abc  "));
#endif
  TEST (NULL, g_snprintf (buf, 128, "%*.*s", 5, 2, "abc") == 5 && !strcmp (buf, "   ab"));

  /* %n */
  TEST (NULL, g_snprintf (buf, 128, "abc%n", &i) == 3 && !strcmp (buf, "abc") && i == 3);
  TEST (NULL, g_snprintf (buf, 128, "abc%ln", &l) == 3 && !strcmp (buf, "abc") && l == 3);

  /* %% */
  TEST (NULL, g_snprintf (buf, 128, "%%") == 1 && !strcmp (buf, "%"));
  
  /* positional parameters */
  TEST (NULL, g_snprintf (buf, 128, "%2$c %1$c", 'b', 'a') == 3 && !strcmp (buf, "a b"));
  TEST (NULL, g_snprintf (buf, 128, "%1$*2$.*3$s", "abc", 5, 2) == 5 && !strcmp (buf, "   ab"));
  TEST (NULL, g_snprintf (buf, 128, "%1$s%1$s", "abc") == 6 && !strcmp (buf, "abcabc"));
  
  /* 64 bit support */
  TEST (NULL, g_snprintf (buf, 128, "%" G_GINT64_FORMAT, (gint64)123456) == 6 && !strcmp (buf, "123456")); 
  TEST (NULL, g_snprintf (buf, 128, "%" G_GINT64_FORMAT, (gint64)-123456) == 7 && !strcmp (buf, "-123456"));   
  TEST (NULL, g_snprintf (buf, 128, "%" G_GUINT64_FORMAT, (guint64)123456) == 6 && !strcmp (buf, "123456")); 
  TEST (NULL, g_snprintf (buf, 128, "%" G_GINT64_MODIFIER "o", (gint64)123456) == 6 && !strcmp (buf, "361100")); 
  TEST (NULL, g_snprintf (buf, 128, "%#" G_GINT64_MODIFIER "o", (gint64)123456) == 7 && !strcmp (buf, "0361100")); 
  TEST (NULL, g_snprintf (buf, 128, "%" G_GINT64_MODIFIER "x", (gint64)123456) == 5 && !strcmp (buf, "1e240")); 
  TEST (NULL, g_snprintf (buf, 128, "%#" G_GINT64_MODIFIER "x", (gint64)123456) == 7 && !strcmp (buf, "0x1e240")); 
  TEST (NULL, g_snprintf (buf, 128, "%" G_GINT64_MODIFIER "X", (gint64)123456) == 5 && !strcmp (buf, "1E240")); 

  return any_failed;
}





