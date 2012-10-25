/* 
 * Copyright (C) 2011 Red Hat, Inc.
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
 * Author: Colin Walters <walters@verbum.org> 
 */

#include "config.h"

#include "glib-unix.h"
#include <string.h>

static void
test_pipe (void)
{
  GError *error = NULL;
  int pipefd[2];
  char buf[1024];
  ssize_t bytes_read;
  gboolean res;

  res = g_unix_open_pipe (pipefd, FD_CLOEXEC, &error);
  g_assert (res);
  g_assert_no_error (error);

  write (pipefd[1], "hello", sizeof ("hello"));
  memset (buf, 0, sizeof (buf));
  bytes_read = read (pipefd[0], buf, sizeof(buf) - 1);
  g_assert_cmpint (bytes_read, >, 0);
  buf[bytes_read] = '\0';

  close (pipefd[0]);
  close (pipefd[1]);

  g_assert (g_str_has_prefix (buf, "hello"));
}

static void
test_error (void)
{
  GError *error = NULL;
  gboolean res;

  res = g_unix_set_fd_nonblocking (123456, TRUE, &error);
  g_assert_cmpint (errno, ==, EBADF);
  g_assert (!res);
  g_assert_error (error, G_UNIX_ERROR, 0);
  g_clear_error (&error);
}

static gboolean sig_received = FALSE;

static gboolean
on_sig_received (gpointer user_data)
{
  GMainLoop *loop = user_data;
  g_main_loop_quit (loop);
  sig_received = TRUE;
  return G_SOURCE_REMOVE;
}

static gboolean
sig_not_received (gpointer data)
{
  GMainLoop *loop = data;
  (void) loop;
  g_error ("Timed out waiting for signal");
  return G_SOURCE_REMOVE;
}

static gboolean
exit_mainloop (gpointer data)
{
  GMainLoop *loop = data;
  g_main_loop_quit (loop);
  return G_SOURCE_REMOVE;
}

static void
test_signal (int signum)
{
  GMainLoop *mainloop;
  int id;

  mainloop = g_main_loop_new (NULL, FALSE);

  sig_received = FALSE;
  g_unix_signal_add (signum, on_sig_received, mainloop);
  kill (getpid (), signum);
  g_assert (!sig_received);
  id = g_timeout_add (5000, sig_not_received, mainloop);
  g_main_loop_run (mainloop);
  g_assert (sig_received);
  sig_received = FALSE;
  g_source_remove (id);

  /* Ensure we don't get double delivery */
  g_timeout_add (500, exit_mainloop, mainloop);
  g_main_loop_run (mainloop);
  g_assert (!sig_received);
  g_main_loop_unref (mainloop);

}

static void
test_sighup (void)
{
  test_signal (SIGHUP);
}

static void
test_sigterm (void)
{
  test_signal (SIGTERM);
}

static void
test_sighup_add_remove (void)
{
  GMainLoop *mainloop;
  guint id;

  mainloop = g_main_loop_new (NULL, FALSE);

  sig_received = FALSE;
  id = g_unix_signal_add (SIGHUP, on_sig_received, mainloop);
  g_source_remove (id);
  kill (getpid (), SIGHUP);
  g_assert (!sig_received);
  g_main_loop_unref (mainloop);

}

typedef struct {
  GPid regular_pid;
  GPid catchall_pid;
  GMainLoop *loop;
  gboolean regular_exited;
  gboolean catchall_exited;
} CatchAllData;

static void
on_catchall_child (GPid   pid,
                   gint   estatus,
                   gpointer user_data)
{
  CatchAllData *data = user_data;
  GError *local_error = NULL;
  GError **error = &local_error;

  g_assert (pid == data->catchall_pid);

  g_spawn_check_exit_status (estatus, error);
  g_assert_no_error (local_error);

  data->catchall_exited = TRUE;
  if (data->regular_exited)
    g_main_loop_quit (data->loop);
}

static void
on_regular_child (GPid  pid,
                  gint  estatus,
                  gpointer user_data)
{
  CatchAllData *data = user_data;
  GError *local_error = NULL;
  GError **error = &local_error;

  g_assert (pid == data->regular_pid);

  g_spawn_check_exit_status (estatus, error);
  g_assert_no_error (local_error);

  data->regular_exited = TRUE;
  if (data->catchall_exited)
    g_main_loop_quit (data->loop);
}

static void
spawn_with_raw_fork (gchar  **child_argv,
                     pid_t   *out_pid)
{
  pid_t pid;

  pid = fork ();
  g_assert (pid >= 0);
  if (pid == 0)
    {
      execv (child_argv[0], child_argv+1);
      g_assert_not_reached ();
    }
  else
    *out_pid = pid;
}

static void
test_catchall (void)
{
  GMainLoop *mainloop;
  CatchAllData data;
  GSource *source;
  GPid pid;
  GError *local_error = NULL;
  GError **error = &local_error;
  char *child_args[] = { "/bin/true", "/bin/true", NULL };

  memset (&data, 0, sizeof (data));
  mainloop = g_main_loop_new (NULL, FALSE);
  data.loop = mainloop;

  source = g_child_watch_source_new (-1);
  g_source_set_callback (source, (GSourceFunc)on_catchall_child, &data, NULL);
  g_source_attach (source, NULL);
  g_source_unref (source);

  g_spawn_async (NULL, (char**)child_args, NULL, G_SPAWN_DO_NOT_REAP_CHILD,
                 NULL, NULL, &pid, error);
  g_child_watch_add (pid, on_regular_child, &data);
  data.regular_pid = pid;

  spawn_with_raw_fork ((char**)child_args, &pid);
  data.catchall_pid = pid;

  g_main_loop_run (mainloop);

  g_source_destroy (source);
  g_main_loop_unref (mainloop);

  g_assert (data.regular_exited && data.catchall_exited);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/glib-unix/pipe", test_pipe);
  g_test_add_func ("/glib-unix/error", test_error);
  g_test_add_func ("/glib-unix/sighup", test_sighup);
  g_test_add_func ("/glib-unix/sigterm", test_sigterm);
  g_test_add_func ("/glib-unix/sighup_again", test_sighup);
  g_test_add_func ("/glib-unix/sighup_add_remove", test_sighup_add_remove);
  g_test_add_func ("/glib-unix/catchall", test_catchall);

  return g_test_run();
}
