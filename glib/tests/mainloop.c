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
  g_assert_cmpint (g_source_get_priority (source), ==, G_PRIORITY_DEFAULT);
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
  g_assert_cmpint (g_source_get_priority (source), ==, G_PRIORITY_HIGH);

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

  source = g_source_new (&funcs, sizeof (GSource));
  g_source_set_funcs (source, &funcs);
  g_source_set_callback (source, cb, data, NULL);
  id = g_source_attach (source, ctx);
  g_source_unref (source);
  g_assert (g_source_remove_by_user_data (data));

  g_idle_add (cb, data);
  g_assert (g_idle_remove_by_data (data));
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

  g_assert_cmpint (g_main_depth (), ==, 0);

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

  /* this is a race condition; under some circumstances we might not get 10
   * 100ms runs in 1050 ms, so consider 9 as "close enough" */
  g_assert_cmpint (a, >=, 9);
  g_assert_cmpint (a, <=, 10);
  g_assert_cmpint (b, ==, 4);
  g_assert_cmpint (c, ==, 3);

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
  g_assert_cmpint (a, ==, 0);
  g_assert_cmpint (b, ==, 1);

  g_assert (g_main_context_iteration (ctx, FALSE));
  g_assert_cmpint (a, ==, 0);
  g_assert_cmpint (b, ==, 2);

  g_source_destroy (sourceb);

  g_assert (g_main_context_iteration (ctx, FALSE));
  g_assert_cmpint (a, ==, 1);
  g_assert_cmpint (b, ==, 2);

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

static GMutex mutex;
static GCond cond;

static gpointer
thread_func (gpointer data)
{
  GMainContext *ctx = data;
  GSource *source;

  g_main_context_push_thread_default (ctx);

  g_mutex_lock (&mutex);
  g_cond_signal (&cond);
  g_mutex_unlock (&mutex);

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
  g_assert_cmpint (count, ==, 1);

  /* invoking out of an idle works too */
  g_idle_add (call_func, NULL);
  g_main_context_iteration (g_main_context_default (), FALSE);
  g_assert_cmpint (count, ==, 2);

  /* test thread-default forcing the invocation to go
   * to another thread
   */
  ctx = g_main_context_new ();
  g_mutex_lock (&mutex);
  thread = g_thread_new ("worker", thread_func, ctx);

  g_cond_wait (&cond, &mutex);

  g_main_context_invoke (ctx, func, thread);
  g_assert_cmpint (count, ==, 2);

  g_thread_join (thread);
  g_assert_cmpint (count, ==, 3);
}

static gboolean
run_inner_loop (gpointer user_data)
{
  GMainContext *ctx = user_data;
  GMainLoop *inner;
  GSource *timeout;

  a++;

  inner = g_main_loop_new (ctx, FALSE);
  timeout = g_timeout_source_new (100);
  g_source_set_callback (timeout, (GSourceFunc)g_main_loop_quit, inner, NULL);
  g_source_attach (timeout, ctx);

  g_main_loop_run (inner);
  g_main_loop_unref (inner);

  return TRUE;
}

static void
test_child_sources (void)
{
  GMainContext *ctx;
  GMainLoop *loop;
  GSource *parent, *child_b, *child_c, *end;

  ctx = g_main_context_new ();
  loop = g_main_loop_new (ctx, FALSE);

  a = b = c = 0;

  parent = g_timeout_source_new (2000);
  g_source_set_callback (parent, run_inner_loop, ctx, NULL);
  g_source_set_priority (parent, G_PRIORITY_LOW);
  g_source_attach (parent, ctx);

  child_b = g_timeout_source_new (250);
  g_source_set_callback (child_b, count_calls, &b, NULL);
  g_source_add_child_source (parent, child_b);

  child_c = g_timeout_source_new (330);
  g_source_set_callback (child_c, count_calls, &c, NULL);
  g_source_set_priority (child_c, G_PRIORITY_HIGH);
  g_source_add_child_source (parent, child_c);

  /* Child sources always have the priority of the parent */
  g_assert_cmpint (g_source_get_priority (parent), ==, G_PRIORITY_LOW);
  g_assert_cmpint (g_source_get_priority (child_b), ==, G_PRIORITY_LOW);
  g_assert_cmpint (g_source_get_priority (child_c), ==, G_PRIORITY_LOW);
  g_source_set_priority (parent, G_PRIORITY_DEFAULT);
  g_assert_cmpint (g_source_get_priority (parent), ==, G_PRIORITY_DEFAULT);
  g_assert_cmpint (g_source_get_priority (child_b), ==, G_PRIORITY_DEFAULT);
  g_assert_cmpint (g_source_get_priority (child_c), ==, G_PRIORITY_DEFAULT);

  end = g_timeout_source_new (1050);
  g_source_set_callback (end, (GSourceFunc)g_main_loop_quit, loop, NULL);
  g_source_attach (end, ctx);
  g_source_unref (end);

  g_main_loop_run (loop);

  /* The parent source's own timeout will never trigger, so "a" will
   * only get incremented when "b" or "c" does. And when timeouts get
   * blocked, they still wait the full interval next time rather than
   * "catching up". So the timing is:
   *
   *  250 - b++ -> a++, run_inner_loop
   *  330 - (c is blocked)
   *  350 - inner_loop ends
   *  350 - c++ belatedly -> a++, run_inner_loop
   *  450 - inner loop ends
   *  500 - b++ -> a++, run_inner_loop
   *  600 - inner_loop ends
   *  680 - c++ -> a++, run_inner_loop
   *  750 - (b is blocked)
   *  780 - inner loop ends
   *  780 - b++ belatedly -> a++, run_inner_loop
   *  880 - inner loop ends
   * 1010 - c++ -> a++, run_inner_loop
   * 1030 - (b is blocked)
   * 1050 - end runs, quits outer loop, which has no effect yet
   * 1110 - inner loop ends, a returns, outer loop exits
   */

  g_assert_cmpint (a, ==, 6);
  g_assert_cmpint (b, ==, 3);
  g_assert_cmpint (c, ==, 3);

  g_source_unref (parent);
  g_source_unref (child_b);
  g_source_unref (child_c);

  g_main_loop_unref (loop);
  g_main_context_unref (ctx);
}

static void
test_recursive_child_sources (void)
{
  GMainContext *ctx;
  GMainLoop *loop;
  GSource *parent, *child_b, *child_c, *end;

  ctx = g_main_context_new ();
  loop = g_main_loop_new (ctx, FALSE);

  a = b = c = 0;

  parent = g_timeout_source_new (500);
  g_source_set_callback (parent, count_calls, &a, NULL);

  child_b = g_timeout_source_new (220);
  g_source_set_callback (child_b, count_calls, &b, NULL);
  g_source_add_child_source (parent, child_b);

  child_c = g_timeout_source_new (430);
  g_source_set_callback (child_c, count_calls, &c, NULL);
  g_source_add_child_source (child_b, child_c);

  g_source_attach (parent, ctx);

  end = g_timeout_source_new (2010);
  g_source_set_callback (end, (GSourceFunc)g_main_loop_quit, loop, NULL);
  g_source_attach (end, ctx);
  g_source_unref (end);

  g_main_loop_run (loop);

  /* Sequence of events:
   * 220 b (b = 440, a = 720)
   * 430 c (c = 860, b = 650, a = 930)
   * 650 b (b = 870, a = 1150)
   * 860 c (c = 1290, b = 1080, a = 1360)
   * 1080 b (b = 1300, a = 1580)
   * 1290 c (c = 1720, b = 1510, a = 1790)
   * 1510 b (b = 1730, a = 2010)
   * 1720 c (c = 2150, b = 1940, a = 2220)
   * 1940 b (b = 2160, a = 2440)
   */

  g_assert_cmpint (a, ==, 9);
  g_assert_cmpint (b, ==, 9);
  g_assert_cmpint (c, ==, 4);

  g_source_unref (parent);
  g_source_unref (child_b);
  g_source_unref (child_c);

  g_main_loop_unref (loop);
  g_main_context_unref (ctx);
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
  g_test_add_func ("/mainloop/child_sources", test_child_sources);
  g_test_add_func ("/mainloop/recursive_child_sources", test_recursive_child_sources);

  return g_test_run ();
}
