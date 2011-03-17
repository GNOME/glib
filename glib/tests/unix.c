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

  g_unix_pipe_flags (pipefd, FD_CLOEXEC, NULL);
  g_assert_no_error (error);
    
  write (pipefd[1], "hello", sizeof ("hello"));
  memset (buf, 0, sizeof (buf));
  bytes_read = read (pipefd[0], buf, sizeof(buf) - 1);
  g_assert_cmpint (bytes_read, >, 0);
  
  close (pipefd[0]);
  close (pipefd[1]);

  g_assert (g_str_has_prefix (buf, "hello"));
}

static gboolean sighup_received = FALSE;

static gboolean
on_sighup_received (gpointer user_data)
{
  GMainLoop *loop = user_data;
  g_main_loop_quit (loop);
  sighup_received = TRUE;
  return FALSE;
}

static gboolean
sighup_not_received (gpointer data)
{
  GMainLoop *loop = data;
  (void) loop;
  g_error ("Timed out waiting for SIGHUP");
  return FALSE;
}

static gboolean
exit_mainloop (gpointer data)
{
  GMainLoop *loop = data;
  g_main_loop_quit (loop);
  return FALSE;
}

static void
test_sighup (void)
{
  GMainLoop *mainloop;

  mainloop = g_main_loop_new (NULL, FALSE);

  sighup_received = FALSE;
  g_unix_signal_add_watch_full (SIGHUP,
				G_PRIORITY_DEFAULT,
				on_sighup_received,
				mainloop,
				NULL);
  kill (getpid (), SIGHUP);
  g_assert (!sighup_received);
  g_timeout_add (5000, sighup_not_received, mainloop);
  g_main_loop_run (mainloop);
  g_assert (sighup_received);
  sighup_received = FALSE;

  /* Ensure we don't get double delivery */
  g_timeout_add (500, exit_mainloop, mainloop);
  g_main_loop_run (mainloop);
  g_assert (!sighup_received);
}

static void
test_sighup_add_remove (void)
{
  GMainLoop *mainloop;
  guint id;

  mainloop = g_main_loop_new (NULL, FALSE);

  sighup_received = FALSE;
  id = g_unix_signal_add_watch_full (SIGHUP,
				     G_PRIORITY_DEFAULT,
				     on_sighup_received,
				     mainloop,
				     NULL);
  g_source_remove (id);
  kill (getpid (), SIGHUP);
  g_assert (!sighup_received);

}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

#ifdef TEST_THREADED
  g_thread_init (NULL);
#endif

  g_test_add_func ("/glib-unix/pipe", test_pipe);
  g_test_add_func ("/glib-unix/sighup", test_sighup);
  g_test_add_func ("/glib-unix/sighup_again", test_sighup);
  g_test_add_func ("/glib-unix/sighup_add_remove", test_sighup_add_remove);

  return g_test_run();
}
