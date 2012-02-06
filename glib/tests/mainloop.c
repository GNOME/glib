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

static gboolean prepare (GSource *source, gint *time)
{
  return FALSE;
}
static gboolean check (GSource *source)
{
  return FALSE;
}
static gboolean dispatch (GSource *source, GSourceFunc cb, gpointer date)
{
  return FALSE;
}

GSourceFuncs funcs = {
  prepare,
  check,
  dispatch,
  NULL
};

static void
test_maincontext_basic (void)
{
  GMainContext *ctx;
  GSource *source;
  guint id;
  gpointer data = &funcs;

  ctx = g_main_context_new ();

  g_assert (!g_main_context_pending (ctx));
  g_assert (!g_main_context_iteration (ctx, FALSE));

  source = g_source_new (&funcs, sizeof (GSource));
  g_assert (g_source_get_priority (source) == G_PRIORITY_DEFAULT);
  g_assert (!g_source_is_destroyed (source));

  g_assert (!g_source_get_can_recurse (source));
  g_assert (g_source_get_name (source) == NULL);

  g_source_set_can_recurse (source, TRUE);
  g_source_set_name (source, "d");

  g_assert (g_source_get_can_recurse (source));
  g_assert_cmpstr (g_source_get_name (source), ==, "d");

  g_assert (g_main_context_find_source_by_user_data (ctx, NULL) == NULL);
  g_assert (g_main_context_find_source_by_funcs_user_data (ctx, &funcs, NULL) == NULL);

  id = g_source_attach (source, ctx);
  g_source_unref (source);
  g_assert_cmpint (g_source_get_id (source), ==, id);
  g_assert (g_main_context_find_source_by_id (ctx, id) == source);

  g_source_set_priority (source, G_PRIORITY_HIGH);
  g_assert (g_source_get_priority (source) == G_PRIORITY_HIGH);

  g_source_destroy (source);
  g_main_context_unref (ctx);

  ctx = g_main_context_default ();
  source = g_source_new (&funcs, sizeof (GSource));
  g_source_set_funcs (source, &funcs);
  g_source_set_callback (source, cb, data, NULL);
  id = g_source_attach (source, ctx);
  g_source_unref (source);
  g_source_set_name_by_id (id, "e");
  g_assert_cmpstr (g_source_get_name (source), ==, "e");
  g_assert (g_source_get_context (source) == ctx);
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

  g_main_loop_unref (loop);
}

static gint a;
static gint b;
static gint c;

static gboolean
count_calls (gpointer data)
{
  gint *i = data;

  (*i)++;

  return TRUE;
}

static void
test_timeouts (void)
{
  GMainContext *ctx;
  GMainLoop *loop;
  GSource *source;

  a = b = c = 0;

  ctx = g_main_context_new ();
  loop = g_main_loop_new (ctx, FALSE);

  source = g_timeout_source_new (100);
  g_source_set_callback (source, count_calls, &a, NULL);
  g_source_attach (source, ctx);
  g_source_unref (source);

  source = g_timeout_source_new (250);
  g_source_set_callback (source, count_calls, &b, NULL);
  g_source_attach (source, ctx);
  g_source_unref (source);

  source = g_timeout_source_new (330);
  g_source_set_callback (source, count_calls, &c, NULL);
  g_source_attach (source, ctx);
  g_source_unref (source);

  source = g_timeout_source_new (1050);
  g_source_set_callback (source, (GSourceFunc)g_main_loop_quit, loop, NULL);
  g_source_attach (source, ctx);
  g_source_unref (source);

  g_main_loop_run (loop);

  g_assert (a == 10);
  g_assert (b == 4);
  g_assert (c == 3);

  g_main_loop_unref (loop);
  g_main_context_unref (ctx);
}

static void
test_priorities (void)
{
  GMainContext *ctx;
  GSource *sourcea;
  GSource *sourceb;

  a = b = c = 0;

  ctx = g_main_context_new ();

  sourcea = g_idle_source_new ();
  g_source_set_callback (sourcea, count_calls, &a, NULL);
  g_source_set_priority (sourcea, 1);
  g_source_attach (sourcea, ctx);
  g_source_unref (sourcea);

  sourceb = g_idle_source_new ();
  g_source_set_callback (sourceb, count_calls, &b, NULL);
  g_source_set_priority (sourceb, 0);
  g_source_attach (sourceb, ctx);
  g_source_unref (sourceb);

  g_assert (g_main_context_pending (ctx));
  g_assert (g_main_context_iteration (ctx, FALSE));
  g_assert (a == 0);
  g_assert (b == 1);

  g_assert (g_main_context_iteration (ctx, FALSE));
  g_assert (a == 0);
  g_assert (b == 2);

  g_source_destroy (sourceb);

  g_assert (g_main_context_iteration (ctx, FALSE));
  g_assert (a == 1);
  g_assert (b == 2);

  g_assert (g_main_context_pending (ctx));
  g_source_destroy (sourcea);
  g_assert (!g_main_context_pending (ctx));

  g_main_context_unref (ctx);
}

static gint count;

static gboolean
func (gpointer data)
{
  if (data != NULL)
    g_assert (data == g_thread_self ());

  count++;

  return FALSE;
}

static gboolean
call_func (gpointer data)
{
  func (g_thread_self ());

  return G_SOURCE_REMOVE;
}

static gpointer
thread_func (gpointer data)
{
  GMainContext *ctx = data;
  GSource *source;

  g_main_context_push_thread_default (ctx);

  source = g_timeout_source_new (500);
  g_source_set_callback (source, (GSourceFunc)g_thread_exit, NULL, NULL);
  g_source_attach (source, ctx);
  g_source_unref (source);

  while (TRUE)
    g_main_context_iteration (ctx, TRUE);

  return NULL;
}

static void
test_invoke (void)
{
  GMainContext *ctx;
  GThread *thread;

  count = 0;

  /* this one gets invoked directly */
  g_main_context_invoke (NULL, func, g_thread_self ());
  g_assert (count == 1);

  /* invoking out of an idle works too */
  g_idle_add (call_func, NULL);
  g_main_context_iteration (g_main_context_default (), FALSE);
  g_assert (count == 2);

  /* test thread-default forcing the invocation to go
   * to another thread
   */
  ctx = g_main_context_new ();
  thread = g_thread_new ("worker", thread_func, ctx);

  g_usleep (1000); /* give some time to push the thread-default */

  g_main_context_invoke (ctx, func, thread);
  g_assert (count == 2);

  g_thread_join (thread);
  g_assert (count == 3);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/maincontext/basic", test_maincontext_basic);
  g_test_add_func ("/mainloop/basic", test_mainloop_basic);
  g_test_add_func ("/mainloop/timeouts", test_timeouts);
  g_test_add_func ("/mainloop/priorities", test_priorities);
  g_test_add_func ("/mainloop/invoke", test_invoke);

  return g_test_run ();
}
