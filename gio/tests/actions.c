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

int
main (int argc, char **argv)
{
  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/actions/basic", test_basic);

  return g_test_run ();
}
