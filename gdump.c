/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Dump introspection data
 *
 * Copyright (C) 2008 Colin Walters <walters@verbum.org>
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
 */

#include <stdlib.h>

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

/* This file is both compiled into libgirepository.so, and installed
 * on the filesystem.  But for the dumper, we want to avoid linking
 * to libgirepository; see
 * https://bugzilla.gnome.org/show_bug.cgi?id=630342
 */
#ifdef G_IREPOSITORY_COMPILATION
#include "config.h"
#include "girepository.h"
#endif

#include <string.h>

static void
escaped_printf (GOutputStream *out, const char *fmt, ...) G_GNUC_PRINTF (2, 3);

static void
escaped_printf (GOutputStream *out, const char *fmt, ...)
{
  char *str;
  va_list args;
  gsize written;
  GError *error = NULL;

  va_start (args, fmt);

  str = g_markup_vprintf_escaped (fmt, args);
  if (!g_output_stream_write_all (out, str, strlen (str), &written, NULL, &error))
    {
      g_critical ("failed to write to iochannel: %s", error->message);
      g_clear_error (&error);
    }
  g_free (str);

  va_end (args);
}

static void
goutput_write (GOutputStream *out, const char *str)
{
  gsize written;
  GError *error = NULL;
  if (!g_output_stream_write_all (out, str, strlen (str), &written, NULL, &error))
    {
      g_critical ("failed to write to iochannel: %s", error->message);
      g_clear_error (&error);
    }
}

typedef GType (*GetTypeFunc)(void);
typedef GQuark (*ErrorQuarkFunc)(void);

static GType
invoke_get_type (GModule *self, const char *symbol, GError **error)
{
  GetTypeFunc sym;
  GType ret;

  if (!g_module_symbol (self, symbol, (void**)&sym))
    {
      g_set_error (error,
		   G_IO_ERROR,
		   G_IO_ERROR_FAILED,
		   "Failed to find symbol '%s'", symbol);
      return G_TYPE_INVALID;
    }

  ret = sym ();
  if (ret == G_TYPE_INVALID)
    {
      g_set_error (error,
		   G_IO_ERROR,
		   G_IO_ERROR_FAILED,
		   "Function '%s' returned G_TYPE_INVALID", symbol);
    }
  return ret;
}

static GQuark
invoke_error_quark (GModule *self, const char *symbol, GError **error)
{
  ErrorQuarkFunc sym;

  if (!g_module_symbol (self, symbol, (void**)&sym))
    {
      g_set_error (error,
		   G_IO_ERROR,
		   G_IO_ERROR_FAILED,
		   "Failed to find symbol '%s'", symbol);
      return G_TYPE_INVALID;
    }

  return sym ();
}

static void
dump_properties (GType type, GOutputStream *out)
{
  guint i;
  guint n_properties;
  GParamSpec **props;

  if (G_TYPE_FUNDAMENTAL (type) == G_TYPE_OBJECT)
    {
      GObjectClass *klass;
      klass = g_type_class_ref (type);
      props = g_object_class_list_properties (klass, &n_properties);
    }
  else
    {
      void *klass;
      klass = g_type_default_interface_ref (type);
      props = g_object_interface_list_properties (klass, &n_properties);
    }

  for (i = 0; i < n_properties; i++)
    {
      GParamSpec *prop;

      prop = props[i];
      if (prop->owner_type != type)
	continue;

      escaped_printf (out, "    <property name=\"%s\" type=\"%s\" flags=\"%d\"/>\n",
		      prop->name, g_type_name (prop->value_type), prop->flags);
    }
  g_free (props);
}

static void
dump_signals (GType type, GOutputStream *out)
{
  guint i;
  guint n_sigs;
  guint *sig_ids;

  sig_ids = g_signal_list_ids (type, &n_sigs);
  for (i = 0; i < n_sigs; i++)
    {
      guint sigid;
      GSignalQuery query;
      guint j;

      sigid = sig_ids[i];
      g_signal_query (sigid, &query);

      escaped_printf (out, "    <signal name=\"%s\" return=\"%s\"",
		      query.signal_name, g_type_name (query.return_type));

      if (query.signal_flags & G_SIGNAL_RUN_FIRST)
        escaped_printf (out, " when=\"first\"");
      else if (query.signal_flags & G_SIGNAL_RUN_LAST)
        escaped_printf (out, " when=\"last\"");
      else if (query.signal_flags & G_SIGNAL_RUN_CLEANUP)
        escaped_printf (out, " when=\"cleanup\"");
#if GLIB_CHECK_VERSION(2, 29, 15)
      else if (query.signal_flags & G_SIGNAL_MUST_COLLECT)
        escaped_printf (out, " when=\"must-collect\"");
#endif
      if (query.signal_flags & G_SIGNAL_NO_RECURSE)
        escaped_printf (out, " no-recurse=\"1\"");

      if (query.signal_flags & G_SIGNAL_DETAILED)
        escaped_printf (out, " detailed=\"1\"");

      if (query.signal_flags & G_SIGNAL_ACTION)
        escaped_printf (out, " action=\"1\"");

      if (query.signal_flags & G_SIGNAL_NO_HOOKS)
        escaped_printf (out, " no-hooks=\"1\"");

      goutput_write (out, ">\n");

      for (j = 0; j < query.n_params; j++)
	{
	  escaped_printf (out, "      <param type=\"%s\"/>\n",
			  g_type_name (query.param_types[j]));
	}
      goutput_write (out, "    </signal>\n");
    }
}

static void
dump_object_type (GType type, const char *symbol, GOutputStream *out)
{
  guint n_interfaces;
  guint i;
  GType *interfaces;

  escaped_printf (out, "  <class name=\"%s\" get-type=\"%s\"",
		  g_type_name (type), symbol);
  if (type != G_TYPE_OBJECT)
    {
      GString *parent_str;
      GType parent;
      gboolean first = TRUE;

      parent = g_type_parent (type);
      parent_str = g_string_new ("");
      while (parent != G_TYPE_INVALID)
        {
          if (first)
            first = FALSE;
          else
            g_string_append_c (parent_str, ',');
          g_string_append (parent_str, g_type_name (parent));
          parent = g_type_parent (parent);
        }

      escaped_printf (out, " parents=\"%s\"", parent_str->str);

      g_string_free (parent_str, TRUE);
    }

  if (G_TYPE_IS_ABSTRACT (type))
    escaped_printf (out, " abstract=\"1\"");
  goutput_write (out, ">\n");

  interfaces = g_type_interfaces (type, &n_interfaces);
  for (i = 0; i < n_interfaces; i++)
    {
      GType itype = interfaces[i];
      escaped_printf (out, "    <implements name=\"%s\"/>\n",
		      g_type_name (itype));
    }
  dump_properties (type, out);
  dump_signals (type, out);
  goutput_write (out, "  </class>\n");
}

static void
dump_interface_type (GType type, const char *symbol, GOutputStream *out)
{
  guint n_interfaces;
  guint i;
  GType *interfaces;

  escaped_printf (out, "  <interface name=\"%s\" get-type=\"%s\">\n",
		  g_type_name (type), symbol);

  interfaces = g_type_interface_prerequisites (type, &n_interfaces);
  for (i = 0; i < n_interfaces; i++)
    {
      GType itype = interfaces[i];
      if (itype == G_TYPE_OBJECT)
	{
	  /* Treat this as implicit for now; in theory GInterfaces are
	   * supported on things like GstMiniObject, but right now
	   * the introspection system only supports GObject.
	   * http://bugzilla.gnome.org/show_bug.cgi?id=559706
	   */
	  continue;
	}
      escaped_printf (out, "    <prerequisite name=\"%s\"/>\n",
		      g_type_name (itype));
    }
  dump_properties (type, out);
  dump_signals (type, out);
  goutput_write (out, "  </interface>\n");
}

static void
dump_boxed_type (GType type, const char *symbol, GOutputStream *out)
{
  escaped_printf (out, "  <boxed name=\"%s\" get-type=\"%s\"/>\n",
		  g_type_name (type), symbol);
}

static void
dump_flags_type (GType type, const char *symbol, GOutputStream *out)
{
  guint i;
  GFlagsClass *klass;

  klass = g_type_class_ref (type);
  escaped_printf (out, "  <flags name=\"%s\" get-type=\"%s\">\n",
		  g_type_name (type), symbol);

  for (i = 0; i < klass->n_values; i++)
    {
      GFlagsValue *value = &(klass->values[i]);

      escaped_printf (out, "    <member name=\"%s\" nick=\"%s\" value=\"%d\"/>\n",
		      value->value_name, value->value_nick, value->value);
    }
  goutput_write (out, "  </flags>\n");
}

static void
dump_enum_type (GType type, const char *symbol, GOutputStream *out)
{
  guint i;
  GEnumClass *klass;

  klass = g_type_class_ref (type);
  escaped_printf (out, "  <enum name=\"%s\" get-type=\"%s\">\n",
		  g_type_name (type), symbol);

  for (i = 0; i < klass->n_values; i++)
    {
      GEnumValue *value = &(klass->values[i]);

      escaped_printf (out, "    <member name=\"%s\" nick=\"%s\" value=\"%d\"/>\n",
		      value->value_name, value->value_nick, value->value);
    }
  goutput_write (out, "  </enum>");
}

static void
dump_fundamental_type (GType type, const char *symbol, GOutputStream *out)
{
  guint n_interfaces;
  guint i;
  GType *interfaces;
  GString *parent_str;
  GType parent;
  gboolean first = TRUE;


  escaped_printf (out, "  <fundamental name=\"%s\" get-type=\"%s\"",
		  g_type_name (type), symbol);

  if (G_TYPE_IS_ABSTRACT (type))
    escaped_printf (out, " abstract=\"1\"");

  if (G_TYPE_IS_INSTANTIATABLE (type))
    escaped_printf (out, " instantiatable=\"1\"");

  parent = g_type_parent (type);
  parent_str = g_string_new ("");
  while (parent != G_TYPE_INVALID)
    {
      if (first)
        first = FALSE;
      else
        g_string_append_c (parent_str, ',');
      if (!g_type_name (parent))
        break;
      g_string_append (parent_str, g_type_name (parent));
      parent = g_type_parent (parent);
    }

  if (parent_str->len > 0)
    escaped_printf (out, " parents=\"%s\"", parent_str->str);
  g_string_free (parent_str, TRUE);

  goutput_write (out, ">\n");

  interfaces = g_type_interfaces (type, &n_interfaces);
  for (i = 0; i < n_interfaces; i++)
    {
      GType itype = interfaces[i];
      escaped_printf (out, "    <implements name=\"%s\"/>\n",
		      g_type_name (itype));
    }
  goutput_write (out, "  </fundamental>\n");
}

static void
dump_type (GType type, const char *symbol, GOutputStream *out)
{
  switch (g_type_fundamental (type))
    {
    case G_TYPE_OBJECT:
      dump_object_type (type, symbol, out);
      break;
    case G_TYPE_INTERFACE:
      dump_interface_type (type, symbol, out);
      break;
    case G_TYPE_BOXED:
      dump_boxed_type (type, symbol, out);
      break;
    case G_TYPE_FLAGS:
      dump_flags_type (type, symbol, out);
      break;
    case G_TYPE_ENUM:
      dump_enum_type (type, symbol, out);
      break;
    case G_TYPE_POINTER:
      /* GValue, etc.  Just skip them. */
      break;
    default:
      dump_fundamental_type (type, symbol, out);
      break;
    }
}

static void
dump_error_quark (GQuark quark, const char *symbol, GOutputStream *out)
{
  escaped_printf (out, "  <error-quark function=\"%s\" domain=\"%s\"/>\n",
		  symbol, g_quark_to_string (quark));
}

/**
 * g_irepository_dump:
 * @arg: Comma-separated pair of input and output filenames
 * @error: a %GError
 *
 * Argument specified is a comma-separated pair of filenames; i.e. of
 * the form "input.txt,output.xml".  The input file should be a
 * UTF-8 Unix-line-ending text file, with each line containing either
 * "get-type:" followed by the name of a GType _get_type function, or
 * "error-quark:" followed by the name of an error quark function.  No
 * extra whitespace is allowed.
 *
 * The output file should already exist, but be empty.  This function will
 * overwrite its contents.
 *
 * Returns: %TRUE on success, %FALSE on error
 */
#ifndef G_IREPOSITORY_COMPILATION
static gboolean
dump_irepository (const char *arg, GError **error) G_GNUC_UNUSED;
static gboolean
dump_irepository (const char *arg, GError **error)
#else
gboolean
g_irepository_dump (const char *arg, GError **error)
#endif
{
  GHashTable *output_types;
  char **args;
  GFile *input_file;
  GFile *output_file;
  GFileInputStream *input;
  GFileOutputStream *output;
  GDataInputStream *in;
  GModule *self;
  gboolean caught_error = FALSE;

  self = g_module_open (NULL, 0);
  if (!self)
    {
      g_set_error (error,
		   G_IO_ERROR,
		   G_IO_ERROR_FAILED,
		   "failed to open self: %s",
		   g_module_error ());
      return FALSE;
    }

  args = g_strsplit (arg, ",", 2);

  input_file = g_file_new_for_path (args[0]);
  output_file = g_file_new_for_path (args[1]);

  input = g_file_read (input_file, NULL, error);
  if (input == NULL)
    return FALSE;

  output = g_file_replace (output_file, NULL, FALSE, 0, NULL, error);
  if (output == NULL)
    {
      g_input_stream_close (G_INPUT_STREAM (input), NULL, NULL);
      return FALSE;
    }

  goutput_write (G_OUTPUT_STREAM (output), "<?xml version=\"1.0\"?>\n");
  goutput_write (G_OUTPUT_STREAM (output), "<dump>\n");

  output_types = g_hash_table_new (NULL, NULL);

  in = g_data_input_stream_new (G_INPUT_STREAM (input));
  g_object_unref (input);

  while (TRUE)
    {
      gsize len;
      char *line = g_data_input_stream_read_line (in, &len, NULL, NULL);
      const char *function;

      if (line == NULL || *line == '\0')
        {
          g_free (line);
          break;
        }

      g_strchomp (line);

      if (strncmp (line, "get-type:", strlen ("get-type:")) == 0)
        {
          GType type;

          function = line + strlen ("get-type:");

          type = invoke_get_type (self, function, error);

          if (type == G_TYPE_INVALID)
            {
              g_printerr ("Invalid GType function: '%s'\n", function);
              caught_error = TRUE;
              g_free (line);
              break;
            }

          if (g_hash_table_lookup (output_types, (gpointer) type))
            goto next;
          g_hash_table_insert (output_types, (gpointer) type, (gpointer) type);

          dump_type (type, function, G_OUTPUT_STREAM (output));
        }
      else if (strncmp (line, "error-quark:", strlen ("error-quark:")) == 0)
        {
          GQuark quark;
          function = line + strlen ("error-quark:");
          quark = invoke_error_quark (self, function, error);

          if (quark == 0)
            {
              g_printerr ("Invalid error quark function: '%s'\n", function);
              caught_error = TRUE;
              g_free (line);
              break;
            }

          dump_error_quark (quark, function, G_OUTPUT_STREAM (output));
        }


    next:
      g_free (line);
    }

  g_hash_table_destroy (output_types);

  goutput_write (G_OUTPUT_STREAM (output), "</dump>\n");

  {
    GError **ioerror;
    /* Avoid overwriting an earlier set error */
    if (caught_error)
      ioerror = NULL;
    else
      ioerror = error;
    if (!g_input_stream_close (G_INPUT_STREAM (in), NULL, ioerror))
      return FALSE;
    if (!g_output_stream_close (G_OUTPUT_STREAM (output), NULL, ioerror))
      return FALSE;
  }

  return !caught_error;
}
