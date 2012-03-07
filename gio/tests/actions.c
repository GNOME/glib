#include <gio/gio.h>
#include <stdlib.h>

#include "gdbus-sessionbus.h"

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

  if (g_test_undefined ())
    {
      if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
        {
          g_action_activate (G_ACTION (action), g_variant_new_string ("xxx"));
          exit (0);
        }
      g_test_trap_assert_failed ();
    }

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

  if (g_test_undefined ())
    {
      if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
        {
          g_action_activate (G_ACTION (action), NULL);
          exit (0);
        }

      g_test_trap_assert_failed ();
    }

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
strv_strv_cmp (gchar **a, gchar **b)
{
  guint n;

  for (n = 0; a[n] != NULL; n++)
    {
       if (!strv_has_string (b, a[n]))
         return FALSE;
    }

  for (n = 0; b[n] != NULL; n++)
    {
       if (!strv_has_string (a, b[n]))
         return FALSE;
    }

  return TRUE;
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

  if (g_test_undefined ())
    {
      if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
        {
          g_simple_action_set_state (action, g_variant_new_int32 (123));
          exit (0);
        }
      g_test_trap_assert_failed ();
    }

  g_simple_action_set_state (action, g_variant_new_string ("hello"));
  state = g_action_get_state (G_ACTION (action));
  g_assert_cmpstr (g_variant_get_string (state, NULL), ==, "hello");
  g_variant_unref (state);

  g_object_unref (action);

  action = g_simple_action_new ("foo", NULL);

  if (g_test_undefined ())
    {
      if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
        {
          g_simple_action_set_state (action, g_variant_new_int32 (123));
          exit (0);
        }
      g_test_trap_assert_failed ();
    }

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

  if (g_test_undefined ())
    {
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
    }

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


GHashTable *activation_counts;

static void
count_activation (const gchar *action)
{
  gint count;

  if (activation_counts == NULL)
    activation_counts = g_hash_table_new (g_str_hash, g_str_equal);
  count = GPOINTER_TO_INT (g_hash_table_lookup (activation_counts, action));
  count++;
  g_hash_table_insert (activation_counts, (gpointer)action, GINT_TO_POINTER (count));
}

static gint
activation_count (const gchar *action)
{
  if (activation_counts == NULL)
    return 0;

  return GPOINTER_TO_INT (g_hash_table_lookup (activation_counts, action));
}

static void
activate_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  count_activation (g_action_get_name (G_ACTION (action)));
}

static void
activate_toggle (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  GVariant *old_state, *new_state;

  count_activation (g_action_get_name (G_ACTION (action)));

  old_state = g_action_get_state (G_ACTION (action));
  new_state = g_variant_new_boolean (!g_variant_get_boolean (old_state));
  g_simple_action_set_state (action, new_state);
  g_variant_unref (old_state);
}

static void
activate_radio (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  GVariant *new_state;

  count_activation (g_action_get_name (G_ACTION (action)));

  new_state = g_variant_new_string (g_variant_get_string (parameter, NULL));
  g_simple_action_set_state (action, new_state);
}

static gboolean
compare_action_groups (GActionGroup *a, GActionGroup *b)
{
  gchar **alist;
  gchar **blist;
  gint i;
  gboolean equal;
  gboolean ares, bres;
  gboolean aenabled, benabled;
  const GVariantType *aparameter_type, *bparameter_type;
  const GVariantType *astate_type, *bstate_type;
  GVariant *astate_hint, *bstate_hint;
  GVariant *astate, *bstate;

  alist = g_action_group_list_actions (a);
  blist = g_action_group_list_actions (b);
  equal = strv_strv_cmp (alist, blist);

  for (i = 0; equal && alist[i]; i++)
    {
      ares = g_action_group_query_action (a, alist[i], &aenabled, &aparameter_type, &astate_type, &astate_hint, &astate);
      bres = g_action_group_query_action (b, alist[i], &benabled, &bparameter_type, &bstate_type, &bstate_hint, &bstate);

      if (ares && bres)
        {
          equal = equal && (aenabled == benabled);
          equal = equal && ((!aparameter_type && !bparameter_type) || g_variant_type_equal (aparameter_type, bparameter_type));
          equal = equal && ((!astate_type && !bstate_type) || g_variant_type_equal (astate_type, bstate_type));
          equal = equal && ((!astate_hint && !bstate_hint) || g_variant_equal (astate_hint, bstate_hint));
          equal = equal && ((!astate && !bstate) || g_variant_equal (astate, bstate));

          if (astate_hint)
            g_variant_unref (astate_hint);
          if (bstate_hint)
            g_variant_unref (bstate_hint);
          if (astate)
            g_variant_unref (astate);
          if (bstate)
            g_variant_unref (bstate);
        }
      else
        equal = FALSE;
    }

  g_strfreev (alist);
  g_strfreev (blist);

  return equal;
}

static gboolean
stop_loop (gpointer data)
{
  GMainLoop *loop = data;

  g_main_loop_quit (loop);

  return G_SOURCE_REMOVE;
}

static void
test_dbus_export (void)
{
  GDBusConnection *bus;
  GSimpleActionGroup *group;
  GDBusActionGroup *proxy;
  GSimpleAction *action;
  GMainLoop *loop;
  static GActionEntry entries[] = {
    { "undo",  activate_action, NULL, NULL,      NULL },
    { "redo",  activate_action, NULL, NULL,      NULL },
    { "cut",   activate_action, NULL, NULL,      NULL },
    { "copy",  activate_action, NULL, NULL,      NULL },
    { "paste", activate_action, NULL, NULL,      NULL },
    { "bold",  activate_toggle, NULL, "true",    NULL },
    { "lang",  activate_radio,  "s",  "'latin'", NULL },
  };
  GError *error = NULL;
  GVariant *v;
  guint id;

  loop = g_main_loop_new (NULL, FALSE);

  session_bus_up ();
  bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);

  group = g_simple_action_group_new ();
  g_simple_action_group_add_entries (group, entries, G_N_ELEMENTS (entries), NULL);

  id = g_dbus_connection_export_action_group (bus, "/", G_ACTION_GROUP (group), &error);
  g_assert_no_error (error);

  proxy = g_dbus_action_group_get (bus, g_dbus_connection_get_unique_name (bus), "/");
  g_strfreev (g_action_group_list_actions (G_ACTION_GROUP (proxy)));

  g_timeout_add (100, stop_loop, loop);
  g_main_loop_run (loop);

  /* test that the initial transfer works */
  g_assert (G_IS_DBUS_ACTION_GROUP (proxy));
  g_assert (compare_action_groups (G_ACTION_GROUP (group), G_ACTION_GROUP (proxy)));

  /* test that various changes get propagated from group to proxy */
  action = g_simple_action_new_stateful ("italic", NULL, g_variant_new_boolean (FALSE));
  g_simple_action_group_insert (group, G_ACTION (action));
  g_object_unref (action);

  g_timeout_add (100, stop_loop, loop);
  g_main_loop_run (loop);

  g_assert (compare_action_groups (G_ACTION_GROUP (group), G_ACTION_GROUP (proxy)));

  action = G_SIMPLE_ACTION (g_simple_action_group_lookup (group, "cut"));
  g_simple_action_set_enabled (action, FALSE);

  g_timeout_add (100, stop_loop, loop);
  g_main_loop_run (loop);

  g_assert (compare_action_groups (G_ACTION_GROUP (group), G_ACTION_GROUP (proxy)));

  action = G_SIMPLE_ACTION (g_simple_action_group_lookup (group, "bold"));
  g_simple_action_set_state (action, g_variant_new_boolean (FALSE));

  g_timeout_add (100, stop_loop, loop);
  g_main_loop_run (loop);

  g_assert (compare_action_groups (G_ACTION_GROUP (group), G_ACTION_GROUP (proxy)));

  g_simple_action_group_remove (group, "italic");

  g_timeout_add (100, stop_loop, loop);
  g_main_loop_run (loop);

  g_assert (compare_action_groups (G_ACTION_GROUP (group), G_ACTION_GROUP (proxy)));

  /* test that activations and state changes propagate the other way */

  g_assert_cmpint (activation_count ("copy"), ==, 0);
  g_action_group_activate_action (G_ACTION_GROUP (proxy), "copy", NULL);

  g_timeout_add (100, stop_loop, loop);
  g_main_loop_run (loop);

  g_assert_cmpint (activation_count ("copy"), ==, 1);
  g_assert (compare_action_groups (G_ACTION_GROUP (group), G_ACTION_GROUP (proxy)));

  g_assert_cmpint (activation_count ("bold"), ==, 0);
  g_action_group_activate_action (G_ACTION_GROUP (proxy), "bold", NULL);

  g_timeout_add (100, stop_loop, loop);
  g_main_loop_run (loop);

  g_assert_cmpint (activation_count ("bold"), ==, 1);
  g_assert (compare_action_groups (G_ACTION_GROUP (group), G_ACTION_GROUP (proxy)));
  v = g_action_group_get_action_state (G_ACTION_GROUP (group), "bold");
  g_assert (g_variant_get_boolean (v));
  g_variant_unref (v);

  g_action_group_change_action_state (G_ACTION_GROUP (proxy), "bold", g_variant_new_boolean (FALSE));

  g_timeout_add (100, stop_loop, loop);
  g_main_loop_run (loop);

  g_assert_cmpint (activation_count ("bold"), ==, 1);
  g_assert (compare_action_groups (G_ACTION_GROUP (group), G_ACTION_GROUP (proxy)));
  v = g_action_group_get_action_state (G_ACTION_GROUP (group), "bold");
  g_assert (!g_variant_get_boolean (v));
  g_variant_unref (v);

  g_dbus_connection_unexport_action_group (bus, id);

  g_object_unref (proxy);
  g_object_unref (group);
  g_main_loop_unref (loop);
  g_object_unref (bus);

  session_bus_down ();
}

static gpointer
do_export (gpointer data)
{
  GActionGroup *group = data;
  GMainContext *ctx;
  gint i;
  GError *error = NULL;
  guint id;
  GDBusConnection *bus;
  GAction *action;
  gchar *path;

  ctx = g_main_context_new ();

  g_main_context_push_thread_default (ctx);

  bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  path = g_strdup_printf("/%p", data);

  for (i = 0; i < 100000; i++)
    {
      id = g_dbus_connection_export_action_group (bus, path, G_ACTION_GROUP (group), &error);
      g_assert_no_error (error);

      action = g_simple_action_group_lookup (G_SIMPLE_ACTION_GROUP (group), "a");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                   !g_action_get_enabled (action));

      g_dbus_connection_unexport_action_group (bus, id);

      while (g_main_context_iteration (ctx, FALSE));
    }

  g_free (path);
  g_object_unref (bus);

  g_main_context_pop_thread_default (ctx);

  g_main_context_unref (ctx);

  return NULL;
}

static void
test_dbus_threaded (void)
{
  GSimpleActionGroup *group[10];
  GThread *export[10];
  static GActionEntry entries[] = {
    { "a",  activate_action, NULL, NULL, NULL },
    { "b",  activate_action, NULL, NULL, NULL },
  };
  gint i;

  session_bus_up ();

  for (i = 0; i < 10; i++)
    {
      group[i] = g_simple_action_group_new ();
      g_simple_action_group_add_entries (group[i], entries, G_N_ELEMENTS (entries), NULL);
      export[i] = g_thread_new ("export", do_export, group[i]);
    }

  for (i = 0; i < 10; i++)
    g_thread_join (export[i]);

  for (i = 0; i < 10; i++)
    g_object_unref (group[i]);

  session_bus_down ();
}

int
main (int argc, char **argv)
{
  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  g_setenv ("DBUS_SESSION_BUS_ADDRESS", session_bus_get_temporary_address (), TRUE);

  g_test_add_func ("/actions/basic", test_basic);
  g_test_add_func ("/actions/simplegroup", test_simple_group);
  g_test_add_func ("/actions/stateful", test_stateful);
  g_test_add_func ("/actions/entries", test_entries);
  g_test_add_func ("/actions/dbus/export", test_dbus_export);
  g_test_add_func ("/actions/dbus/threaded", test_dbus_threaded);

  return g_test_run ();
}
