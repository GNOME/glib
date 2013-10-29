#include <gio/gio.h>
#include <string.h>

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

  g_application_hold (application);
  g_print ("open");

  for (i = 0; i < n_files; i++)
    {
      gchar *uri = g_file_get_uri (files[i]);
      g_print (" %s", uri);
      g_free (uri);
    }
  g_application_release (application);

  g_print ("\n");
}

int
main (int argc, char **argv)
{
  GApplication *app;
  int status;

  app = g_application_new ("org.gtk.TestApplication",
                           G_APPLICATION_SEND_ENVIRONMENT |
                           G_APPLICATION_HANDLES_OPEN |
                           (g_strcmp0 (argv[1], "./cmd") == 0 ?
                             G_APPLICATION_HANDLES_COMMAND_LINE
                           : 0));
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  g_signal_connect (app, "open", G_CALLBACK (open), NULL);
#ifdef STANDALONE
  g_application_set_inactivity_timeout (app, 10000);
#else
  g_application_set_inactivity_timeout (app, 1000);
#endif
  status = g_application_run (app, argc - 1, argv + 1);

  g_object_unref (app);

  g_print ("exit status: %d\n", status);

  return 0;
}
