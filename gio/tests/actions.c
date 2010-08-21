#include <gio/gio.h>
#include <stdlib.h>

typedef struct
{
  GVariant *params;
  gboolean did_run;
} Activation;

static void
activate (GAction  *action,
          GVariant *parameter,
          gpointer  user_data)
{
  Activation *activation = user_data;

  if (parameter)
    activation->params = g_variant_ref (parameter);
  else
    activation->params = NULL;
  activation->did_run = TRUE;
}

static void
test_basic (void)
{
  Activation a = { 0, };
  GAction *action;

  action = g_action_new ("foo", NULL);
  g_signal_connect (action, "activate", G_CALLBACK (activate), &a);
  g_assert (!a.did_run);
  g_action_activate (action, NULL);
  g_assert (a.did_run);
  a.did_run = FALSE;

  g_action_set_enabled (action, FALSE);
  g_action_activate (action, NULL);
  g_assert (!a.did_run);

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      g_action_activate (action, g_variant_new_string ("xxx"));
      exit (0);
    }
  g_test_trap_assert_failed ();

  g_object_unref (action);
  g_assert (!a.did_run);

  action = g_action_new ("foo", G_VARIANT_TYPE_STRING);
  g_signal_connect (action, "activate", G_CALLBACK (activate), &a);
  g_assert (!a.did_run);
  g_action_activate (action, g_variant_new_string ("Hello world"));
  g_assert (a.did_run);
  g_assert_cmpstr (g_variant_get_string (a.params, NULL), ==, "Hello world");
  g_variant_unref (a.params);
  a.did_run = FALSE;

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      g_action_activate (action, NULL);
      exit (0);
    }

  g_test_trap_assert_failed ();

  g_object_unref (action);
  g_assert (!a.did_run);
}

static void
test_simple_group (void)
{
  GSimpleActionGroup *group;
  Activation a = { 0, };
  GAction *action;

  action = g_action_new ("foo", NULL);
  g_signal_connect (action, "activate", G_CALLBACK (activate), &a);
  g_assert (!a.did_run);
  g_action_activate (action, NULL);
  g_assert (a.did_run);
  a.did_run = FALSE;

  group = g_simple_action_group_new ();
  g_simple_action_group_insert (group, action);
  g_object_unref (action);

  g_assert (!a.did_run);
  g_action_group_activate (G_ACTION_GROUP (group), "foo", NULL);
  g_assert (a.did_run);

  a.did_run = FALSE;
  g_object_unref (group);
  g_assert (!a.did_run);
}

static void
test_stateful (void)
{
  GAction *action;

  action = g_action_new_stateful ("foo", NULL, g_variant_new_string ("hihi"));
  g_assert (g_variant_type_equal (g_action_get_state_type (action),
                                  G_VARIANT_TYPE_STRING));
  g_assert_cmpstr (g_variant_get_string (g_action_get_state (action), NULL),
                   ==, "hihi");

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      g_action_set_state (action, g_variant_new_int32 (123));
      exit (0);
    }
  g_test_trap_assert_failed ();

  g_action_set_state (action, g_variant_new_string ("hello"));
  g_assert_cmpstr (g_variant_get_string (g_action_get_state (action), NULL),
                   ==, "hello");

  g_object_unref (action);

  action = g_action_new ("foo", NULL);
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      g_action_set_state (action, g_variant_new_int32 (123));
      exit (0);
    }
  g_test_trap_assert_failed ();
  g_object_unref (action);
}

int
main (int argc, char **argv)
{
  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/actions/basic", test_basic);
  g_test_add_func ("/actions/simplegroup", test_simple_group);
  g_test_add_func ("/actions/stateful", test_stateful);

  return g_test_run ();
}
