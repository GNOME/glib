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
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include <string.h>

#include <glib.h>

/* Bug 311337 */
static void
test_iconv_state (void)
{
  gchar *in = "\xf4\xe5\xf8\xe5\xed";
  gchar *expected = "\xd7\xa4\xd7\x95\xd7\xa8\xd7\x95\xd7\x9d";
  gchar *out;
  gsize bytes_read = 0;
  gsize bytes_written = 0;
  GError *error = NULL;

  out = g_convert (in, -1, "UTF-8", "CP1255", 
		   &bytes_read, &bytes_written, &error);
  
  g_assert (error == NULL);
  g_assert (bytes_read == 5);
  g_assert (bytes_written == 10);
  g_assert (strcmp (out, expected) == 0);
}

/* some tests involving "vulgar fraction one half" */
static void 
test_one_half (void)
{
  gchar *in = "\xc2\xbd";
  gchar *out;
  gsize bytes_read = 0;
  gsize bytes_written = 0;
  GError *error = NULL;  

  out = g_convert (in, -1, 
		   "ISO8859-1", "UTF-8",
		   &bytes_read, &bytes_written,
		   &error);

  g_assert (error == NULL);
  g_assert (bytes_read == 2);
  g_assert (bytes_written == 1);
  g_assert (strcmp (out, "\xbd") == 0);
  g_free (out);

  out = g_convert (in, -1, 
		   "ISO8859-15", "UTF-8",
		   &bytes_read, &bytes_written,
		   &error);

  g_assert (error && error->code == G_CONVERT_ERROR_ILLEGAL_SEQUENCE);
  g_assert (bytes_read == 0);
  g_assert (bytes_written == 0);
  g_assert (out == NULL);
  g_clear_error (&error);

  out = g_convert_with_fallback (in, -1, 
				 "ISO8859-15", "UTF-8",
				 "a",
				 &bytes_read, &bytes_written,
				 &error);

  g_assert (error == NULL);
  g_assert (bytes_read == 2);
  g_assert (bytes_written == 1);
  g_assert (strcmp (out, "a") == 0);
  g_free (out);
}

static void
test_byte_order (void)
{
  gchar *in_be = "\xfe\xff\x03\x93"; /* capital gamma */
  gchar *in_le = "\xff\xfe\x93\x03";
  gchar *expected = "\xce\x93";
  gchar *out;
  gsize bytes_read = 0;
  gsize bytes_written = 0;
  GError *error = NULL;  
  int i;

  out = g_convert (in_le, -1, 
		   "UTF-8", "UTF-16",
		   &bytes_read, &bytes_written,
		   &error);

  g_assert (error == NULL);
  g_assert (bytes_read == 4);
  g_assert (bytes_written == 2);
  g_assert (strcmp (out, expected) == 0);
  g_free (out);

  out = g_convert (in_le, -1, 
		   "UTF-8", "UTF-16",
		   &bytes_read, &bytes_written,
		   &error);

  g_assert (error == NULL);
  g_assert (bytes_read == 2);
  g_assert (bytes_written == 2);
  g_assert (strcmp (out, expected) == 0);
  g_free (out);
}

int
main (int argc, char *argv[])
{
  test_iconv_state ();
  test_one_half ();
  
#if 0
  /* this one currently fails due to 
   * https://bugzilla.redhat.com/bugzilla/show_bug.cgi?id=165368 
   */
  test_byte_order ();
#endif

  return 0;
}
