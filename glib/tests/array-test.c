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
#include <stdlib.h>
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
array_ref_count (void)
{
  GArray *garray;
  GArray *garray2;
  gint i;

  garray = g_array_new (FALSE, FALSE, sizeof (gint));
  g_assert_cmpint (g_array_get_element_size (garray), ==, sizeof (gint));
  for (i = 0; i < 100; i++)
    g_array_prepend_val (garray, i);

  /* check we can ref, unref and still access the array */
  garray2 = g_array_ref (garray);
  g_assert (garray == garray2);
  g_array_unref (garray2);
  for (i = 0; i < 100; i++)
    g_assert_cmpint (g_array_index (garray, gint, i), ==, (100 - i - 1));

  /* garray2 should be an empty valid GArray wrapper */
  garray2 = g_array_ref (garray);
  g_array_free (garray, TRUE);

  g_assert_cmpint (garray2->len, ==, 0);
  g_array_unref (garray2);
}

static void
array_large_size (void)
{
  GArray* array;

  g_test_bug ("568760");

  array = g_array_new (TRUE, TRUE, sizeof (char));

  /* it might take really long until the allocation happens */
  if (g_test_trap_fork (10 /* s */ * 1000 /* ms */ * 1000 /* µs */, 0))
    {
      g_array_set_size (array, 1073750016);
      exit (0); /* success */
    }

  if (!g_test_trap_has_passed ())
    {
      g_test_trap_assert_stderr ("*failed to allocate 2147483648 bytes*");
    }

  g_array_free (array, TRUE);
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
pointer_array_ref_count (void)
{
  GPtrArray *gparray;
  GPtrArray *gparray2;
  gint i;
  gint sum = 0;

  gparray = g_ptr_array_new ();
  for (i = 0; i < 10000; i++)
    g_ptr_array_add (gparray, GINT_TO_POINTER (i));

  /* check we can ref, unref and still access the array */
  gparray2 = g_ptr_array_ref (gparray);
  g_assert (gparray == gparray2);
  g_ptr_array_unref (gparray2);
  for (i = 0; i < 10000; i++)
    g_assert (g_ptr_array_index (gparray, i) == GINT_TO_POINTER (i));

  g_ptr_array_foreach (gparray, sum_up, &sum);
  g_assert (sum == 49995000);

  /* gparray2 should be an empty valid GPtrArray wrapper */
  gparray2 = g_ptr_array_ref (gparray);
  g_ptr_array_free (gparray, TRUE);

  g_assert_cmpint (gparray2->len, ==, 0);
  g_ptr_array_unref (gparray2);
}

static gint num_free_func_invocations = 0;

static void
my_free_func (gpointer data)
{
  num_free_func_invocations++;
  g_free (data);
}

static void
pointer_array_free_func (void)
{
  GPtrArray *gparray;
  GPtrArray *gparray2;
  gchar **strv;
  gchar *s;

  num_free_func_invocations = 0;
  gparray = g_ptr_array_new_with_free_func (my_free_func);
  g_ptr_array_unref (gparray);
  g_assert_cmpint (num_free_func_invocations, ==, 0);

  gparray = g_ptr_array_new_with_free_func (my_free_func);
  g_ptr_array_free (gparray, TRUE);
  g_assert_cmpint (num_free_func_invocations, ==, 0);

  num_free_func_invocations = 0;
  gparray = g_ptr_array_new_with_free_func (my_free_func);
  g_ptr_array_add (gparray, g_strdup ("foo"));
  g_ptr_array_add (gparray, g_strdup ("bar"));
  g_ptr_array_add (gparray, g_strdup ("baz"));
  g_ptr_array_remove_index (gparray, 0);
  g_assert_cmpint (num_free_func_invocations, ==, 1);
  s = g_strdup ("frob");
  g_ptr_array_add (gparray, s);
  g_assert (g_ptr_array_remove (gparray, s));
  g_assert_cmpint (num_free_func_invocations, ==, 2);
  g_ptr_array_set_size (gparray, 1);
  g_assert_cmpint (num_free_func_invocations, ==, 3);
  g_ptr_array_ref (gparray);
  g_ptr_array_unref (gparray);
  g_assert_cmpint (num_free_func_invocations, ==, 3);
  g_ptr_array_unref (gparray);
  g_assert_cmpint (num_free_func_invocations, ==, 4);

  num_free_func_invocations = 0;
  gparray = g_ptr_array_new_with_free_func (my_free_func);
  g_ptr_array_add (gparray, g_strdup ("foo"));
  g_ptr_array_add (gparray, g_strdup ("bar"));
  g_ptr_array_add (gparray, g_strdup ("baz"));
  g_ptr_array_add (gparray, NULL);
  gparray2 = g_ptr_array_ref (gparray);
  strv = (gchar **) g_ptr_array_free (gparray, FALSE);
  g_assert_cmpint (num_free_func_invocations, ==, 0);
  g_strfreev (strv);
  g_ptr_array_unref (gparray2);
  g_assert_cmpint (num_free_func_invocations, ==, 0);

  num_free_func_invocations = 0;
  gparray = g_ptr_array_new_with_free_func (my_free_func);
  g_ptr_array_add (gparray, g_strdup ("foo"));
  g_ptr_array_add (gparray, g_strdup ("bar"));
  g_ptr_array_add (gparray, g_strdup ("baz"));
  g_ptr_array_unref (gparray);
  g_assert_cmpint (num_free_func_invocations, ==, 3);

  num_free_func_invocations = 0;
  gparray = g_ptr_array_new_with_free_func (my_free_func);
  g_ptr_array_add (gparray, g_strdup ("foo"));
  g_ptr_array_add (gparray, g_strdup ("bar"));
  g_ptr_array_add (gparray, g_strdup ("baz"));
  g_ptr_array_free (gparray, TRUE);
  g_assert_cmpint (num_free_func_invocations, ==, 3);

  num_free_func_invocations = 0;
  gparray = g_ptr_array_new_with_free_func (my_free_func);
  g_ptr_array_add (gparray, "foo");
  g_ptr_array_add (gparray, "bar");
  g_ptr_array_add (gparray, "baz");
  g_ptr_array_set_free_func (gparray, NULL);
  g_ptr_array_free (gparray, TRUE);
  g_assert_cmpint (num_free_func_invocations, ==, 0);
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

static void
byte_array_ref_count (void)
{
  GByteArray *gbarray;
  GByteArray *gbarray2;
  gint i;

  gbarray = g_byte_array_new ();
  for (i = 0; i < 10000; i++)
    g_byte_array_append (gbarray, (guint8*) "abcd", 4);

  gbarray2 = g_byte_array_ref (gbarray);
  g_assert (gbarray2 == gbarray);
  g_byte_array_unref (gbarray2);
  for (i = 0; i < 10000; i++)
    {
      g_assert (gbarray->data[4*i] == 'a');
      g_assert (gbarray->data[4*i+1] == 'b');
      g_assert (gbarray->data[4*i+2] == 'c');
      g_assert (gbarray->data[4*i+3] == 'd');
    }

  gbarray2 = g_byte_array_ref (gbarray);
  g_assert (gbarray2 == gbarray);
  g_byte_array_free (gbarray, TRUE);
  g_assert_cmpint (gbarray2->len, ==, 0);
  g_byte_array_unref (gbarray2);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_bug_base ("http://bugs.gnome.org/%s");

  /* array tests */
  g_test_add_func ("/array/append", array_append);
  g_test_add_func ("/array/prepend", array_prepend);
  g_test_add_func ("/array/ref-count", array_ref_count);
  g_test_add_func ("/array/large-size", array_large_size);

  /* pointer arrays */
  g_test_add_func ("/pointerarray/add", pointer_array_add);
  g_test_add_func ("/pointerarray/ref-count", pointer_array_ref_count);
  g_test_add_func ("/pointerarray/free-func", pointer_array_free_func);

  /* byte arrays */
  g_test_add_func ("/bytearray/append", byte_array_append);
  g_test_add_func ("/bytearray/ref-count", byte_array_ref_count);

  return g_test_run ();
}

