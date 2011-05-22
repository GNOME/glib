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

#include <glib.h>

#define THREADS 10
#define ROUNDS 10000

volatile gint bucket[THREADS];
volatile gint atomic;

static gpointer
thread_func (gpointer data)
{
  gint idx = GPOINTER_TO_INT (data);
  gint i;
  gint d;

  for (i = 0; i < ROUNDS; i++)
    {
      d = g_random_int_range (-10, 100);
      bucket[idx] += d;
      g_atomic_int_add (&atomic, d);
      g_thread_yield ();
    }

  return NULL;
}

static void
test_atomic (void)
{
  gint sum;
  gint i;
  GThread *threads[THREADS];

  atomic = 0;
  for (i = 0; i < THREADS; i++)
    bucket[i] = 0;

  for (i = 0; i < THREADS; i++)
    threads[i] = g_thread_create (thread_func, GINT_TO_POINTER (i), TRUE, NULL);

  for (i = 0; i < THREADS; i++)
    g_thread_join (threads[i]);

  sum = 0;
  for (i = 0; i < THREADS; i++)
    sum += bucket[i];

  g_assert_cmpint (sum, ==, atomic);
}

int
main (int argc, char *argv[])
{
  g_thread_init (NULL);

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/glib/atomic/add", test_atomic);

  return g_test_run ();
}
