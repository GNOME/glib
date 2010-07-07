#include <gio.h>
#include <gstdio.h>
#include <string.h>

#ifdef G_OS_UNIX
#include <stdlib.h>
#include <fcntl.h>
#endif

static gboolean action3_added = FALSE;

static void
on_app_action (GApplication *application,
               const gchar  *action_name,
               GVariant     *platform_data)
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

static void
on_app_activated (GApplication  *application,
		  GVariant      *args,
		  GVariant      *platform_data)
{
  GVariantIter iter;
  const char *key;
  GVariant *value;
  char *cwd;

  cwd = g_get_current_dir ();
  g_variant_iter_init (&iter, platform_data);
  while (g_variant_iter_next (&iter, "{&sv}", &key, &value))
    {
      const char *activate_cwd;

      if (strcmp (key, "cwd") != 0)
	continue;

      activate_cwd = g_variant_get_bytestring (value);
      g_assert_cmpstr (cwd, ==, activate_cwd);
      g_variant_unref (value);
    }

  g_free (cwd);
}

static gboolean
on_monitor_fd_io (GIOChannel *source,
		  GIOCondition condition,
		  gpointer data)
{
  exit (0);
  return FALSE;
}

int
main (int argc, char *argv[])
{
  GApplication *app;

#ifdef G_OS_UNIX
  {
    const char *slave_fd_env = g_getenv ("_G_TEST_SLAVE_FD");
    if (slave_fd_env)
      {
	int slave_fd = atoi (slave_fd_env);
	fcntl (slave_fd, F_SETFD, FD_CLOEXEC);
	g_io_add_watch (g_io_channel_unix_new (slave_fd), G_IO_HUP | G_IO_ERR,
			on_monitor_fd_io, NULL);
      }
  }
#endif

  app = g_application_unregistered_try_new ("org.gtk.test.app", 
					    argc, argv, NULL);
  if (!(argc > 1 && strcmp (argv[1], "--non-unique") == 0))
    {
      if (!g_application_register (app))
	exit (0);
    }

  if (g_application_is_remote (app))
    {
      g_application_invoke_action (app, "action1", 0);
    }
  else
    {
      g_application_add_action (app, "action1", "Action1");
      g_application_add_action (app, "action2", "Action2");
      g_signal_connect (app, "action-with-data",
                        G_CALLBACK (on_app_action), NULL);
      g_signal_connect (app, "prepare-activation",
                        G_CALLBACK (on_app_activated), NULL);
      g_application_run (app);
    }

  return 0;
}
