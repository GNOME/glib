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
  GSimpleAction *action;
  gchar *name;
  GVariantType *parameter_type;
  gboolean enabled;
  GVariantType *state_type;
  GVariant *state;

  action = g_simple_action_new ("foo", NULL);
  g_assert (g_action_get_enabled (G_ACTION (action)));
  g_assert (g_action_get_parameter_type (G_ACTION (action)) == NULL);
  g_assert (g_action_get_state_type (G_ACTION (action)) == NULL);
  g_assert (g_action_get_state_hint (G_ACTION (action)) == NULL);
  g_assert (g_action_get_state (G_ACTION (action)) == NULL);
  g_object_get (action,
                "name", &name,
                "parameter-type", &parameter_type,
                "enabled", &enabled,
                "state-type", &state_type,
                "state", &state,
                 NULL);
  g_assert_cmpstr (name, ==, "foo");
  g_assert (parameter_type == NULL);
  g_assert (enabled);
  g_assert (state_type == NULL);
  g_assert (state == NULL);
  g_free (name);

  g_signal_connect (action, "activate", G_CALLBACK (activate), &a);
  g_assert (!a.did_run);
  g_action_activate (G_ACTION (action), NULL);
  g_assert (a.did_run);
  a.did_run = FALSE;

  g_simple_action_set_enabled (action, FALSE);
  g_action_activate (G_ACTION (action), NULL);
  g_assert (!a.did_run);

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      g_action_activate (G_ACTION (action), g_variant_new_string ("xxx"));
      exit (0);
    }
  g_test_trap_assert_failed ();

  g_object_unref (action);
  g_assert (!a.did_run);

  action = g_simple_action_new ("foo", G_VARIANT_TYPE_STRING);
  g_assert (g_action_get_enabled (G_ACTION (action)));
  g_assert (g_variant_type_equal (g_action_get_parameter_type (G_ACTION (action)), G_VARIANT_TYPE_STRING));
  g_assert (g_action_get_state_type (G_ACTION (action)) == NULL);
  g_assert (g_action_get_state_hint (G_ACTION (action)) == NULL);
  g_assert (g_action_get_state (G_ACTION (action)) == NULL);

  g_signal_connect (action, "activate", G_CALLBACK (activate), &a);
  g_assert (!a.did_run);
  g_action_activate (G_ACTION (action), g_variant_new_string ("Hello world"));
  g_assert (a.did_run);
  g_assert_cmpstr (g_variant_get_string (a.params, NULL), ==, "Hello world");
  g_variant_unref (a.params);
  a.did_run = FALSE;

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      g_action_activate (G_ACTION (action), NULL);
      exit (0);
    }

  g_test_trap_assert_failed ();

  g_object_unref (action);
  g_assert (!a.did_run);
}

static gboolean
strv_has_string (gchar       **haystack,
                 const gchar  *needle)
{
  guint n;

  for (n = 0; haystack != NULL && haystack[n] != NULL; n++)
    {
      if (g_strcmp0 (haystack[n], needle) == 0)
        return TRUE;
    }
  return FALSE;
}

static gboolean
strv_set_equal (gchar **strv, ...)
{
  gint count;
  va_list list;
  const gchar *str;
  gboolean res;

  res = TRUE;
  count = 0;
  va_start (list, strv);
  while (1)
    {
      str = va_arg (list, const gchar *);
      if (str == NULL)
        break;
      if (!strv_has_string (strv, str))
        {
          res = FALSE;
          break;
        }
      count++;
    }
  va_end (list);

  if (res)
    res = g_strv_length ((gchar**)strv) == count;

  return res;
}

static void
test_simple_group (void)
{
  GSimpleActionGroup *group;
  Activation a = { 0, };
  GSimpleAction *simple;
  GAction *action;
  gchar **actions;
  GVariant *state;

  simple = g_simple_action_new ("foo", NULL);
  g_signal_connect (simple, "activate", G_CALLBACK (activate), &a);
  g_assert (!a.did_run);
  g_action_activate (G_ACTION (simple), NULL);
  g_assert (a.did_run);
  a.did_run = FALSE;

  group = g_simple_action_group_new ();
  g_simple_action_group_insert (group, G_ACTION (simple));
  g_object_unref (simple);

  g_assert (!a.did_run);
  g_action_group_activate_action (G_ACTION_GROUP (group), "foo", NULL);
  g_assert (a.did_run);

  simple = g_simple_action_new_stateful ("bar", G_VARIANT_TYPE_STRING, g_variant_new_string ("hihi"));
  g_simple_action_group_insert (group, G_ACTION (simple));
  g_object_unref (simple);

  g_assert (g_action_group_has_action (G_ACTION_GROUP (group), "foo"));
  g_assert (g_action_group_has_action (G_ACTION_GROUP (group), "bar"));
  g_assert (!g_action_group_has_action (G_ACTION_GROUP (group), "baz"));
  actions = g_action_group_list_actions (G_ACTION_GROUP (group));
  g_assert_cmpint (g_strv_length (actions), ==, 2);
  g_assert (strv_set_equal (actions, "foo", "bar", NULL));
  g_strfreev (actions);
  g_assert (g_action_group_get_action_enabled (G_ACTION_GROUP (group), "foo"));
  g_assert (g_action_group_get_action_enabled (G_ACTION_GROUP (group), "bar"));
  g_assert (g_action_group_get_action_parameter_type (G_ACTION_GROUP (group), "foo") == NULL);
  g_assert (g_variant_type_equal (g_action_group_get_action_parameter_type (G_ACTION_GROUP (group), "bar"), G_VARIANT_TYPE_STRING));
  g_assert (g_action_group_get_action_state_type (G_ACTION_GROUP (group), "foo") == NULL);
  g_assert (g_variant_type_equal (g_action_group_get_action_state_type (G_ACTION_GROUP (group), "bar"), G_VARIANT_TYPE_STRING));
  g_assert (g_action_group_get_action_state_hint (G_ACTION_GROUP (group), "foo") == NULL);
  g_assert (g_action_group_get_action_state_hint (G_ACTION_GROUP (group), "bar") == NULL);
  g_assert (g_action_group_get_action_state (G_ACTION_GROUP (group), "foo") == NULL);
  state = g_action_group_get_action_state (G_ACTION_GROUP (group), "bar");
  g_assert (g_variant_type_equal (g_variant_get_type (state), G_VARIANT_TYPE_STRING));
  g_assert_cmpstr (g_variant_get_string (state, NULL), ==, "hihi");
  g_variant_unref (state);

  g_action_group_change_action_state (G_ACTION_GROUP (group), "bar", g_variant_new_string ("boo"));
  state = g_action_group_get_action_state (G_ACTION_GROUP (group), "bar");
  g_assert_cmpstr (g_variant_get_string (state, NULL), ==, "boo");
  g_variant_unref (state);

  action = g_simple_action_group_lookup (group, "bar");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
  g_assert (!g_action_group_get_action_enabled (G_ACTION_GROUP (group), "bar"));

  g_simple_action_group_remove (group, "bar");
  action = g_simple_action_group_lookup (group, "foo");
  g_assert_cmpstr (g_action_get_name (action), ==, "foo");
  action = g_simple_action_group_lookup (group, "bar");
  g_assert (action == NULL);

  a.did_run = FALSE;
  g_object_unref (group);
  g_assert (!a.did_run);
}

static void
test_stateful (void)
{
  GSimpleAction *action;
  GVariant *state;

  action = g_simple_action_new_stateful ("foo", NULL, g_variant_new_string ("hihi"));
  g_assert (g_action_get_enabled (G_ACTION (action)));
  g_assert (g_action_get_parameter_type (G_ACTION (action)) == NULL);
  g_assert (g_action_get_state_hint (G_ACTION (action)) == NULL);
  g_assert (g_variant_type_equal (g_action_get_state_type (G_ACTION (action)),
                                  G_VARIANT_TYPE_STRING));
  state = g_action_get_state (G_ACTION (action));
  g_assert_cmpstr (g_variant_get_string (state, NULL), ==, "hihi");
  g_variant_unref (state);

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      g_simple_action_set_state (action, g_variant_new_int32 (123));
      exit (0);
    }
  g_test_trap_assert_failed ();

  g_simple_action_set_state (action, g_variant_new_string ("hello"));
  state = g_action_get_state (G_ACTION (action));
  g_assert_cmpstr (g_variant_get_string (state, NULL), ==, "hello");
  g_variant_unref (state);

  g_object_unref (action);

  action = g_simple_action_new ("foo", NULL);
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      g_simple_action_set_state (action, g_variant_new_int32 (123));
      exit (0);
    }
  g_test_trap_assert_failed ();
  g_object_unref (action);
}

static gboolean foo_activated = FALSE;
static gboolean bar_activated = FALSE;

static void
activate_foo (GSimpleAction *simple,
              GVariant      *parameter,
              gpointer       user_data)
{
  g_assert (user_data == GINT_TO_POINTER (123));
  g_assert (parameter == NULL);
  foo_activated = TRUE;
}

static void
activate_bar (GSimpleAction *simple,
              GVariant      *parameter,
              gpointer       user_data)
{
  g_assert (user_data == GINT_TO_POINTER (123));
  g_assert_cmpstr (g_variant_get_string (parameter, NULL), ==, "param");
  bar_activated = TRUE;
}

static void
change_volume_state (GSimpleAction *action,
                     GVariant      *value,
                     gpointer       user_data)
{
  gint requested;

  requested = g_variant_get_int32 (value);

  /* Volume only goes from 0 to 10 */
  if (0 <= requested && requested <= 10)
    g_simple_action_set_state (action, value);
}

static void
test_entries (void)
{
  const GActionEntry entries[] = {
    { "foo",    activate_foo                                     },
    { "bar",    activate_bar, "s"                                },
    { "toggle", NULL,         NULL, "false"                      },
    { "volume", NULL,         NULL, "0",     change_volume_state }
  };
  GSimpleActionGroup *actions;
  GVariant *state;

  actions = g_simple_action_group_new ();
  g_simple_action_group_add_entries (actions, entries,
                                     G_N_ELEMENTS (entries),
                                     GINT_TO_POINTER (123));

  g_assert (!foo_activated);
  g_action_group_activate_action (G_ACTION_GROUP (actions), "foo", NULL);
  g_assert (foo_activated);
  foo_activated = FALSE;

  g_assert (!bar_activated);
  g_action_group_activate_action (G_ACTION_GROUP (actions), "bar",
                                  g_variant_new_string ("param"));
  g_assert (bar_activated);
  g_assert (!foo_activated);

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      const GActionEntry bad_type = {
        "bad-type", NULL, "ss"
      };

      g_simple_action_group_add_entries (actions, &bad_type, 1, NULL);
      exit (0);
    }
  g_test_trap_assert_failed ();

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      const GActionEntry bad_state = {
        "bad-state", NULL, NULL, "flse"
      };

      g_simple_action_group_add_entries (actions, &bad_state, 1, NULL);
      exit (0);
    }
  g_test_trap_assert_failed ();

  state = g_action_group_get_action_state (G_ACTION_GROUP (actions), "volume");
  g_assert_cmpint (g_variant_get_int32 (state), ==, 0);
  g_variant_unref (state);

  /* should change */
  g_action_group_change_action_state (G_ACTION_GROUP (actions), "volume",
                                      g_variant_new_int32 (7));
  state = g_action_group_get_action_state (G_ACTION_GROUP (actions), "volume");
  g_assert_cmpint (g_variant_get_int32 (state), ==, 7);
  g_variant_unref (state);

  /* should not change */
  g_action_group_change_action_state (G_ACTION_GROUP (actions), "volume",
                                      g_variant_new_int32 (11));
  state = g_action_group_get_action_state (G_ACTION_GROUP (actions), "volume");
  g_assert_cmpint (g_variant_get_int32 (state), ==, 7);
  g_variant_unref (state);

  g_object_unref (actions);
}

int
main (int argc, char **argv)
{
  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/actions/basic", test_basic);
  g_test_add_func ("/actions/simplegroup", test_simple_group);
  g_test_add_func ("/actions/stateful", test_stateful);
  g_test_add_func ("/actions/entries", test_entries);

  return g_test_run ();
}
