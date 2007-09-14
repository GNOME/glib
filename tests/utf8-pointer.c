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

#include <string.h>
#include <glib.h>

/* Test conversions between offsets and pointers */

static void test_utf8 (gchar *string)
{
  gint num_chars;
  gchar **p;
  gint i, j;
  
  g_assert (g_utf8_validate (string, -1, NULL));
  
  num_chars = g_utf8_strlen (string, -1);
  
  p = (gchar **) g_malloc (num_chars * sizeof (gchar *));
  
  p[0] = string;
  for (i = 1; i < num_chars; i++)
    p[i] = g_utf8_next_char (p[i-1]);
  
  for (i = 0; i < num_chars; i++)
    for (j = 0; j < num_chars; j++) 
      {
	g_assert (g_utf8_offset_to_pointer (p[i], j - i) == p[j]);	
	g_assert (g_utf8_pointer_to_offset (p[i], p[j]) == j - i);	
      }
  
  g_free (p);
}

gchar *longline = "asdasdas dsaf asfd as fdasdf asfd asdf as dfas dfasdf a"
"asd fasdf asdf asdf asd fasfd as fdasfd asdf as fdççççççççças ffsd asfd as fdASASASAs As"
"Asfdsf sdfg sdfg dsfg dfg sdfgsdfgsdfgsdfg sdfgsdfg sdfg sdfg sdf gsdfg sdfg sd"
"asd fasdf asdf asdf asd fasfd as fdaèèèèèèè òòòòòòòòòòòòsfd asdf as fdas ffsd asfd as fdASASASAs D"
"Asfdsf sdfg sdfg dsfg dfg sdfgsdfgsdfgsdfg sdfgsdfg sdfgùùùùùùùùùùùùùù sdfg sdf gsdfg sdfg sd"
"asd fasdf asdf asdf asd fasfd as fdasfd asd@@@@@@@f as fdas ffsd asfd as fdASASASAs D "
"Asfdsf sdfg sdfg dsfg dfg sdfgsdfgsdfgsdfg sdfgsdf€€€€€€€€€€€€€€€€€€g sdfg sdfg sdf gsdfg sdfg sd"
"asd fasdf asdf asdf asd fasfd as fdasfd asdf as fdas ffsd asfd as fdASASASAs D"
"Asfdsf sdfg sdfg dsfg dfg sdfgsdfgsdfgsdfg sdfgsdfg sdfg sdfg sdf gsdfg sdfg sd\n\nlalala\n";

static void
test_length (void)
{
  g_assert (g_utf8_strlen ("1234", -1) == 4);
  g_assert (g_utf8_strlen ("1234", 0) == 0);
  g_assert (g_utf8_strlen ("1234", 1) == 1);
  g_assert (g_utf8_strlen ("1234", 2) == 2);
  g_assert (g_utf8_strlen ("1234", 3) == 3);
  g_assert (g_utf8_strlen ("1234", 4) == 4);
  g_assert (g_utf8_strlen ("1234", 5) == 4);

  g_assert (g_utf8_strlen (longline, -1) == 762);
  g_assert (g_utf8_strlen (longline, strlen (longline)) == 762);
  g_assert (g_utf8_strlen (longline, 1024) == 762);

  g_assert (g_utf8_strlen (NULL, 0) == 0);

  g_assert (g_utf8_strlen ("a\340\250\201c", -1) == 3);
  g_assert (g_utf8_strlen ("a\340\250\201c", 1) == 1);
  g_assert (g_utf8_strlen ("a\340\250\201c", 2) == 1);
  g_assert (g_utf8_strlen ("a\340\250\201c", 3) == 1);
  g_assert (g_utf8_strlen ("a\340\250\201c", 4) == 2);
  g_assert (g_utf8_strlen ("a\340\250\201c", 5) == 3);
}

static void
test_misc (void)
{
  char *s;
  s = g_utf8_strreverse ("1234", -1);
  g_assert (strcmp (s, "4321") == 0);
  g_free (s);
  s = g_utf8_strreverse ("1234", 3);
  g_assert (strcmp (s, "321") == 0);
  g_free (s);
}

int main (int argc, char *argv[])
{
  test_utf8 (longline);
  test_length ();
  test_misc ();
  
  return 0;
}
