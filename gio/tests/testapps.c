#include <gio/gio.h>
#ifdef G_OS_UNIX
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#endif

#include "gdbus-sessionbus.h"

static gint appeared;
static gint disappeared;
static gint changed;

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

#ifdef G_OS_UNIX
void
child_setup_pipe (gpointer user_data)
{
  int *fds = user_data;

  close (fds[0]);
  dup2 (fds[1], 3);
  g_setenv ("_G_TEST_SLAVE_FD", "3", TRUE);
  close (fds[1]);
}
#endif

static gboolean
spawn_async_with_monitor_pipe (const gchar *argv[], GPid *pid, int *fd)
{
#ifdef G_OS_UNIX
  int fds[2];
  gboolean result;

  pipe (fds);

  result = g_spawn_async (NULL, (char**)argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD, child_setup_pipe, &fds, pid, NULL);
  close (fds[1]);
  *fd = fds[0];
  return result;
#else
  *fd = -1;
  return g_spawn_async (NULL, argv, 0, NULL, NULL, pid, NULL);
#endif
}

static gboolean
start_application (GPid *pid, int *fd)
{
  const gchar *argv[] = { "./testapp", NULL };

  g_assert (spawn_async_with_monitor_pipe (argv, pid, fd));

  return FALSE;
}

typedef struct {
  GMainContext *context;
  GSource *child_watch;
  GSource *timeout;
  GPid pid;
  int fd;

  gboolean child_exited;
  GMainLoop *loop;
} AwaitChildTerminationData;

static void
on_child_termination_exited (GPid pid,
                             gint status,
                             gpointer user_data)
{
  AwaitChildTerminationData *data = user_data;
  data->child_exited = TRUE;
  g_spawn_close_pid (data->pid);
  g_main_loop_quit (data->loop);
}

static gboolean
on_child_termination_timeout (gpointer user_data)
{
  AwaitChildTerminationData *data = user_data;
  g_main_loop_quit (data->loop);
  return FALSE;
}

static void
await_child_termination_init (AwaitChildTerminationData *data,
                              GPid                       pid,
                              int                        fd)
{
  data->context = g_main_context_get_thread_default ();
  data->child_exited = FALSE;
  data->pid = pid;
  data->fd = fd;
}

static void
await_child_termination_terminate (AwaitChildTerminationData *data)
{
#ifdef G_OS_UNIX
  kill (data->pid, SIGTERM);
  close (data->fd);
#endif
}

static gboolean
await_child_termination_run (AwaitChildTerminationData *data)
{
  GSource *timeout_source;
  GSource *child_watch_source;

  data->loop = g_main_loop_new (data->context, FALSE);

  child_watch_source = g_child_watch_source_new (data->pid);
  g_source_set_callback (child_watch_source, (GSourceFunc) on_child_termination_exited, data, NULL);
  g_source_attach (child_watch_source, data->context);
  g_source_unref (child_watch_source);

  timeout_source = g_timeout_source_new_seconds (5);
  g_source_set_callback (timeout_source, on_child_termination_timeout, data, NULL);
  g_source_attach (timeout_source, data->context);
  g_source_unref (timeout_source);

  g_main_loop_run (data->loop);

  g_source_destroy (child_watch_source);
  g_source_destroy (timeout_source);

  g_main_loop_unref (data->loop);

  return data->child_exited;
}

static void
terminate_child_sync (GPid pid, int fd)
{
  AwaitChildTerminationData data;

  await_child_termination_init (&data, pid, fd);
  await_child_termination_terminate (&data);
  await_child_termination_run (&data);
}

typedef void (*RunWithApplicationFunc) (void);

typedef struct {
  GMainLoop *loop;
  RunWithApplicationFunc func;
  guint timeout_id;
} RunWithAppNameAppearedData;

static void
on_run_with_application_name_appeared (GDBusConnection *connection,
                                       const gchar     *name,
                                       const gchar     *name_owner,
                                       gpointer         user_data)
{
  RunWithAppNameAppearedData *data = user_data;

  data->func ();

  g_main_loop_quit (data->loop);
}

static gboolean
on_run_with_application_timeout (gpointer user_data)
{
  RunWithAppNameAppearedData *data = user_data;
  data->timeout_id = 0;
  g_error ("Timed out starting testapp");
  return FALSE;
}

static void
run_with_application (RunWithApplicationFunc test_func)
{
  GMainLoop *loop;
  RunWithAppNameAppearedData data;
  gint watch;
  GPid main_pid;
  gint main_fd;

  loop = g_main_loop_new (NULL, FALSE);

  data.timeout_id = 0;
  data.func = test_func;
  data.loop = loop;

  watch = g_bus_watch_name (G_BUS_TYPE_SESSION,
                            "org.gtk.test.app",
                            0,
                            on_run_with_application_name_appeared,
                            NULL,
                            &data,
                            NULL);

  data.timeout_id = g_timeout_add_seconds (5, on_run_with_application_timeout, &data);

  start_application (&main_pid, &main_fd);

  g_main_loop_run (loop);

  if (data.timeout_id)
    {
      g_source_remove (data.timeout_id);
      data.timeout_id = 0;
    }

  g_main_loop_unref (loop);

  g_bus_unwatch_name (watch);

  terminate_child_sync (main_pid, main_fd);
}

/* This test starts an application, checks that its name appears
 * on the bus, then starts it again and checks that the second
 * instance exits right away.
 */
static void
test_unique_on_app_appeared (void)
{
  GPid sub_pid;
  int sub_fd;
  int watch;
  AwaitChildTerminationData data;

  appeared = 0;
  disappeared = 0;

  watch = g_bus_watch_name (G_BUS_TYPE_SESSION,
                            "org.gtk.test.app",
                            0,
                            name_appeared,
                            name_disappeared,
                            NULL,
                            NULL);

  start_application (&sub_pid, &sub_fd);
  await_child_termination_init (&data, sub_pid, sub_fd);
  await_child_termination_run (&data);

  g_bus_unwatch_name (watch);

  g_assert_cmpint (appeared, ==, 1);
  g_assert_cmpint (disappeared, ==, 0);
}

static void
test_unique (void)
{
  run_with_application (test_unique_on_app_appeared);
}

static void
on_name_disappeared_quit (GDBusConnection *connection,
                          const gchar     *name,
                          gpointer         user_data)
{
  GMainLoop *loop = user_data;

  g_main_loop_quit (loop);
}

static gboolean
call_quit (gpointer data)
{
  GDBusConnection *connection;
  GError *error = NULL;
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
                                     &error);
  if (error)
    {
      g_assert_error (error, G_DBUS_ERROR, G_DBUS_ERROR_NO_REPLY);
      g_error_free (error);
    }

  if (res)
    g_variant_unref (res);

  return FALSE;
}

/* This test starts an application, checks that its name appears on
 * the bus, then calls Quit, and verifies that the name disappears and
 * the application exits.
 */
static void
test_quit_on_app_appeared (void)
{
  GMainLoop *loop;
  int quit_disappeared_watch;

  loop = g_main_loop_new (NULL, FALSE);
  quit_disappeared_watch = g_bus_watch_name (G_BUS_TYPE_SESSION,
                            "org.gtk.test.app",
                            0,
                            NULL,
                            on_name_disappeared_quit,
                            loop,
                            NULL);

  /* We need a timeout here, since we may otherwise end up calling
   * Quit after the application took the name, but before it registered
   * the object.
   */
  g_timeout_add (500, call_quit, NULL);

  g_main_loop_run (loop);

  g_bus_unwatch_name (quit_disappeared_watch);

  g_main_loop_unref (loop);
}

static void
test_quit (void)
{
  run_with_application (test_quit_on_app_appeared);
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

static void
test_list_actions_on_app_appeared (void)
{
  gchar **actions;

  actions = list_actions ();

  g_assert (g_strv_length  (actions) == 2);
  g_assert (_g_strv_has_string ((const char *const *)actions, "action1"));
  g_assert (_g_strv_has_string ((const char *const *)actions, "action2"));

  g_strfreev (actions);
}

/* This test start an application, waits for its name to appear on
 * the bus, then calls ListActions, and verifies that it gets the expected
 * actions back.
 */
static void
test_list_actions (void)
{
  run_with_application (test_list_actions_on_app_appeared);
}

static gboolean
invoke_action (gpointer user_data)
{
  const gchar *action = user_data;
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

/* This test starts an application, waits for it to appear,
 * then invokes 'action1' and checks that it causes the application
 * to exit with an exit code of 1.
 */
static void
test_invoke (void)
{
  GMainLoop *loop;
  int quit_disappeared_watch;

  loop = g_main_loop_new (NULL, FALSE);

  quit_disappeared_watch = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                             "org.gtk.test.app",
                                             0,
                                             NULL,
                                             on_name_disappeared_quit,
                                             loop,
                                             NULL);

  g_timeout_add (0, invoke_action, "action1");

  g_main_loop_run (loop);

  g_bus_unwatch_name (quit_disappeared_watch);
}

static void
test_remote_on_application_appeared (void)
{
  GPid sub_pid;
  int sub_fd;
  AwaitChildTerminationData data;
  gchar *argv[] = { "./testapp", "--non-unique", NULL };

  spawn_async_with_monitor_pipe ((const char **) argv, &sub_pid, &sub_fd);

  await_child_termination_init (&data, sub_pid, sub_fd);
  await_child_termination_run (&data);
}

static void
test_remote (void)
{
  run_with_application (test_remote_on_application_appeared);
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
test_change_action_on_application_appeared (void)
{
  GMainLoop *loop;
  guint id;
  GDBusConnection *connection;

  loop = g_main_loop_new (NULL, FALSE);

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

  g_timeout_add (0, invoke_action, "action2");

  g_main_loop_run (loop);

  g_assert_cmpint (changed, >, 0);

  g_dbus_connection_signal_unsubscribe (connection, id);
  g_object_unref (connection);
  g_main_loop_unref (loop);
}

static void
test_change_action (void)
{
  run_with_application (test_change_action_on_application_appeared);
}

int
main (int argc, char *argv[])
{
  gint ret;

  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  g_unsetenv ("DISPLAY");
  g_setenv ("DBUS_SESSION_BUS_ADDRESS", session_bus_get_temporary_address (), TRUE);

  session_bus_up ();

  g_test_add_func ("/application/unique", test_unique);
  g_test_add_func ("/application/quit", test_quit);
  g_test_add_func ("/application/list-actions", test_list_actions);
  g_test_add_func ("/application/invoke", test_invoke);
  g_test_add_func ("/application/remote", test_remote);
  g_test_add_func ("/application/change-action", test_change_action);

  ret = g_test_run ();

  session_bus_down ();

  return ret;
}

