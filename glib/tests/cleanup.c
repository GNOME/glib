/*
 * Copyright 2011 Red Hat, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * See the included COPYING file for more information.
 */

#undef G_CLEANUP_SCOPE
#define G_CLEANUP_SCOPE my_cleanup

#include <glib.h>
#include <string.h>

GCleanupScope G_CLEANUP_SCOPE[1] = { { G_CLEANUP_SCOPE_FORCE, } };

static void
cleanup_one (gpointer data)
{
  gint *value = data;
  g_assert_cmpint (*value, ==, 1);
  (*value)++;
}

static void
cleanup_two (gpointer data)
{
  gint *value = data;
  g_assert_cmpint (*value, ==, 2);
  (*value)++;
}

static void
cleanup_three (gpointer data)
{
  gint *value = data;
  g_assert_cmpint (*value, ==, 3);
  (*value)++;
}

static void
cleanup_four (gpointer data)
{
  gint *value = data;
  g_assert_cmpint (*value, ==, 4);
  (*value)++;
}

static void
cleanup_two_and_push_three (gpointer data)
{
  cleanup_two (data);

  /* Push another item */
  G_CLEANUP_IN (data, cleanup_three, G_CLEANUP_PHASE_DEFAULT);
}

static void
test_push_and_clean (void)
{
  gint value = 1;

  /* This tests ordering of pushing three, and having it run before four is executed */
  G_CLEANUP_IN (&value, cleanup_one, G_CLEANUP_PHASE_EARLY);
  G_CLEANUP_IN (&value, cleanup_two_and_push_three, G_CLEANUP_PHASE_DEFAULT);
  G_CLEANUP_IN (&value, cleanup_four, G_CLEANUP_PHASE_LATE);

  g_cleanup_clean (my_cleanup);

  g_assert_cmpint (value, ==, 5);
}

static void
test_push_and_remove (void)
{
  gint value = 1;
  gpointer item;
  gpointer item2;
  gpointer func;
  gpointer data;

  /* This tests ordering of pushing three, and having it run before four is executed */
  G_CLEANUP_IN (&value, cleanup_one, G_CLEANUP_PHASE_EARLY);

  item = g_cleanup_push (my_cleanup, G_CLEANUP_PHASE_EARLY, cleanup_three, &value);
  item2 = g_cleanup_push (my_cleanup, G_CLEANUP_PHASE_EARLY, cleanup_four, &value);

  G_CLEANUP_IN (&value, cleanup_two, G_CLEANUP_PHASE_DEFAULT);
  G_CLEANUP_IN (item2, g_cleanup_remove, G_CLEANUP_PHASE_EARLY);

  G_CLEANUP_IN (&value, cleanup_three, G_CLEANUP_PHASE_DEFAULT + 1);
  G_CLEANUP_IN (&value, cleanup_four, G_CLEANUP_PHASE_LATE);

  /* One remove happens before clean */
  func = g_cleanup_steal (item, &data);
  g_assert (func == cleanup_three);
  g_assert (data == &value);

  g_cleanup_clean (my_cleanup);

  /* The other remove happened during clean */

  g_assert_cmpint (value, ==, 5);
}

static void
cleanup_pointer (gchar *value)
{
  g_assert_cmpstr (value, ==, "blah");
  memcpy (value, "alot", 4);
}

static void
test_pointer (void)
{
  gchar buf_one[] = "blah";
  gchar *pointer_one = buf_one;
  gchar *pointer_two = NULL;
  gchar buf_three[] = "blah";
  gchar *pointer_three = buf_three;

  gpointer item;

  /* This tests ordering of pushing three, and having it run before four is executed */
  G_CLEANUP_POINTER_IN (&pointer_one, cleanup_pointer, 0);
  G_CLEANUP_POINTER_IN (&pointer_two, cleanup_pointer, 0);

  item = g_cleanup_push_pointer (my_cleanup, 0, (GCleanupFunc)cleanup_pointer, (gpointer *)&pointer_three);
  G_CLEANUP_IN (item, g_cleanup_remove, G_CLEANUP_PHASE_EARLY);

  g_cleanup_clean (my_cleanup);

  g_assert_cmpstr (buf_one, ==, "alot");
  g_assert (pointer_one == NULL);
  g_assert (pointer_two == NULL);
  g_assert_cmpstr (buf_three, ==, "blah");
  g_assert (pointer_three == buf_three);
}

static void
test_source_cleaned (void)
{
  static GSourceFuncs funcs = { NULL, };
  GSource *source;
  guint id;

  source = g_source_new (&funcs, sizeof (GSource));
  id = g_source_attach (source, NULL);
  g_cleanup_push_source (my_cleanup, 0, source);
  g_source_unref (source);

  g_assert (g_main_context_find_source_by_id (NULL, id) != NULL);

  g_cleanup_clean (my_cleanup);

  g_assert (g_main_context_find_source_by_id (NULL, id) == NULL);
}

static void
test_source_destroyed (void)
{
  static GSourceFuncs funcs = { NULL, };
  GSource *source;
  guint id;

  source = g_source_new (&funcs, sizeof (GSource));
  id = g_source_attach (source, NULL);
  g_cleanup_push_source (my_cleanup, 0, source);
  g_source_unref (source);

  g_assert (g_main_context_find_source_by_id (NULL, id) != NULL);
  g_source_destroy (source);

  g_assert (g_main_context_find_source_by_id (NULL, id) == NULL);

  g_cleanup_clean (my_cleanup);

  /* We're really checking for invalid memory access in this test */
}


int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/cleanup/push-and-clean", test_push_and_clean);
  g_test_add_func ("/cleanup/push-and-remove", test_push_and_remove);
  g_test_add_func ("/cleanup/pointer", test_pointer);
  g_test_add_func ("/cleanup/source-cleaned", test_source_cleaned);
  g_test_add_func ("/cleanup/source-destroyed", test_source_destroyed);

  return g_test_run ();
}
