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

#include <config.h>
#include <gio.h>

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

int
main (int argc, char *argv[])
{
  gboolean do_get = FALSE;
  gboolean do_set = FALSE;
  gboolean do_writable = FALSE;
  gboolean do_monitor = FALSE;

  gchar *path = NULL;
  gchar *schema;
  gchar *key;
  gchar *value;

  GSettings *settings;
  GVariant *v;

  GOptionContext *context;
  GOptionEntry entries[] = {
    { "get", 'g', 0, G_OPTION_ARG_NONE, &do_get, "get a key", NULL },
    { "set", 's', 0, G_OPTION_ARG_NONE, &do_set, "set a key", NULL },
    { "writable", 'w', 0, G_OPTION_ARG_NONE, &do_writable, "check if key is writable", NULL },
    { "monitor", 'm', 0, G_OPTION_ARG_NONE, &do_monitor, "monitor key for changes", NULL },
    { "path", 'p', 0, G_OPTION_ARG_STRING, &path, "path for the schema" },
    { NULL }
  };
  GError *error;

  g_type_init ();

  context = g_option_context_new ("SCHEMA KEY [VALUE]");

  g_option_context_set_summary (context, "Manipulate GSettings configuration");

  g_option_context_add_main_entries (context, entries, NULL);

  error = NULL;
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      return 1;
    }

  if (do_get + do_set + do_writable + do_monitor != 1)
    {
      g_printerr ("You must specify exactly one of --get, --set, --writable or --monitor\n");
      return 1;
    }

  if (do_get || do_writable || do_monitor)
    {
      if (argc != 3)
        {
          g_printerr ("You must specify a schema and a key\n");
          return 1;
        }

      schema = argv[1];
      key = argv[2];
    }
  else if (do_set)
    {
      if (argc != 4)
        {
          g_printerr ("You must specify a schema, a key and a value\n");
          return 1;
        }

      schema = argv[1];
      key = argv[2];
      value = argv[3];
    }

  settings = g_settings_new_with_path (schema, path);

  if (do_get)
    {
      v = g_settings_get_value (settings, key);

      g_print ("%s\n", g_variant_print (v, FALSE));

      return 0;
    }
  else if (do_writable)
    {
      gboolean writable;

      if (g_settings_is_writable (settings, key))
        g_print ("true\n");
      else
        g_print ("false\n");

      return 0;
    }
  else if (do_set)
    {
      const GVariantType *type;
      GError *error;

      v = g_settings_get_value (settings, key);
      type = g_variant_get_type (v);
      g_variant_unref (v);

      error = NULL;
      v = g_variant_parse (type, value, NULL, NULL, &error);
      if (v == NULL)
        {
          g_printerr ("%s\n", error->message);
          return 1;
        }

      if (!g_settings_set_value (settings, key, v))
        {
          g_printerr ("Key %s is not writable\n", key);
          return 1;
        }
    }
  else if (do_monitor)
    {
      gchar *detailed_signal;
      GMainLoop *loop;

      detailed_signal = g_strdup_printf ("changed::%s", key);
      g_signal_connect (settings, detailed_signal,
                        G_CALLBACK (key_changed), NULL);

      loop = g_main_loop_new (NULL, FALSE);
      g_main_loop_run (loop);
    }

  return 0;
}
