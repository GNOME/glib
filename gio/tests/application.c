#include <stdlib.h>
#include <gio.h>
#include <gstdio.h>

#include "gdbus-sessionbus.h"

enum
{
  INVOKE_ACTION,
  CHECK_ACTION,
  DISABLE_ACTION,
  INVOKE_DISABLED_ACTION,
  CHECK_DISABLED_ACTION,
  END
};

static guint timestamp = 0;
static gint state = -1;
static gboolean action_invoked = FALSE;

static void
on_app_action (GApplication *application,
               const gchar  *action_name,
               guint         action_timestamp)
{
  if (g_test_verbose ())
    g_print ("Action '%s' invoked (timestamp: %u, expected: %u)\n",
             action_name,
             action_timestamp,
             timestamp);

  g_assert_cmpstr (action_name, ==, "About");
  g_assert_cmpint (action_timestamp, ==, timestamp);

  action_invoked = TRUE;
}

static gboolean
check_invoke_action (gpointer data)
{
  GApplication *application = data;

  if (state == INVOKE_ACTION)
    {
      timestamp = (guint) time (NULL);

      if (g_test_verbose ())
        g_print ("Invoking About...\n");

      g_application_invoke_action (application, "About", timestamp);
      state = CHECK_ACTION;
      return TRUE;
    }

  if (state == CHECK_ACTION)
    {
      if (g_test_verbose ())
        g_print ("Verifying About invocation...\n");

      g_assert (action_invoked);
      state = DISABLE_ACTION;
      return TRUE;
    }

  if (state == DISABLE_ACTION)
    {
      if (g_test_verbose ())
        g_print ("Disabling About...\n");

      g_application_set_action_enabled (application, "About", FALSE);
      action_invoked = FALSE;
      state = INVOKE_DISABLED_ACTION;
      return TRUE;
    }

  if (state == INVOKE_DISABLED_ACTION)
    {
      if (g_test_verbose ())
        g_print ("Invoking disabled About action...\n");

      g_application_invoke_action (application, "About", (guint) time (NULL));
      state = CHECK_DISABLED_ACTION;
      return TRUE;
    }

  if (state == CHECK_DISABLED_ACTION)
    {
      if (g_test_verbose ())
        g_print ("Verifying lack of About invocation...\n");

      g_assert (!action_invoked);
      state = END;
      return TRUE;
    }

  if (state == END)
    {
      if (g_test_verbose ())
        g_print ("Test complete\n");

      g_application_quit (application, (guint) time (NULL));
      return FALSE;
    }

  g_assert_not_reached ();
}

static void
test_basic (void)
{
  GApplication *app;

  app = g_application_new_and_register ("org.gtk.TestApplication", 0, NULL);
  g_application_add_action (app, "About", "Print an about message");

  g_signal_connect (app, "action::About", G_CALLBACK (on_app_action), NULL);

  state = INVOKE_ACTION;
  g_timeout_add (100, check_invoke_action, app);

  g_application_run (app);

  g_assert (state == END);
  g_object_unref (app);
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

  g_test_add_func ("/application/basic", test_basic);

  ret = g_test_run ();

  session_bus_down ();

  return ret;
}
