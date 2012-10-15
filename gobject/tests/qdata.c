/*
 * Copyright 2012 Red Hat, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * See the included COPYING file for more information.
 */

#include <glib-object.h>

gboolean fail;

#define THREADS 10
#define ROUNDS 10000

GObject *object;
volatile gint bucket[THREADS];

static gpointer
thread_func (gpointer data)
{
  gint idx = GPOINTER_TO_INT (data);
  gint i;
  gint d;
  gint value;
  gint new_value;

  for (i = 0; i < ROUNDS; i++)
    {
      d = g_random_int_range (-10, 100);
      bucket[idx] += d;
retry:
      value = GPOINTER_TO_INT (g_object_get_data (object, "test"));
      new_value = value + d;
      if (fail)
        g_object_set_data (object, "test", GINT_TO_POINTER (new_value));
      else
        {
          if (!g_object_replace_data (object, "test",
                                      GINT_TO_POINTER (value),
                                      GINT_TO_POINTER (new_value),
                                      NULL, NULL))
            goto retry;
        }
      g_thread_yield ();
    }

  return NULL;
}

static void
test_qdata_threaded (void)
{
  gint sum;
  gint i;
  GThread *threads[THREADS];
  gint result;

  object = g_object_new (G_TYPE_OBJECT, NULL);
  g_object_set_data (object, "test", GINT_TO_POINTER (0));

  for (i = 0; i < THREADS; i++)
    bucket[i] = 0;

  for (i = 0; i < THREADS; i++)
    threads[i] = g_thread_new ("qdata", thread_func, GINT_TO_POINTER (i));

  for (i = 0; i < THREADS; i++)
    g_thread_join (threads[i]);

  sum = 0;
  for (i = 0; i < THREADS; i++)
    sum += bucket[i];

  result = GPOINTER_TO_INT (g_object_get_data (object, "test"));

  g_assert_cmpint (sum, ==, result);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  fail = !!g_getenv ("FAIL");

  g_test_add_func ("/qdata/threaded", test_qdata_threaded);

  return g_test_run ();
}
