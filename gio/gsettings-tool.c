/* GLIB - Library of useful routines for C programming
 * Copyright (C) 2010 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Matthias Clasen
 */

#include "config.h"

#include <locale.h>
#include <gi18n.h>
#include <gio.h>

static gint
usage (gint      *argc,
       gchar    **argv[],
       gboolean   use_stdout)
{
  GOptionContext *context;
  gchar *s;

  g_set_prgname (g_path_get_basename ((*argv)[0]));

  context = g_option_context_new (_("COMMAND"));
  g_option_context_set_help_enabled (context, FALSE);
  s = g_strdup_printf (
    _("Commands:\n"
      "  help        Show this information\n"
      "  get         Get the value of a key\n"
      "  set         Set the value of a key\n"
      "  monitor     Monitor a key for changes\n"
      "  writable    Check if a key is writable\n"
      "\n"
      "Use '%s COMMAND --help' to get help for individual commands.\n"),
      g_get_prgname ());
  g_option_context_set_description (context, s);
  g_free (s);
  s = g_option_context_get_help (context, FALSE, NULL);
  if (use_stdout)
    g_print ("%s", s);
  else
    g_printerr ("%s", s);
  g_free (s);
  g_option_context_free (context);

  return use_stdout ? 0 : 1;
}

static void
remove_arg (gint num, gint *argc, gchar **argv[])
{
  gint n;

  g_assert (num <= (*argc));

  for (n = num; (*argv)[n] != NULL; n++)
    (*argv)[n] = (*argv)[n+1];
  (*argv)[n] = NULL;
  (*argc) = (*argc) - 1;
}

static void
modify_argv0_for_command (gint         *argc,
                          gchar       **argv[],
                          const gchar  *command)
{
  gchar *s;

  g_assert (g_strcmp0 ((*argv)[1], command) == 0);
  remove_arg (1, argc, argv);

  s = g_strdup_printf ("%s %s", (*argv)[0], command);
  (*argv)[0] = s;
}


static gint
handle_get (gint   *argc,
            gchar **argv[])
{
  gchar *schema;
  gchar *path;
  gchar *key;
  GSettings *settings;
  GVariant *v;
  GOptionContext *context;
  GOptionEntry entries[] = {
    { "path", 'p', 0, G_OPTION_ARG_STRING, &path, N_("Specify the path for the schema"), N_("PATH") },
    { NULL }
  };
  GError *error;
  gint ret = 1;

  modify_argv0_for_command (argc, argv, "get");

  context = g_option_context_new (_("SCHEMA KEY"));
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_set_summary (context, _("Get the value of KEY"));
  g_option_context_set_description (context,
    _("Arguments:\n"
      "  SCHEMA      The id of the schema\n"
      "  KEY         The name of the key\n"));
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);

  path = NULL;

  error = NULL;
  if (!g_option_context_parse (context, argc, argv, NULL) || *argc != 3)
    {
      gchar *s;
      s = g_option_context_get_help (context, FALSE, NULL);
      g_printerr ("%s", s);
      g_free (s);

      goto out;
    }

  schema = (*argv)[1];
  key = (*argv)[2];

  settings = g_settings_new_with_path (schema, path);

  v = g_settings_get_value (settings, key);
  g_print ("%s\n", g_variant_print (v, FALSE));
  g_variant_unref (v);
  ret = 0;

 out:
  g_option_context_free (context);

  return ret;
}

static gint
handle_set (gint   *argc,
            gchar **argv[])
{
  gchar *schema;
  gchar *path;
  gchar *key;
  gchar *value;
  GSettings *settings;
  GVariant *v;
  const GVariantType *type;
  GOptionContext *context;
  GOptionEntry entries[] = {
    { "path", 'p', 0, G_OPTION_ARG_STRING, &path, N_("Specify the path for the schema"), N_("PATH") },
    { NULL }
  };
  GError *error;
  gint ret = 1;

  modify_argv0_for_command (argc, argv, "set");

  context = g_option_context_new (_("SCHEMA KEY VALUE"));
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_set_summary (context, _("Set the value of KEY"));
  g_option_context_set_description (context,
    _("Arguments:\n"
      "  SCHEMA      The id of the schema\n"
      "  KEY         The name of the key\n"
      "  VALUE       The value to set key to, as a serialized GVariant\n"));
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);

  path = NULL;

  error = NULL;
  if (!g_option_context_parse (context, argc, argv, NULL) || *argc != 4)
    {
      gchar *s;
      s = g_option_context_get_help (context, FALSE, NULL);
      g_printerr ("%s", s);
      g_free (s);

      goto out;
    }

  schema = (*argv)[1];
  key = (*argv)[2];
  value = (*argv)[3];

  settings = g_settings_new_with_path (schema, path);

  v = g_settings_get_value (settings, key);
  type = g_variant_get_type (v);
  g_variant_unref (v);

  error = NULL;
  v = g_variant_parse (type, value, NULL, NULL, &error);
  if (v == NULL)
    {
      g_printerr ("%s\n", error->message);
      goto out;
    }

  if (!g_settings_set_value (settings, key, v))
    {
      g_printerr (_("Key %s is not writable\n"), key);
      goto out;
    }

  ret = 0;

 out:
  g_option_context_free (context);

  return ret;
}

static gint
handle_writable (gint   *argc,
                 gchar **argv[])
{
  gchar *schema;
  gchar *path;
  gchar *key;
  GSettings *settings;
  GOptionContext *context;
  GOptionEntry entries[] = {
    { "path", 'p', 0, G_OPTION_ARG_STRING, &path, N_("Specify the path for the schema"), N_("PATH") },
    { NULL }
  };
  GError *error;
  gint ret = 1;

  modify_argv0_for_command (argc, argv, "writable");

  context = g_option_context_new (_("SCHEMA KEY"));
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_set_summary (context, _("Find out whether KEY is writable"));
  g_option_context_set_description (context,
    _("Arguments:\n"
      "  SCHEMA      The id of the schema\n"
      "  KEY         The name of the key\n"));
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);

  path = NULL;

  error = NULL;
  if (!g_option_context_parse (context, argc, argv, NULL) || *argc != 3)
    {
      gchar *s;
      s = g_option_context_get_help (context, FALSE, NULL);
      g_printerr ("%s", s);
      g_free (s);

      goto out;
    }

  schema = (*argv)[1];
  key = (*argv)[2];

  settings = g_settings_new_with_path (schema, path);

  if (g_settings_is_writable (settings, key))
    g_print ("true\n");
  else
    g_print ("false\n");
  ret = 0;

 out:
  g_option_context_free (context);

  return ret;
}

static void
key_changed (GSettings   *settings,
             const gchar *key)
{
  GVariant *v;
  gchar *value;

  v = g_settings_get_value (settings, key);
  value = g_variant_print (v, FALSE);
  g_print ("%s\n", value);
  g_free (value);
  g_variant_unref (v);
}

static gint
handle_monitor (gint   *argc,
                gchar **argv[])
{
  gchar *schema;
  gchar *path;
  gchar *key;
  GSettings *settings;
  gchar *detailed_signal;
  GMainLoop *loop;
  GOptionContext *context;
  GOptionEntry entries[] = {
    { "path", 'p', 0, G_OPTION_ARG_STRING, &path, N_("Specify the path for the schema"), N_("PATH") },
    { NULL }
  };
  GError *error;
  gint ret = 1;

  modify_argv0_for_command (argc, argv, "monitor");

  context = g_option_context_new (_("SCHEMA KEY"));
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_set_summary (context,
    _("Monitor KEY for changes and print the changed values.\n"
      "Monitoring will continue until the process is terminated."));

  g_option_context_set_description (context,
    _("Arguments:\n"
      "  SCHEMA      The id of the schema\n"
      "  KEY         The name of the key\n"));
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);

  path = NULL;

  error = NULL;
  if (!g_option_context_parse (context, argc, argv, NULL) || *argc != 3)
    {
      gchar *s;
      s = g_option_context_get_help (context, FALSE, NULL);
      g_printerr ("%s", s);
      g_free (s);

      goto out;
    }

  schema = (*argv)[1];
  key = (*argv)[2];

  settings = g_settings_new_with_path (schema, path);

  detailed_signal = g_strdup_printf ("changed::%s", key);
  g_signal_connect (settings, detailed_signal,
                    G_CALLBACK (key_changed), NULL);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);
  g_main_loop_unref (loop);

 out:
  g_option_context_free (context);

  return ret;
}
int
main (int argc, char *argv[])
{
  gboolean ret = 1;

  setlocale (LC_ALL, "");

  g_type_init ();

  if (argc < 2)
    ret = usage (&argc, &argv, FALSE);
  else if (g_strcmp0 (argv[1], "help") == 0)
    ret = usage (&argc, &argv, TRUE);
  else if (g_strcmp0 (argv[1], "get") == 0)
    ret = handle_get (&argc, &argv);
  else if (g_strcmp0 (argv[1], "set") == 0)
    ret = handle_set (&argc, &argv);
  else if (g_strcmp0 (argv[1], "monitor") == 0)
    ret = handle_monitor (&argc, &argv);
  else if (g_strcmp0 (argv[1], "writable") == 0)
    ret = handle_writable (&argc, &argv);
  else
    {
      g_printerr (_("Unknown command '%s'\n"), argv[1]);
      ret = usage (&argc, &argv, FALSE);
    }

  return ret;
}
