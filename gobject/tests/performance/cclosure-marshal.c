/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright © 2026 Christian Hergert
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdint.h>
#include <stdlib.h>

#include <glib-object.h>

#define DEFAULT_ITERATIONS 5000000
#define DEFAULT_N_CLOSURES 100000
#define DEFAULT_N_SAMPLES 7
#define MAX_N_SAMPLES 100

/*
 * The repeated-closure case measures steady-state reuse by one call site.
 *
 * The one-shot case deliberately invokes each closure only once. It therefore
 * distinguishes a plan cached on an individual GClosure from a plan shared by
 * all uses of g_cclosure_marshal_generic() with the same ABI signature.
 */
typedef struct
{
  GValue params[4];
  GValue result;
} Invocation;

static int64_t n_iterations = DEFAULT_ITERATIONS;
static int n_closures = DEFAULT_N_CLOSURES;
static int n_samples = DEFAULT_N_SAMPLES;

static int64_t
benchmark_callback (gpointer instance,
                    int      int_arg,
                    uint64_t uint64_arg,
                    double   double_arg,
                    gpointer user_data)
{
  return (int64_t) GPOINTER_TO_SIZE (instance) +
         int_arg +
         (int64_t) uint64_arg +
         (int64_t) double_arg +
         (int64_t) GPOINTER_TO_SIZE (user_data);
}

static GClosure *
create_closure (void)
{
  GClosure *closure;

  closure = g_cclosure_new (G_CALLBACK (benchmark_callback), GSIZE_TO_POINTER (5), NULL);
  g_closure_set_marshal (closure, g_cclosure_marshal_generic);
  g_closure_ref (closure);
  g_closure_sink (closure);

  return closure;
}

static void
invocation_init (Invocation *invocation)
{
  invocation->params[0] = (GValue) G_VALUE_INIT;
  g_value_init (&invocation->params[0], G_TYPE_POINTER);
  g_value_set_pointer (&invocation->params[0], GSIZE_TO_POINTER (1));

  invocation->params[1] = (GValue) G_VALUE_INIT;
  g_value_init (&invocation->params[1], G_TYPE_INT);
  g_value_set_int (&invocation->params[1], 2);

  invocation->params[2] = (GValue) G_VALUE_INIT;
  g_value_init (&invocation->params[2], G_TYPE_UINT64);
  g_value_set_uint64 (&invocation->params[2], 3);

  invocation->params[3] = (GValue) G_VALUE_INIT;
  g_value_init (&invocation->params[3], G_TYPE_DOUBLE);
  g_value_set_double (&invocation->params[3], 4.0);

  invocation->result = (GValue) G_VALUE_INIT;
  g_value_init (&invocation->result, G_TYPE_INT64);
}

static void
invocation_clear (Invocation *invocation)
{
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (invocation->params); i++)
    g_value_unset (&invocation->params[i]);

  g_value_unset (&invocation->result);
}

static inline void
invoke_closure (GClosure   *closure,
                Invocation *invocation)
{
  g_closure_invoke (closure,
                    &invocation->result,
                    G_N_ELEMENTS (invocation->params),
                    invocation->params,
                    NULL);
}

static int
compare_double (const void *a,
                const void *b)
{
  const double *da = a;
  const double *db = b;

  return (*da > *db) - (*da < *db);
}

static double
elapsed_ns_per_invocation (int64_t started,
                           int64_t stopped,
                           int64_t operations)
{
  return ((double) (stopped - started) * 1000.0) / (double) operations;
}

static void
print_samples (const char *name,
               double     samples[])
{
  double minimum;
  double median;

  qsort (samples, n_samples, sizeof (samples[0]), compare_double);
  minimum = samples[0];
  median = samples[n_samples / 2];

  g_print ("%-32s %8.2f ns/invocation (best %8.2f)\n", name, median, minimum);
}

static void
benchmark_repeated_closure (Invocation *invocation)
{
  GClosure *closure;
  double samples[MAX_N_SAMPLES];
  int sample;

  closure = create_closure ();

  /* Populate any lazy cache before measuring steady-state invocation. */
  invoke_closure (closure, invocation);

  for (sample = 0; sample < n_samples; sample++)
    {
      int64_t started;
      int64_t stopped;
      int64_t i;

      started = g_get_monotonic_time ();
      for (i = 0; i < n_iterations; i++)
        invoke_closure (closure, invocation);
      stopped = g_get_monotonic_time ();

      samples[sample] = elapsed_ns_per_invocation (started, stopped, n_iterations);
    }

  g_assert_cmpint (g_value_get_int64 (&invocation->result), ==, 15);
  print_samples ("one closure, repeated", samples);
  g_closure_unref (closure);
}

static void
benchmark_one_shot_closures (Invocation *invocation)
{
  double samples[MAX_N_SAMPLES];
  int sample;

  for (sample = 0; sample < n_samples; sample++)
    {
      GClosure **closures;
      int64_t started;
      int64_t stopped;
      int i;

      closures = g_new (GClosure *, n_closures);
      for (i = 0; i < n_closures; i++)
        closures[i] = create_closure ();

      /*
       * Do not warm these closures: each call site is intentionally invoked
       * once. A plan cache local to GClosure cannot help this case, while a
       * cache shared by equivalent signatures can reuse a plan prepared by
       * the preceding benchmark (and by earlier closures in this batch).
       */
      started = g_get_monotonic_time ();
      for (i = 0; i < n_closures; i++)
        invoke_closure (closures[i], invocation);
      stopped = g_get_monotonic_time ();

      samples[sample] = elapsed_ns_per_invocation (started, stopped, n_closures);

      for (i = 0; i < n_closures; i++)
        g_closure_unref (closures[i]);
      g_free (closures);
    }

  g_assert_cmpint (g_value_get_int64 (&invocation->result), ==, 15);
  print_samples ("shared signature, one shot", samples);
}

int
main (int   argc,
      char *argv[])
{
  GOptionContext *context;
  GError *error = NULL;
  Invocation invocation;
  GOptionEntry entries[] = {
    {
      "iterations", 'i', 0, G_OPTION_ARG_INT64, &n_iterations,
      "Invocations in each repeated-closure sample", "N"
    },
    {
      "closures", 'c', 0, G_OPTION_ARG_INT, &n_closures,
      "Distinct closures in each one-shot sample", "N"
    },
    {
      "samples", 's', 0, G_OPTION_ARG_INT, &n_samples,
      "Samples per benchmark", "N"
    },
    G_OPTION_ENTRY_NULL
  };

  context = g_option_context_new ("- benchmark g_cclosure_marshal_generic()");
  g_option_context_add_main_entries (context, entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_clear_error (&error);
      g_option_context_free (context);
      return EXIT_FAILURE;
    }

  g_option_context_free (context);

  if (n_iterations <= 0 ||
      n_closures <= 0 ||
      n_samples <= 0 ||
      n_samples > MAX_N_SAMPLES)
    {
      g_printerr ("iterations and closures must be positive; samples must be in [1, %d]\n",
                  MAX_N_SAMPLES);
      return EXIT_FAILURE;
    }

  invocation_init (&invocation);

  g_print ("Mixed signature: pointer, int, uint64, double, closure data -> int64\n");
  g_print ("Median of %d samples; lower is better\n\n", n_samples);

  benchmark_repeated_closure (&invocation);
  benchmark_one_shot_closures (&invocation);

  invocation_clear (&invocation);

  return EXIT_SUCCESS;
}
