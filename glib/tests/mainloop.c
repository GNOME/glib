/* Unit tests for GMainLoop
 * Copyright (C) 2011 Red Hat, Inc
 * Author: Matthias Clasen
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
 */

#include <glib.h>

static gboolean cb (gpointer data)
{
  return FALSE;
}

static void
test_maincontext_basic (void)
{
  GMainContext *ctx;
  GSource *source;
  GSourceFuncs funcs;
  guint id;
  gpointer data = &funcs;

  ctx = g_main_context_new ();

  g_assert (!g_main_context_pending (ctx));
  g_assert (!g_main_context_iteration (ctx, FALSE));

  source = g_source_new (&funcs, sizeof (GSource));
  g_assert (g_source_get_priority (source) == G_PRIORITY_DEFAULT);

  g_assert (!g_source_get_can_recurse (source));
  g_assert (g_source_get_name (source) == NULL);

  g_source_set_can_recurse (source, TRUE);
  g_source_set_name (source, "d");

  g_assert (g_source_get_can_recurse (source));
  g_assert_cmpstr (g_source_get_name (source), ==, "d");

  g_assert (g_main_context_find_source_by_user_data (ctx, NULL) == NULL);
  g_assert (g_main_context_find_source_by_funcs_user_data (ctx, &funcs, NULL) == NULL);

  id = g_source_attach (source, ctx);
  g_assert_cmpint (g_source_get_id (source), ==, id);
  g_assert (g_main_context_find_source_by_id (ctx, id) == source);

  g_source_set_priority (source, G_PRIORITY_HIGH);
  g_assert (g_source_get_priority (source) == G_PRIORITY_HIGH);

  g_main_context_unref (ctx);

  ctx = g_main_context_default ();
  source = g_source_new (&funcs, sizeof (GSource));
  g_source_set_funcs (source, &funcs);
  g_source_set_callback (source, cb, data, NULL);
  id = g_source_attach (source, ctx);
  g_source_set_name_by_id (id, "e");
  g_assert_cmpstr (g_source_get_name (source), ==, "e");
  g_assert (g_source_remove_by_funcs_user_data (&funcs, data));
}

static void
test_mainloop_basic (void)
{
  GMainLoop *loop;
  GMainContext *ctx;

  loop = g_main_loop_new (NULL, FALSE);

  g_assert (!g_main_loop_is_running (loop));

  g_main_loop_ref (loop);

  ctx = g_main_loop_get_context (loop);
  g_assert (ctx == g_main_context_default ());

  g_main_loop_unref (loop);

  g_assert (g_main_depth () == 0);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/maincontext/basic", test_maincontext_basic);
  g_test_add_func ("/mainloop/basic", test_mainloop_basic);

  return g_test_run ();
}
