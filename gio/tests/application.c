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
               GVariant     *platform_data)
{
  gboolean found_timestamp;
  GVariantIter *iter;
  const char *key;
  guint action_timestamp;
  GVariant *value;
  
  if (g_test_verbose ())
    {
      char *str = g_variant_print (platform_data, FALSE);
      g_print ("Action '%s' invoked (data: %s, expected: %u)\n",
	       action_name,
	       str,
	       timestamp);
      g_free (str);
    }

  g_assert_cmpstr (action_name, ==, "About");

  g_variant_get (platform_data, "a{sv}", &iter);
  found_timestamp = FALSE;
  while (g_variant_iter_next (iter, "{&sv}",
			      &key, &value))
    {
      if (g_strcmp0 ("timestamp", key) == 0)
	{
	  found_timestamp = TRUE;
	  g_variant_get (value, "u", &action_timestamp);
	  break;
	}
    }

  g_variant_iter_free (iter);

  g_assert_cmpuint (timestamp, ==, action_timestamp);

  action_invoked = TRUE;
}

static GVariant *
create_timestamp_data ()
{
  GVariantBuilder builder;

  timestamp = 42 + timestamp;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));
  g_variant_builder_add (&builder, "{sv}",
			 "timestamp", g_variant_new ("u", timestamp));
  
  return g_variant_builder_end (&builder);
}

static gboolean
check_invoke_action (gpointer data)
{
  GApplication *application = data;

  if (state == INVOKE_ACTION)
    {
      if (g_test_verbose ())
        g_print ("Invoking About...\n");

      g_application_invoke_action (application, "About", create_timestamp_data ());
      
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

      g_application_invoke_action (application, "About", create_timestamp_data ());
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

      g_application_quit_with_data (application, create_timestamp_data ());
      return FALSE;
    }

  g_assert_not_reached ();
}

static void
test_basic (void)
{
  GApplication *app;
  const gchar *appid;
  gboolean quit;
  gboolean remote;
  gboolean reg;
  gchar **actions;

  app = g_application_new ("org.gtk.TestApplication", 0, NULL);

  g_assert (g_application_get_instance () == app);
  g_assert_cmpstr (g_application_get_id (app), ==, "org.gtk.TestApplication");
  g_object_get (app,
                "application-id", &appid,
                "default-quit", &quit,
                "is-remote", &remote,
                "register", &reg,
                NULL);
  g_assert_cmpstr (appid, ==, "org.gtk.TestApplication");
  g_assert (quit);
  g_assert (!remote);
  g_assert (reg);

  g_application_add_action (app, "About", "Print an about message");

  g_assert (g_application_get_action_enabled (app, "About"));
  g_assert_cmpstr (g_application_get_action_description (app, "About"), ==, "Print an about message");

  actions = g_application_list_actions (app);
  g_assert_cmpint (g_strv_length (actions), ==, 1);
  g_assert_cmpstr (actions[0], ==, "About");
  g_strfreev (actions);

  g_application_add_action (app, "Action2", "Another action");
  actions = g_application_list_actions (app);
  g_assert_cmpint (g_strv_length (actions), ==, 2);
  g_strfreev (actions);
  g_application_remove_action (app, "Action2");
  actions = g_application_list_actions (app);
  g_assert_cmpint (g_strv_length (actions), ==, 1);
  g_strfreev (actions);

  g_signal_connect (app, "action-with-data::About", G_CALLBACK (on_app_action), NULL);

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
