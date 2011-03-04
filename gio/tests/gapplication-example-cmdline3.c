#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>

static gboolean
my_cmdline_handler (gpointer data)
{
  GApplicationCommandLine *cmdline = data;
  gchar **argv;
  gint argc;
  gint i;

  argv = g_application_command_line_get_arguments (cmdline, &argc);

  g_application_command_line_print (cmdline,
                                    "This text is written back\n"
                                    "to stdout of the caller\n");

  for (i = 0; i < argc; i++)
    g_print ("argument %d: %s\n", i, argv[i]);

  g_strfreev (argv);

  g_application_command_line_set_exit_status (cmdline, 1);

  /* we are done handling this commandline */
  g_object_unref (cmdline);

  return FALSE;
}

static int
command_line (GApplication            *application,
              GApplicationCommandLine *cmdline)
{
  /* keep the application running until we are done with this commandline */
  g_application_hold (application);

  g_object_set_data_full (G_OBJECT (cmdline),
                          "application", application,
                          (GDestroyNotify)g_application_release);

  g_object_ref (cmdline);
  g_idle_add (my_cmdline_handler, cmdline);

  return 0;
}

int
main (int argc, char **argv)
{
  GApplication *app;
  int status;

  app = g_application_new ("org.gtk.TestApplication",
                           G_APPLICATION_HANDLES_COMMAND_LINE);
  g_signal_connect (app, "command-line", G_CALLBACK (command_line), NULL);
  g_application_set_inactivity_timeout (app, 10000);

  status = g_application_run (app, argc, argv);

  g_object_unref (app);

  return status;
}
