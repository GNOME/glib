#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>

static void
activate (GApplication *application)
{
  g_application_hold (application);
  g_print ("activated\n");
  g_application_release (application);
}

static void
activate_action (GAction  *action,
                 GVariant *parameter,
                 gpointer  data)
{
  GApplication *application = data;

  g_application_hold (application);
  g_print ("action %s activated\n", g_action_get_name (action));
  g_application_release (application);
}

static void
activate_toggle_action (GAction  *action,
                        GVariant *parameter,
                        gpointer  data)
{
  GApplication *application = data;
  GVariant *state;
  gboolean b;

  g_application_hold (application);
  g_print ("action %s activated\n", g_action_get_name (action));

  state = g_action_get_state (action);
  b = g_variant_get_boolean (state);
  g_variant_unref (state);
  g_action_set_state (action, g_variant_new_boolean (!b));

  g_application_release (application);
}

static void
add_actions (GApplication *app)
{
  GSimpleActionGroup *actions;
  GSimpleAction *action;

  actions = g_simple_action_group_new ();

  action = g_simple_action_new ("simple-action", NULL);
  g_signal_connect (action, "activate", G_CALLBACK (activate_action), app);
  g_simple_action_group_insert (actions, G_ACTION (action));

  action = g_simple_action_new_stateful ("toggle-action", NULL,
                                         g_variant_new_boolean (FALSE));
  g_signal_connect (action, "activate", G_CALLBACK (activate_toggle_action), app);
  g_simple_action_group_insert (actions, G_ACTION (action));

  g_application_set_action_group (app, G_ACTION_GROUP (actions));
}

int
main (int argc, char **argv)
{
  GApplication *app;
  int status;

  app = g_application_new ("org.gtk.TestApplication", 0);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  g_application_set_inactivity_timeout (app, 10000);

  add_actions (app);

  status = g_application_run (app, argc, argv);

  g_object_unref (app);

  return status;
}
