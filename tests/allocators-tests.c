/*
 * Copyright (C) 2022 Canonical Ltd.
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
 *
 * Author: Marco Trevisan <marco.trevisan@canonical.com>
 */

#include <glib.h>

/* These are to please CI, should be adjusted smartly */
#define SIMPLE_TYPE_ITERATIONS 100000000
#if defined(G_OS_UNIX) && !__APPLE__
#define POINTERS_ARRAY_SIZE 1000000
#elif defined(__APPLE__)
#define POINTERS_ARRAY_SIZE 100000
#else
#define POINTERS_ARRAY_SIZE 10000
#endif
#define MAX_ALLOCATED_SIZE (POINTERS_ARRAY_SIZE * 512)

#ifdef G_USE_SYSTEM_ALLOCATOR
#define instance_alloc(s) g_malloc ((s))
#define instance_alloc0(s) g_malloc0 ((s))
#define instance_free(s, p) g_free ((p))
#else
#define instance_alloc(s) g_slice_alloc ((s))
#define instance_alloc0(s) g_slice_alloc0 ((s))
#define instance_free(s, p) g_slice_free1 ((s), (p))
#endif

static void
csv_report (const char *allocator, gint iterations, gdouble time_elapsed)
{
  g_test_message ("CSV: %s/%s/%d,%f",
                  g_test_get_path (), allocator, iterations, time_elapsed);
}

#define allocate_and_free_many(type, type_size, allocator, N) \
  {                                                           \
    guint i;                                                  \
                                                              \
    for (i = 0; i < N; i++)                                   \
      {                                                       \
        gpointer ptr = allocator (type_size);                 \
        instance_free (type_size, ptr);                       \
      }                                                       \
  }

#define test_alloc_and_free(type, type_size, allocator, N)                           \
  {                                                                                  \
    gdouble time_elapsed;                                                            \
                                                                                     \
    g_test_timer_start ();                                                           \
                                                                                     \
    allocate_and_free_many (type, type_size, allocator, N);                          \
                                                                                     \
    time_elapsed = g_test_timer_elapsed ();                                          \
    g_test_minimized_result (time_elapsed,                                           \
                             "Allocated and free'd %u instances of %s "              \
                             "(size: %" G_GSIZE_FORMAT " using %s in %6.5f seconds", \
                             N, #type, (gsize) type_size, #allocator, time_elapsed); \
    csv_report (#allocator, N, time_elapsed);                                        \
  }

#define test_alloc_many_and_free(type, type_size, allocator, N)                      \
  {                                                                                  \
    guint i;                                                                         \
    gdouble time_elapsed;                                                            \
    gpointer *ptrs[N] = { 0 };                                                       \
    gdouble total_time = 0;                                                          \
                                                                                     \
    g_test_timer_start ();                                                           \
                                                                                     \
    for (i = 0; i < N; i++)                                                          \
      ptrs[i] = allocator (type_size);                                               \
                                                                                     \
    time_elapsed = g_test_timer_elapsed ();                                          \
    total_time += time_elapsed;                                                      \
                                                                                     \
    g_test_minimized_result (time_elapsed,                                           \
                             "Allocated %u instances of %s (size: %" G_GSIZE_FORMAT  \
                             ") using %s in %6.5f seconds",                          \
                             N, #type, (gsize) type_size, #allocator, time_elapsed); \
    csv_report (#allocator, N, time_elapsed);                                        \
                                                                                     \
    g_test_timer_start ();                                                           \
                                                                                     \
    for (i = 0; i < N; i++)                                                          \
      instance_free (type_size, ptrs[i]);                                            \
                                                                                     \
    time_elapsed = g_test_timer_elapsed ();                                          \
    total_time += time_elapsed;                                                      \
                                                                                     \
    g_test_minimized_result (time_elapsed,                                           \
                             "Free'd %u instances of %s in %6.5f seconds",           \
                             N, #type, time_elapsed);                                \
    csv_report ("free", N, time_elapsed);                                            \
                                                                                     \
    g_test_minimized_result (total_time,                                             \
                             "Allocated and Free'd %u instances of %s using %s "     \
                             "in %6.5f seconds",                                     \
                             N, #type, #allocator, total_time);                      \
                                                                                     \
    csv_report (#allocator "+free", N, total_time);                                  \
  }

#define test_allocation_simple_type(type)                                                 \
  static void                                                                             \
      test_allocation_##type (void)                                                       \
  {                                                                                       \
    test_alloc_and_free (type, sizeof (type), instance_alloc, SIMPLE_TYPE_ITERATIONS);    \
    test_alloc_and_free (type, sizeof (type), instance_alloc0, SIMPLE_TYPE_ITERATIONS);   \
                                                                                          \
    test_alloc_many_and_free (type, sizeof (type), instance_alloc, POINTERS_ARRAY_SIZE);  \
    test_alloc_many_and_free (type, sizeof (type), instance_alloc0, POINTERS_ARRAY_SIZE); \
  }

#define test_allocation_sized(size)                                                      \
  static void                                                                            \
      test_allocation_sized_##size (void)                                                \
  {                                                                                      \
    test_alloc_and_free ("struct" #size, size, instance_alloc, SIMPLE_TYPE_ITERATIONS);  \
    test_alloc_and_free ("struct" #size, size, instance_alloc0, SIMPLE_TYPE_ITERATIONS); \
                                                                                         \
    test_alloc_many_and_free ("struct" #size, size, instance_alloc,                      \
                              MIN (POINTERS_ARRAY_SIZE, MAX_ALLOCATED_SIZE / size));     \
    test_alloc_many_and_free ("struct" #size, size, instance_alloc0,                     \
                              MIN (POINTERS_ARRAY_SIZE, MAX_ALLOCATED_SIZE / size));     \
  }

#define test_allocate_and_free_many_mixed(max_steps, allocator, N)                    \
  {                                                                                   \
    guint i;                                                                          \
    gdouble time_elapsed;                                                             \
    GArray *steps_size = g_array_sized_new (FALSE, FALSE, sizeof (gsize), max_steps); \
                                                                                      \
    for (i = 0; i < max_steps; i++)                                                   \
      {                                                                               \
        gsize size = 1 << (i + 1);                                                    \
        if (g_test_verbose ())                                                        \
          g_test_message ("Step allocation size %" G_GSIZE_FORMAT, size);             \
        g_array_insert_val (steps_size, i, size);                                     \
      }                                                                               \
                                                                                      \
    g_test_timer_start ();                                                            \
                                                                                      \
    for (i = 0; i < N; i++)                                                           \
      {                                                                               \
        gsize element_size = g_array_index (steps_size, gsize, (i % max_steps));      \
        gpointer ptr = allocator (element_size);                                      \
        instance_free (element_size, ptr);                                            \
      }                                                                               \
                                                                                      \
    time_elapsed = g_test_timer_elapsed ();                                           \
    g_test_minimized_result (time_elapsed,                                            \
                             "Allocated and free'd %u instances of mixed types "      \
                             "(step: %s) using %s in %6.5f seconds",                  \
                             N, #max_steps, #allocator, time_elapsed);                \
    csv_report (#allocator, N, time_elapsed);                                         \
  }

#define test_allocation_mixed_sizes(max_steps)                     \
  static void                                                      \
      test_allocation_mixed_step_##max_steps (void)                \
  {                                                                \
    gsize step_size = 0;                                           \
    gint i;                                                        \
                                                                   \
    for (i = 0; i < max_steps; i++)                                \
      step_size += 1 << (i + 1);                                   \
    (void) step_size;                                              \
                                                                   \
    test_allocate_and_free_many_mixed (max_steps, instance_alloc,  \
                                       SIMPLE_TYPE_ITERATIONS);    \
    test_allocate_and_free_many_mixed (max_steps, instance_alloc0, \
                                       SIMPLE_TYPE_ITERATIONS);    \
  }

test_allocation_simple_type (gchar);
test_allocation_simple_type (gshort);
test_allocation_simple_type (glong);
test_allocation_simple_type (gint);
test_allocation_simple_type (gboolean);
test_allocation_simple_type (guchar);
test_allocation_simple_type (gushort);
test_allocation_simple_type (gulong);
test_allocation_simple_type (guint);
test_allocation_simple_type (gfloat);
test_allocation_simple_type (gdouble);
test_allocation_simple_type (gpointer);

test_allocation_sized (32);
test_allocation_sized (64);
test_allocation_sized (128);
test_allocation_sized (256);
test_allocation_sized (512);
test_allocation_sized (1024);
test_allocation_sized (2048);
test_allocation_sized (4096);

test_allocation_mixed_sizes (8);
test_allocation_mixed_sizes (12);

#ifdef G_USE_SYSTEM_ALLOCATOR
#define TEST_BASENAME "/allocation/system"
#else
#define TEST_BASENAME "/allocation/gslice"
#endif

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  g_test_message ("GSlice will use a chunk size of %" G_GINT64_FORMAT,
                  g_slice_get_config (G_SLICE_CONFIG_CHUNK_SIZES));
  G_GNUC_END_IGNORE_DEPRECATIONS;

  g_test_add_func (TEST_BASENAME "/simple-type/gchar", test_allocation_gchar);
  g_test_add_func (TEST_BASENAME "/simple-type/gshort", test_allocation_gshort);
  g_test_add_func (TEST_BASENAME "/simple-type/glong", test_allocation_glong);
  g_test_add_func (TEST_BASENAME "/simple-type/gint", test_allocation_gint);
  g_test_add_func (TEST_BASENAME "/simple-type/gboolean", test_allocation_gboolean);
  g_test_add_func (TEST_BASENAME "/simple-type/guchar", test_allocation_guchar);
  g_test_add_func (TEST_BASENAME "/simple-type/gushort", test_allocation_gushort);
  g_test_add_func (TEST_BASENAME "/simple-type/gulong", test_allocation_gulong);
  g_test_add_func (TEST_BASENAME "/simple-type/guint", test_allocation_guint);
  g_test_add_func (TEST_BASENAME "/simple-type/gfloat", test_allocation_gfloat);
  g_test_add_func (TEST_BASENAME "/simple-type/gdouble", test_allocation_gdouble);
  g_test_add_func (TEST_BASENAME "/simple-type/gpointer", test_allocation_gpointer);

  /* FIXME: Depending on the OS we should only test up to the size that the
   * GSlice would support, otherwise we'd get system allocator anyways.
   */
  g_test_add_func (TEST_BASENAME "/sized/32", test_allocation_sized_32);
  g_test_add_func (TEST_BASENAME "/sized/64", test_allocation_sized_64);
  g_test_add_func (TEST_BASENAME "/sized/128", test_allocation_sized_128);
  g_test_add_func (TEST_BASENAME "/sized/256", test_allocation_sized_256);
  g_test_add_func (TEST_BASENAME "/sized/512", test_allocation_sized_512);
  g_test_add_func (TEST_BASENAME "/sized/1024", test_allocation_sized_1024);
  g_test_add_func (TEST_BASENAME "/sized/2048", test_allocation_sized_2048);
  g_test_add_func (TEST_BASENAME "/sized/4096", test_allocation_sized_4096);

  g_test_add_func (TEST_BASENAME "/mixed/8", test_allocation_mixed_step_8);
  g_test_add_func (TEST_BASENAME "/mixed/12", test_allocation_mixed_step_12);

  return g_test_run ();
}
