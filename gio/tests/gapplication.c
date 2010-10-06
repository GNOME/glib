#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>
#include "gdbus-sessionbus.h"

static void
activate (GApplication *application)
{
  g_application_hold (application);
  g_print ("activated\n");
  g_application_release (application);
}

static void
open (GApplication  *application,
      GFile        **files,
      gint           n_files,
      const gchar   *hint)
{
  gint i;

  g_print ("open");

  for (i = 0; i < n_files; i++)
    {
      gchar *uri = g_file_get_uri (files[i]);
      g_print (" %s", uri);
      g_free (uri);
    }

  g_print ("\n");
}

static int
app_main (int argc, char **argv)
{
  GApplication *app;
  int status;

  app = g_application_new ("org.gtk.TestApplication",
                           G_APPLICATION_FLAGS_HANDLES_OPEN);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  g_signal_connect (app, "open", G_CALLBACK (open), NULL);
  g_application_set_inactivity_timeout (app, 1000);
  status = g_application_run (app, argc, argv);
  g_object_unref (app);

  return status;
}

static gint outstanding_watches;
static GMainLoop *main_loop;

typedef struct
{
  const gchar *expected_stdout;
  gint stdout_pipe;
} ChildData;

static void
child_quit (GPid     pid,
            gint     status,
            gpointer data)
{
  ChildData *child = data;
  gssize expected, actual;
  gchar *buffer;

  g_assert_cmpint (status, ==, 0);

  if (--outstanding_watches == 0)
    g_main_loop_quit (main_loop);

  expected = strlen (child->expected_stdout);
  buffer = g_alloca (expected + 100);
  actual = read (child->stdout_pipe, buffer, expected + 100);
  close (child->stdout_pipe);

  g_assert_cmpint (actual, >=, 0);

  if (actual != expected ||
      memcmp (buffer, child->expected_stdout, expected) != 0)
    {
      buffer[MIN(expected + 100, actual)] = '\0';

      g_error ("\nExpected\n-----\n%s-----\nGot (%s)\n-----\n%s-----\n",
               child->expected_stdout,
               (actual > expected) ? "truncated" : "full", buffer);
    }

  g_slice_free (ChildData, child);
}

static void
spawn (const gchar *expected_stdout,
       const gchar *first_arg,
       ...)
{
  gint pipefd[2];
  GPid pid;

  /* We assume that the child will not produce enough stdout
   * to deadlock by filling the pipe.
   */
  pipe (pipefd);
  pid = fork ();

  if (pid == 0)
    {
      const gchar *arg;
      GPtrArray *array;
      gchar **args;
      int status;
      va_list ap;

      dup2 (pipefd[1], 1);
      close (pipefd[0]);
      close (pipefd[1]);

      va_start (ap, first_arg);
      array = g_ptr_array_new ();
      g_ptr_array_add (array, g_strdup ("./app"));
      for (arg = first_arg; arg; arg = va_arg (ap, const gchar *))
        g_ptr_array_add (array, g_strdup (arg));
      g_ptr_array_add (array, NULL);
      args = (gchar **) g_ptr_array_free (array, FALSE);

      status = app_main (g_strv_length (args), args);
      g_strfreev (args);

      g_print ("exit status: %d\n", status);
      exit (0);
    }
  else
    {
      ChildData *data;

      data = g_slice_new (ChildData);
      data->expected_stdout = expected_stdout;
      data->stdout_pipe = pipefd[0];
      close (pipefd[1]);

      g_child_watch_add (pid, child_quit, data);
      outstanding_watches++;
    }
}

static void
basic (void)
{
  session_bus_up ();

  main_loop = g_main_loop_new (NULL, 0);

  /* spawn the master */
  spawn ("activated\nopen file:///a file:///b\nexit status: 0\n", NULL);

  /* make sure it becomes the master */
  g_usleep (100000);

  /* send it some files */
  spawn ("exit status: 0\n", "/a", "/b", NULL);

  g_main_loop_run (main_loop);

  session_bus_down ();
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gapplication/basic", basic);

  return g_test_run ();
}
