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

#include <stdio.h>
#include <string.h>
#include "glib.h"

static void 
sum_up (gpointer data, 
	gpointer user_data)
{
  gint *sum = (gint *)user_data;

  *sum += GPOINTER_TO_INT (data);
}

static void
array_append (void)
{
  GArray *garray;
  gint i;

  garray = g_array_new (FALSE, FALSE, sizeof (gint));
  for (i = 0; i < 10000; i++)
    g_array_append_val (garray, i);

  for (i = 0; i < 10000; i++)
    g_assert_cmpint (g_array_index (garray, gint, i), ==, i);

  g_array_free (garray, TRUE);
}

static void
array_prepend (void)
{
  GArray *garray;
  gint i;

  garray = g_array_new (FALSE, FALSE, sizeof (gint));
  for (i = 0; i < 100; i++)
    g_array_prepend_val (garray, i);

  for (i = 0; i < 100; i++)
    g_assert_cmpint (g_array_index (garray, gint, i), ==, (100 - i - 1));

  g_array_free (garray, TRUE);
}

static void
pointer_array_add (void)
{
  GPtrArray *gparray;
  gint i;
  gint sum = 0;

  gparray = g_ptr_array_new ();
  for (i = 0; i < 10000; i++)
    g_ptr_array_add (gparray, GINT_TO_POINTER (i));

  for (i = 0; i < 10000; i++)
    g_assert (g_ptr_array_index (gparray, i) == GINT_TO_POINTER (i));
  
  g_ptr_array_foreach (gparray, sum_up, &sum);
  g_assert (sum == 49995000);

  g_ptr_array_free (gparray, TRUE);
}

static void
byte_array_append (void)
{
  GByteArray *gbarray;
  gint i;

  gbarray = g_byte_array_new ();
  for (i = 0; i < 10000; i++)
    g_byte_array_append (gbarray, (guint8*) "abcd", 4);

  for (i = 0; i < 10000; i++)
    {
      g_assert (gbarray->data[4*i] == 'a');
      g_assert (gbarray->data[4*i+1] == 'b');
      g_assert (gbarray->data[4*i+2] == 'c');
      g_assert (gbarray->data[4*i+3] == 'd');
    }

  g_byte_array_free (gbarray, TRUE);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  /* array tests */
  g_test_add_func ("/array/append", array_append);
  g_test_add_func ("/array/prepend", array_prepend);

  /* pointer arrays */
  g_test_add_func ("/pointerarray/add", pointer_array_add);

  /* byte arrays */
  g_test_add_func ("/bytearray/append", byte_array_append);

  return g_test_run ();
}

