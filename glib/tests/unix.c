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
#include <pwd.h>

static void
test_pipe (void)
{
  GError *error = NULL;
  int pipefd[2];
  char buf[1024];
  gssize bytes_read;
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

static void
test_nonblocking (void)
{
  GError *error = NULL;
  int pipefd[2];
  gboolean res;
  int flags;

  res = g_unix_open_pipe (pipefd, FD_CLOEXEC, &error);
  g_assert (res);
  g_assert_no_error (error);

  res = g_unix_set_fd_nonblocking (pipefd[0], TRUE, &error);
  g_assert (res);
  g_assert_no_error (error);
 
  flags = fcntl (pipefd[0], F_GETFL);
  g_assert_cmpint (flags, !=, -1);
  g_assert (flags & O_NONBLOCK);

  res = g_unix_set_fd_nonblocking (pipefd[0], FALSE, &error);
  g_assert (res);
  g_assert_no_error (error);
 
  flags = fcntl (pipefd[0], F_GETFL);
  g_assert_cmpint (flags, !=, -1);
  g_assert (!(flags & O_NONBLOCK));

  close (pipefd[0]);
  close (pipefd[1]);
}

static gboolean sig_received = FALSE;
static gboolean sig_timeout = FALSE;
static int sig_counter = 0;

static gboolean
on_sig_received (gpointer user_data)
{
  GMainLoop *loop = user_data;
  g_main_loop_quit (loop);
  sig_received = TRUE;
  sig_counter ++;
  return G_SOURCE_REMOVE;
}

static gboolean
on_sig_timeout (gpointer data)
{
  GMainLoop *loop = data;
  g_main_loop_quit (loop);
  sig_timeout = TRUE;
  return G_SOURCE_REMOVE;
}

static gboolean
exit_mainloop (gpointer data)
{
  GMainLoop *loop = data;
  g_main_loop_quit (loop);
  return G_SOURCE_REMOVE;
}

static gboolean
on_sig_received_2 (gpointer data)
{
  GMainLoop *loop = data;

  sig_counter ++;
  if (sig_counter == 2)
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
  sig_counter = 0;
  g_unix_signal_add (signum, on_sig_received, mainloop);
  kill (getpid (), signum);
  g_assert (!sig_received);
  id = g_timeout_add (5000, on_sig_timeout, mainloop);
  g_main_loop_run (mainloop);
  g_assert (sig_received);
  sig_received = FALSE;
  g_source_remove (id);

  /* Ensure we don't get double delivery */
  g_timeout_add (500, exit_mainloop, mainloop);
  g_main_loop_run (mainloop);
  g_assert (!sig_received);

  /* Ensure that two sources for the same signal get it */
  sig_counter = 0;
  g_unix_signal_add (signum, on_sig_received_2, mainloop);
  g_unix_signal_add (signum, on_sig_received_2, mainloop);
  id = g_timeout_add (5000, on_sig_timeout, mainloop);

  kill (getpid (), signum);
  g_main_loop_run (mainloop);
  g_assert_cmpint (sig_counter, ==, 2);
  g_source_remove (id);

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
  guint id;
  struct sigaction action;

  sig_received = FALSE;
  id = g_unix_signal_add (SIGHUP, on_sig_received, NULL);
  g_source_remove (id);

  sigaction (SIGHUP, NULL, &action);
  g_assert (action.sa_handler == SIG_DFL);
}

static gboolean
nested_idle (gpointer data)
{
  GMainLoop *nested;
  GMainContext *context;
  GSource *source;

  context = g_main_context_new ();
  nested = g_main_loop_new (context, FALSE);

  source = g_unix_signal_source_new (SIGHUP);
  g_source_set_callback (source, on_sig_received, nested, NULL);
  g_source_attach (source, context);
  g_source_unref (source);

  kill (getpid (), SIGHUP);
  g_main_loop_run (nested);
  g_assert_cmpint (sig_counter, ==, 1);

  g_main_loop_unref (nested);
  g_main_context_unref (context);

  return G_SOURCE_REMOVE;
}

static void
test_sighup_nested (void)
{
  GMainLoop *mainloop;

  mainloop = g_main_loop_new (NULL, FALSE);

  sig_counter = 0;
  sig_received = FALSE;
  g_unix_signal_add (SIGHUP, on_sig_received, mainloop);
  g_idle_add (nested_idle, mainloop);

  g_main_loop_run (mainloop);
  g_assert_cmpint (sig_counter, ==, 2);

  g_main_loop_unref (mainloop);
}

static gboolean
on_sigwinch_received (gpointer data)
{
  GMainLoop *loop = (GMainLoop *) data;

  sig_counter ++;

  if (sig_counter == 1)
    kill (getpid (), SIGWINCH);
  else if (sig_counter == 2)
    g_main_loop_quit (loop);
  else if (sig_counter > 2)
    g_assert_not_reached ();

  /* Increase the time window in which an issue could happen. */
  g_usleep (G_USEC_PER_SEC);

  return G_SOURCE_CONTINUE;
}

static void
test_callback_after_signal (void)
{
  /* Checks that user signal callback is invoked *after* receiving a signal.
   * In other words a new signal is never merged with the one being currently
   * dispatched or whose dispatch had already finished. */

  GMainLoop *mainloop;
  GMainContext *context;
  GSource *source;

  sig_counter = 0;

  context = g_main_context_new ();
  mainloop = g_main_loop_new (context, FALSE);

  source = g_unix_signal_source_new (SIGWINCH);
  g_source_set_callback (source, on_sigwinch_received, mainloop, NULL);
  g_source_attach (source, context);
  g_source_unref (source);

  g_assert_cmpint (sig_counter, ==, 0);
  kill (getpid (), SIGWINCH);
  g_main_loop_run (mainloop);
  g_assert_cmpint (sig_counter, ==, 2);

  g_main_loop_unref (mainloop);
  g_main_context_unref (context);
}

static void
test_get_passwd_entry_root (void)
{
  struct passwd *pwd;
  GError *local_error = NULL;

  g_test_summary ("Tests that g_unix_get_passwd_entry() works for a "
                  "known-existing username.");

  pwd = g_unix_get_passwd_entry ("root", &local_error);
  g_assert_no_error (local_error);

  g_assert_cmpstr (pwd->pw_name, ==, "root");
  g_assert_cmpuint (pwd->pw_uid, ==, 0);

  g_free (pwd);
}

static void
test_get_passwd_entry_nonexistent (void)
{
  struct passwd *pwd;
  GError *local_error = NULL;

  g_test_summary ("Tests that g_unix_get_passwd_entry() returns an error for a "
                  "nonexistent username.");

  pwd = g_unix_get_passwd_entry ("thisusernamedoesntexist", &local_error);
  g_assert_error (local_error, G_UNIX_ERROR, 0);
  g_assert_null (pwd);

  g_clear_error (&local_error);
}

static void
_child_wait_watch_cb (GPid pid,
                      gint wait_status,
                      gpointer user_data)
{
  gboolean *p_got_callback = user_data;

  g_assert_nonnull (p_got_callback);
  g_assert_false (*p_got_callback);
  *p_got_callback = TRUE;
}

static void
test_child_wait (void)
{
  gboolean r;
  GPid pid;
  guint id;
  pid_t pid2;
  int wstatus;
  gboolean got_callback = FALSE;
  gboolean iterate_maincontext = g_test_rand_bit ();
  char **argv;
  int errsv;

  /* - We spawn a trivial child process that exits after a short time.
   * - We schedule a g_child_watch_add()
   * - we may iterate the GMainContext a bit. Randomly we either get the
   *   child-watcher callback or not.
   * - if we didn't get the callback, we g_source_remove() the child watcher.
   *
   * Afterwards, if the callback didn't fire, we check that we are able to waitpid()
   * on the process ourselves. Of course, if the child watcher notified, the waitpid()
   * will fail with ECHILD.
   */

  argv = g_test_rand_bit () ? ((char *[]){ "/bin/sleep", "0.05", NULL }) : ((char *[]){ "/bin/true", NULL });

  r = g_spawn_async (NULL,
                     argv,
                     NULL,
                     G_SPAWN_DO_NOT_REAP_CHILD,
                     NULL,
                     NULL,
                     &pid,
                     NULL);
  if (!r)
    {
      /* Some odd system without /bin/sleep? Skip the test. */
      g_test_skip ("failure to spawn test process in test_child_wait()");
      return;
    }

  g_assert_cmpint (pid, >=, 1);

  if (g_test_rand_bit ())
    g_usleep (g_test_rand_int_range (0, (G_USEC_PER_SEC / 10)));

  id = g_child_watch_add (pid, _child_wait_watch_cb, &got_callback);

  if (g_test_rand_bit ())
    g_usleep (g_test_rand_int_range (0, (G_USEC_PER_SEC / 10)));

  if (iterate_maincontext)
    {
      gint64 start_usec = g_get_monotonic_time ();
      gint64 end_usec = start_usec + g_test_rand_int_range (0, (G_USEC_PER_SEC / 10));

      while (!got_callback && g_get_monotonic_time () < end_usec)
        g_main_context_iteration (NULL, FALSE);
    }

  if (!got_callback)
    g_source_remove (id);

  errno = 0;
  pid2 = waitpid (pid, &wstatus, 0);
  errsv = errno;
  if (got_callback)
    {
      g_assert_true (iterate_maincontext);
      g_assert_cmpint (errsv, ==, ECHILD);
      g_assert_cmpint (pid2, <, 0);
    }
  else
    {
      g_assert_cmpint (errsv, ==, 0);
      g_assert_cmpint (pid2, ==, pid);
      g_assert_true (WIFEXITED (wstatus));
      g_assert_cmpint (WEXITSTATUS (wstatus), ==, 0);
    }
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/glib-unix/pipe", test_pipe);
  g_test_add_func ("/glib-unix/error", test_error);
  g_test_add_func ("/glib-unix/nonblocking", test_nonblocking);
  g_test_add_func ("/glib-unix/sighup", test_sighup);
  g_test_add_func ("/glib-unix/sigterm", test_sigterm);
  g_test_add_func ("/glib-unix/sighup_again", test_sighup);
  g_test_add_func ("/glib-unix/sighup_add_remove", test_sighup_add_remove);
  g_test_add_func ("/glib-unix/sighup_nested", test_sighup_nested);
  g_test_add_func ("/glib-unix/callback_after_signal", test_callback_after_signal);
  g_test_add_func ("/glib-unix/get-passwd-entry/root", test_get_passwd_entry_root);
  g_test_add_func ("/glib-unix/get-passwd-entry/nonexistent", test_get_passwd_entry_nonexistent);

  return g_test_run();
}
