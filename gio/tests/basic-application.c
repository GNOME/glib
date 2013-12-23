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
  g_print ("\n");

  g_application_release (application);
}

static int
command_line (GApplication            *application,
              GApplicationCommandLine *cmdline)
{
  gchar **argv;
  gint argc;
  gint i;

  g_application_hold (application);
  argv = g_application_command_line_get_arguments (cmdline, &argc);

  if (argc > 1)
    {
      if (g_strcmp0 (argv[1], "echo") == 0)
        {
          g_print ("cmdline");
          for (i = 0; i < argc; i++)
            g_print (" %s", argv[i]);
          g_print ("\n");
        }
      else if (g_strcmp0 (argv[1], "env") == 0)
        {
          const gchar * const *env;

          env = g_application_command_line_get_environ (cmdline);
          g_print ("environment");
          for (i = 0; env[i]; i++)
            if (g_str_has_prefix (env[i], "TEST="))
              g_print (" %s", env[i]);      
          g_print ("\n");
        }
      else if (g_strcmp0 (argv[1], "getenv") == 0)
        {
          g_print ("getenv TEST=%s\n", g_application_command_line_getenv (cmdline, "TEST"));
        }
      else if (g_strcmp0 (argv[1], "print") == 0)
        {
          g_application_command_line_print (cmdline, "print %s\n", argv[2]);
        }
      else if (g_strcmp0 (argv[1], "printerr") == 0)
        {
          g_application_command_line_printerr (cmdline, "printerr %s\n", argv[2]);
        }
      else if (g_strcmp0 (argv[1], "file") == 0)
        {
          GFile *file;

          file = g_application_command_line_create_file_for_arg (cmdline, argv[2]);         
          g_print ("file %s\n", g_file_get_path (file));
          g_object_unref (file);
        }
      else if (g_strcmp0 (argv[1], "properties") == 0)
        {
          gboolean remote;
          GVariant *data;

          g_object_get (cmdline,
                        "is-remote", &remote,
                        NULL);

          data = g_application_command_line_get_platform_data (cmdline);
          g_assert (remote);
          g_assert (g_variant_is_of_type (data, G_VARIANT_TYPE ("a{sv}")));
          g_variant_unref (data);
          g_print ("properties ok\n");
        }
      else if (g_strcmp0 (argv[1], "cwd") == 0)
        {
          g_print ("cwd %s\n", g_application_command_line_get_cwd (cmdline));
        }
      else if (g_strcmp0 (argv[1], "busy") == 0)
        {
          g_application_mark_busy (g_application_get_default ());
          g_print ("busy\n");
        }
      else if (g_strcmp0 (argv[1], "idle") == 0)
        {
          g_application_unmark_busy (g_application_get_default ());
          g_print ("idle\n");
        }
      else if (g_strcmp0 (argv[1], "stdin") == 0)
        {
          GInputStream *stream;

          stream = g_application_command_line_get_stdin (cmdline);

          g_assert (stream == NULL || G_IS_INPUT_STREAM (stream));
          g_object_unref (stream);

          g_print ("stdin ok\n");
        }
      else
        g_print ("unexpected command: %s\n", argv[1]);
    }
  else
    g_print ("got ./cmd %d\n", g_application_command_line_get_is_remote (cmdline));

  g_strfreev (argv);
  g_application_release (application);

  return 0;
}

int
main (int argc, char **argv)
{
  GApplication *app;
  int status;

  app = g_application_new ("org.gtk.TestApplication",
                           G_APPLICATION_SEND_ENVIRONMENT |
                           (g_strcmp0 (argv[1], "./cmd") == 0
                             ? G_APPLICATION_HANDLES_COMMAND_LINE
                             : G_APPLICATION_HANDLES_OPEN));
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  g_signal_connect (app, "open", G_CALLBACK (open), NULL);
  g_signal_connect (app, "command-line", G_CALLBACK (command_line), NULL);
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
