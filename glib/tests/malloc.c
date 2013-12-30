/* Unit tests for g_malloc
 * Copyright (C) 2013 Red Hat, Inc.
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In no event shall the authors or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 * Author: Matthias Clasen
 */

#include "glib.h"
#include <stdlib.h>

static void
test_profiler (void)
{
  if (g_test_subprocess ())
    {
      gpointer p;
      g_mem_set_vtable (glib_mem_profiler_table);
      p = g_malloc (100);
      p = g_realloc (p, 200);
      g_free (p);
      p = g_malloc0 (1000);
      g_free (p);
      p = g_try_malloc (2000);
      p = g_try_realloc (p, 3000);
      g_free (p);
      p = g_malloc (0);
      p = g_malloc0 (0);
      p = g_realloc (NULL, 0);
      p = g_try_malloc (0);
      p = g_try_realloc (NULL, 0);
      g_mem_profile ();
      exit (0);
    }
  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_passed ();
  g_test_trap_assert_stdout ("*GLib Memory statistics*");
}

static void
test_fallback_calloc (void)
{
  if (g_test_subprocess ())
    {
      GMemVTable vtable = { malloc, realloc, free, NULL, NULL, NULL };
      gpointer p;

      g_mem_set_vtable (&vtable);
      p = g_malloc0 (1000);
      g_free (p);
      exit (0);
    }
  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_passed ();
}

static void
test_incomplete_vtable (void)
{
  if (g_test_subprocess ())
    {
      GMemVTable vtable = { malloc, realloc, NULL, NULL, NULL, NULL };
      gpointer p;

      g_mem_set_vtable (&vtable);
      p = g_malloc0 (1000);
      g_free (p);
      exit (0);
    }
  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*lacks one of*");
}

static void
test_double_vtable (void)
{
  if (g_test_subprocess ())
    {
      GMemVTable vtable = { malloc, realloc, free, NULL, NULL, NULL };

      g_mem_set_vtable (&vtable);
      g_mem_set_vtable (&vtable);
      exit (0);
    }
  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*can only be set once*");
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/malloc/incomplete-vtable", test_incomplete_vtable);
  g_test_add_func ("/malloc/double-vtable", test_double_vtable);
  g_test_add_func ("/malloc/fallback-calloc", test_fallback_calloc);
  g_test_add_func ("/malloc/profiler", test_profiler);

  return g_test_run ();
}
