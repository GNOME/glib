#include <stdlib.h>
#include <gio.h>
#include <gstdio.h>
#include <string.h>

static gboolean action3_added = FALSE;

static void
on_app_action (GApplication *application,
               const gchar  *action_name,
               guint         action_timestamp)
{
  if (strcmp (action_name, "action1") == 0)
    exit (1);
  else  if (strcmp (action_name, "action2") == 0)
    {
      if (action3_added)
        g_application_remove_action (application, "action3");
      else
        g_application_add_action (application, "action3", "An extra action");
      action3_added = !action3_added;
    }
}

static gboolean
invoke_action1 (gpointer data)
{
  GApplication *app = data;

  g_application_invoke_action (app, "action1", 0);

  return FALSE;
}

static void
on_app_activated (GApplication  *application,
		  GVariant      *args,
		  GVariant      *platform_data)
{
  char *str;

  g_print ("got args: ");
  str = g_variant_print (args, TRUE);
  g_print ("%s ", str);
  g_free (str);
  str = g_variant_print (platform_data, TRUE);
  g_print ("%s\n", str);
  g_free (str);
}

int
main (int argc, char *argv[])
{
  GApplication *app;
  GMainLoop *loop;

  app = g_application_new ("org.gtk.test.app");
  if (!(argc > 1 && strcmp (argv[1], "--non-unique") == 0))
    g_application_register_with_data (app, argc, argv, NULL);

  if (g_application_is_remote (app))
    {
      g_timeout_add (1000, invoke_action1, app);
      loop = g_main_loop_new (NULL, FALSE);
      g_main_loop_run (loop);
    }
  else
    {
      g_application_add_action (app, "action1", "Action1");
      g_application_add_action (app, "action2", "Action2");
      g_signal_connect (app, "action",
                        G_CALLBACK (on_app_action), NULL);
      g_signal_connect (app, "prepare-activation",
                        G_CALLBACK (on_app_activated), NULL);
      g_application_run (app);
    }

  return 0;
}
