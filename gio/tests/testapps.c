#include <gio/gio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

static gint appeared;
static gint disappeared;
static gint changed;
static gboolean died;
static gboolean timed_out;
GPid pid;

static void
name_appeared (GDBusConnection *connection,
               const gchar     *name,
               const gchar     *name_owner,
               gpointer         user_data)
{
  GMainLoop *loop = user_data;

  appeared++;

  if (loop)
    g_main_loop_quit (loop);
}

static void
name_disappeared (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
  GMainLoop *loop = user_data;

  disappeared++;

  if (loop)
    g_main_loop_quit (loop);
}

static gboolean
start_application (gpointer data)
{
  gchar *argv[] = { "./testapp", NULL };

  g_assert (g_spawn_async (NULL, argv, NULL, 0, NULL, NULL, &pid, NULL));

  return FALSE;
}

static gboolean
run_application_sync (gpointer data)
{
  GMainLoop *loop = data;

  g_assert (g_spawn_command_line_sync ("./testapp", NULL, NULL, NULL, NULL));

  if (loop)
    g_main_loop_quit (loop);

  return FALSE;
}

static gboolean
timeout (gpointer data)
{
  GMainLoop *loop = data;

  timed_out = TRUE;

  g_main_loop_quit (loop);

  return TRUE;
}

/* This test starts an application, checks that its name appears
 * on the bus, then starts it again and checks that the second
 * instance exits right away.
 */
static void
test_unique (void)
{
  GMainLoop *loop;
  gint watch;
  guint id1, id2, id3;

  appeared = 0;
  timed_out = FALSE;

  loop = g_main_loop_new (NULL, FALSE);
  id1 = g_timeout_add (5000, timeout, loop);

  watch = g_bus_watch_name (G_BUS_TYPE_SESSION,
                            "org.gtk.test.app",
                            0,
                            name_appeared,
                            NULL,
                            loop,
                            NULL);

  id2 = g_timeout_add (0, start_application, loop);

  g_main_loop_run (loop);

  g_assert_cmpint (appeared, ==, 1);

  id3 = g_timeout_add (0, run_application_sync, loop);

  g_main_loop_run (loop);

  g_assert_cmpint (appeared, ==, 1);
  g_assert_cmpint (timed_out, ==, FALSE);

  g_bus_unwatch_name (watch);

  kill (pid, SIGTERM);

  g_main_loop_unref (loop);
  g_source_remove (id1);
  g_source_remove (id2);
  g_source_remove (id3);
}

static gboolean
quit_app (gpointer data)
{
  GDBusConnection *connection;
  GVariant *res;

  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  res = g_dbus_connection_call_sync (connection,
                                     "org.gtk.test.app",
                                     "/org/gtk/test/app",
                                     "org.gtk.Application",
                                     "Quit",
                                     g_variant_new ("(u)", 0),
				     NULL,
                                     G_DBUS_CALL_FLAGS_NONE,
                                     -1,
                                     NULL,
                                     NULL);
  if (res)
    g_variant_unref (res);

  return FALSE;
}

static void
child_is_dead (GPid     pid,
               gint     status,
               gpointer data)
{
  GMainLoop *loop = data;

  died++;

  g_assert (WIFEXITED (status) && WEXITSTATUS(status) == 0);

  if (loop)
    g_main_loop_quit (loop);
}

/* This test start an application, checks that its name appears on
 * the bus, then calls Quit, and verifies that the name disappears
 * and the application exits.
 */
static void
test_quit (void)
{
  GMainLoop *loop;
  gint watch;
  guint id1, id2, id3;
  gchar *argv[] = { "./testapp", NULL };

  appeared = 0;
  disappeared = 0;
  died = FALSE;
  timed_out = FALSE;

  loop = g_main_loop_new (NULL, FALSE);
  watch = g_bus_watch_name (G_BUS_TYPE_SESSION,
                            "org.gtk.test.app",
                            0,
                            name_appeared,
                            name_disappeared,
                            NULL,
                            NULL);

  g_assert (g_spawn_async (NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid, NULL));

  id1 = g_child_watch_add (pid, child_is_dead, loop);

  id2 = g_timeout_add (500, quit_app, NULL);

  id3 = g_timeout_add (5000, timeout, loop);

  g_main_loop_run (loop);
  g_assert_cmpint (timed_out, ==, FALSE);
  g_assert_cmpint (appeared, ==, 1);
  g_assert_cmpint (disappeared, >=, 1);
  g_assert_cmpint (died, ==, TRUE);

  g_bus_unwatch_name (watch);

  g_main_loop_unref (loop);
  g_source_remove (id1);
  g_source_remove (id2);
  g_source_remove (id3);
}

static gboolean
_g_strv_has_string (const gchar* const * haystack,
                    const gchar *needle)
{
  guint n;

  for (n = 0; haystack != NULL && haystack[n] != NULL; n++)
    {
      if (g_strcmp0 (haystack[n], needle) == 0)
        return TRUE;
    }
  return FALSE;
}

static gchar **
list_actions (void)
{
  GDBusConnection *connection;
  GVariant *res;
  gchar **strv;
  gchar *str;
  GVariantIter *iter;
  gint i;

  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  res = g_dbus_connection_call_sync (connection,
                                     "org.gtk.test.app",
                                     "/org/gtk/test/app",
                                     "org.gtk.Application",
                                     "ListActions",
                                     NULL,
				     NULL,
                                     G_DBUS_CALL_FLAGS_NONE,
                                     -1,
                                     NULL,
                                     NULL);

  strv = g_new0 (gchar *, 32);
  g_variant_get (res, "(a{s(sb)})", &iter);
  i = 0;
  while (g_variant_iter_loop (iter, "{s(sb)}", &str, NULL, NULL))
    {
      strv[i] = g_strdup (str);
      i++;
      g_assert (i < 32);
    }
  g_variant_iter_free (iter);

  strv[i] = NULL;

  g_variant_unref (res);
  g_object_unref (connection);

  return strv;
}

/* This test start an application, waits for its name to appear on
 * the bus, then calls ListActions, and verifies that it gets the expected
 * actions back.
 */
static void
test_list_actions (void)
{
  GMainLoop *loop;
  gchar *argv[] = { "./testapp", NULL };
  gchar **actions;
  gint watch;

  appeared = 0;

  loop = g_main_loop_new (NULL, FALSE);
  watch = g_bus_watch_name (G_BUS_TYPE_SESSION,
                            "org.gtk.test.app",
                            0,
                            name_appeared,
                            NULL,
                            loop,
                            NULL);

  g_assert (g_spawn_async (NULL, argv, NULL, 0, NULL, NULL, &pid, NULL));
  if (!appeared)
    g_main_loop_run (loop);
  g_main_loop_unref (loop);

  actions = list_actions ();

  g_assert (g_strv_length  (actions) == 2);
  g_assert (_g_strv_has_string ((const char *const *)actions, "action1"));
  g_assert (_g_strv_has_string ((const char *const *)actions, "action2"));

  g_strfreev (actions);

  kill (pid, SIGTERM);

  g_bus_unwatch_name (watch);
}

static gboolean
invoke_action (gpointer data)
{
  const gchar *action = data;
  GDBusConnection *connection;
  GVariant *res;

  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  res = g_dbus_connection_call_sync (connection,
                                     "org.gtk.test.app",
                                     "/org/gtk/test/app",
                                     "org.gtk.Application",
                                     "InvokeAction",
                                     g_variant_new ("(su)",
                                                    action,
                                                    0),
				     NULL,
                                     G_DBUS_CALL_FLAGS_NONE,
                                     -1,
                                     NULL,
                                     NULL);
  if (res)
    g_variant_unref (res);
  g_object_unref (connection);

  return FALSE;
}

static void
exit_with_code_1 (GPid     pid,
                  gint     status,
                  gpointer data)
{
  GMainLoop *loop = data;

  died++;

  g_assert (WIFEXITED (status) && WEXITSTATUS(status) == 1);

  if (loop)
    g_main_loop_quit (loop);
}

/* This test starts an application, waits for it to appear,
 * then invokes 'action1' and checks that it causes the application
 * to exit with an exit code of 1.
 */
static void
test_invoke (void)
{
  GMainLoop *loop;
  gint watch;
  gchar *argv[] = { "./testapp", NULL };
  guint id1, id2, id3;

  appeared = 0;
  disappeared = 0;
  died = FALSE;
  timed_out = FALSE;

  loop = g_main_loop_new (NULL, FALSE);
  watch = g_bus_watch_name (G_BUS_TYPE_SESSION,
                            "org.gtk.test.app",
                            0,
                            name_appeared,
                            name_disappeared,
                            NULL,
                            NULL);

  g_assert (g_spawn_async (NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid, NULL));

  id1 = g_child_watch_add (pid, exit_with_code_1, loop);

  id2 = g_timeout_add (500, invoke_action, "action1");

  id3 = g_timeout_add (5000, timeout, loop);

  g_main_loop_run (loop);
  g_assert_cmpint (timed_out, ==, FALSE);
  g_assert_cmpint (appeared, >=, 1);
  g_assert_cmpint (disappeared, >=, 1);
  g_assert_cmpint (died, ==, TRUE);

  g_bus_unwatch_name (watch);
  g_main_loop_unref (loop);
  g_source_remove (id1);
  g_source_remove (id2);
  g_source_remove (id3);

  kill (pid, SIGTERM);
}

static void
test_remote (void)
{
  GMainLoop *loop;
  gint watch;
  GPid pid1, pid2;
  gchar *argv[] = { "./testapp", NULL, NULL };

  appeared = 0;
  timed_out = FALSE;

  loop = g_main_loop_new (NULL, FALSE);
  g_timeout_add (5000, timeout, loop);

  watch = g_bus_watch_name (G_BUS_TYPE_SESSION,
                            "org.gtk.test.app",
                            0,
                            name_appeared,
                            NULL,
                            loop,
                            NULL);

  g_assert (g_spawn_async (NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid1, NULL));
  g_child_watch_add (pid1, exit_with_code_1, loop);

  g_main_loop_run (loop);

  g_assert_cmpint (appeared, ==, 1);

  argv[1] = "--non-unique";
  g_assert (g_spawn_async (NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid2, NULL));

  g_main_loop_run (loop);

  g_assert_cmpint (appeared, ==, 1);
  g_assert_cmpint (timed_out, ==, FALSE);

  g_main_loop_unref (loop);
  g_bus_unwatch_name (watch);

  kill (pid1, SIGTERM);
  kill (pid2, SIGTERM);
}

static void
actions_changed (GDBusConnection *connection,
                 const gchar     *sender_name,
                 const gchar     *object_path,
                 const gchar     *interface_name,
                 const gchar     *signal_name,
                 GVariant        *parameters,
                 gpointer         user_data)
{
  GMainLoop *loop = user_data;

  g_assert_cmpstr (interface_name, ==, "org.gtk.Application");
  g_assert_cmpstr (signal_name, ==, "ActionsChanged");

  changed++;

  g_main_loop_quit (loop);
}

static void
test_change_action (void)
{
  GMainLoop *loop;
  gint watch;
  guint id;
  GPid pid1;
  gchar *argv[] = { "./testapp", NULL, NULL };
  GDBusConnection *connection;

  appeared = 0;
  changed = 0;
  timed_out = FALSE;

  loop = g_main_loop_new (NULL, FALSE);
  g_timeout_add (5000, timeout, loop);

  watch = g_bus_watch_name (G_BUS_TYPE_SESSION,
                            "org.gtk.test.app",
                            0,
                            name_appeared,
                            NULL,
                            loop,
                            NULL);

  g_assert (g_spawn_async (NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid1, NULL));
  g_main_loop_run (loop);

  g_assert_cmpint (appeared, ==, 1);

  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  id = g_dbus_connection_signal_subscribe (connection,
                                           NULL,
                                           "org.gtk.Application",
                                           "ActionsChanged",
                                           "/org/gtk/test/app",
                                           NULL,
                                           actions_changed,
                                           loop,
                                           NULL);

  g_timeout_add (1000, invoke_action, "action2");

  g_main_loop_run (loop);

  g_assert_cmpint (changed, >, 0);
  g_assert_cmpint (timed_out, ==, FALSE);

  g_dbus_connection_signal_unsubscribe (connection, id);
  g_object_unref (connection);
  g_main_loop_unref (loop);
  g_bus_unwatch_name (watch);

  kill (pid1, SIGTERM);
}

int
main (int argc, char *argv[])
{
  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/application/unique", test_unique);
  g_test_add_func ("/application/quit", test_quit);
  g_test_add_func ("/application/list-actions", test_list_actions);
  g_test_add_func ("/application/invoke", test_invoke);
  g_test_add_func ("/application/remote", test_remote);
  g_test_add_func ("/application/change-action", test_change_action);

  return g_test_run ();
}

