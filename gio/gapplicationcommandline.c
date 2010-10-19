/*
 * Copyright © 2010 Codethink Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * licence or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,
 * USA.
 *
 * Authors: Ryan Lortie <desrt@desrt.ca>
 */

#include "gapplicationcommandline.h"

#include <string.h>
#include <stdio.h>

G_DEFINE_TYPE (GApplicationCommandLine, g_application_command_line, G_TYPE_OBJECT)

/**
 * SECTION:gapplicationcommandline
 * @title: GApplicationCommandLine
 * @short_description: A class representing a command-line invocation of
 *                     this application.
 * @see_also: #GApplication
 *
 * #GApplicationCommandLine represents a command-line invocation of
 * containing application.  It is created by #GApplication and emitted
 * in the <varname>command-line</varname> signal and virtual function.
 *
 * The class contains the list of arguments that the program was invoked
 * with.  It is also possible to query if the commandline invocation was
 * local (ie: the current process is running in direct response to the
 * invocation) or remote (ie: some other process forwarded the
 * commandline to this process).
 *
 * The exit status of the originally-invoked process may be set and
 * messages can be printed to stdout or stderr of that process.  The
 * lifecycle of the originally-invoked process is tied to the lifecycle
 * of this object (ie: the process exits when the last reference is
 * dropped).
 **/

enum
{
  PROP_NONE,
  PROP_ARGUMENTS,
  PROP_PLATFORM_DATA,
  PROP_IS_REMOTE
};

struct _GApplicationCommandLinePrivate
{
  GVariant *platform_data;
  GVariant *arguments;
  GVariant *cwd;
  gint exit_status;
};

/* All subclasses represent remote invocations of some kind. */
#define IS_REMOTE(cmdline) (G_TYPE_FROM_INSTANCE (cmdline) != \
                            G_TYPE_APPLICATION_COMMAND_LINE)

static void
grok_platform_data (GApplicationCommandLine *cmdline)
{
  GVariantIter iter;
  const gchar *key;
  GVariant *value;

  g_variant_iter_init (&iter, cmdline->priv->platform_data);

  while (g_variant_iter_loop (&iter, "{&sv}", &key, &value))
    if (strcmp (key, "cwd") == 0)
      {
        if (!cmdline->priv->cwd)
          cmdline->priv->cwd = g_variant_ref (value);
      }
}

static void
g_application_command_line_real_print_literal (GApplicationCommandLine *cmdline,
                                               const gchar             *message)
{
  g_print ("%s\n", message);
}

static void
g_application_command_line_real_printerr_literal (GApplicationCommandLine *cmdline,
                                                  const gchar             *message)
{
  g_printerr ("%s\n", message);
}

static void
g_application_command_line_get_property (GObject *object, guint prop_id,
                                         GValue *value, GParamSpec *pspec)
{
  GApplicationCommandLine *cmdline = G_APPLICATION_COMMAND_LINE (object);

  switch (prop_id)
    {
    case PROP_ARGUMENTS:
      g_value_set_variant (value, cmdline->priv->arguments);
      break;

    case PROP_PLATFORM_DATA:
      g_value_set_variant (value, cmdline->priv->platform_data);
      break;

    case PROP_IS_REMOTE:
      g_value_set_boolean (value, IS_REMOTE (cmdline));
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
g_application_command_line_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  GApplicationCommandLine *cmdline = G_APPLICATION_COMMAND_LINE (object);

  switch (prop_id)
    {
    case PROP_ARGUMENTS:
      g_assert (cmdline->priv->arguments == NULL);
      cmdline->priv->arguments = g_value_dup_variant (value);
      break;

    case PROP_PLATFORM_DATA:
      g_assert (cmdline->priv->platform_data == NULL);
      cmdline->priv->platform_data = g_value_dup_variant (value);
      if (cmdline->priv->platform_data != NULL)
        grok_platform_data (cmdline);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
g_application_command_line_finalize (GObject *object)
{
  GApplicationCommandLine *cmdline = G_APPLICATION_COMMAND_LINE (object);

  if (cmdline->priv->platform_data)
    g_variant_unref (cmdline->priv->platform_data);
  if (cmdline->priv->arguments)
    g_variant_unref (cmdline->priv->arguments);
  if (cmdline->priv->cwd)
    g_variant_unref (cmdline->priv->cwd);

  G_OBJECT_CLASS (g_application_command_line_parent_class)
    ->finalize (object);
}

static void
g_application_command_line_init (GApplicationCommandLine *cmdline)
{
  cmdline->priv =
    G_TYPE_INSTANCE_GET_PRIVATE (cmdline,
                                 G_TYPE_APPLICATION_COMMAND_LINE,
                                 GApplicationCommandLinePrivate);
}

static void
g_application_command_line_class_init (GApplicationCommandLineClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->get_property = g_application_command_line_get_property;
  object_class->set_property = g_application_command_line_set_property;
  object_class->finalize = g_application_command_line_finalize;
  class->printerr_literal = g_application_command_line_real_printerr_literal;
  class->print_literal = g_application_command_line_real_print_literal;

  g_object_class_install_property (object_class, PROP_ARGUMENTS,
    g_param_spec_variant ("arguments", "commandline arguments",
                          "the commandline that caused this cmdline",
                          G_VARIANT_TYPE_BYTESTRING_ARRAY, NULL,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_PLATFORM_DATA,
    g_param_spec_variant ("platform-data", "platform data",
                          "platform-specific data for the cmdline",
                          G_VARIANT_TYPE ("a{sv}"), NULL,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_IS_REMOTE,
    g_param_spec_boolean ("is-remote", "is remote",
                          "TRUE if this is a remote cmdline", FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (class, sizeof (GApplicationCommandLinePrivate));
}


/**
 * g_application_command_line_get_arguments:
 * @cmdline: a #GApplicationCommandLine
 * @argc: the length of the arguments array, or %NULL
 *
 * Gets the list of arguments that was passed on the command line.
 *
 * The strings in the array may contain non-utf8 data.
 *
 * The return value is %NULL-terminated and should be freed using
 * g_strfreev().
 *
 * Returns: the string array containing the arguments (the argv)
 *
 * Since: 2.28
 **/
gchar **
g_application_command_line_get_arguments (GApplicationCommandLine   *cmdline,
                                          int                       *argc)
{
  gchar **argv;
  gsize len;

  g_return_val_if_fail (G_IS_APPLICATION_COMMAND_LINE (cmdline), NULL);

  argv = g_variant_dup_bytestring_array (cmdline->priv->arguments, &len);

  if (argc)
    *argc = len;

  return argv;
}

/**
 * g_application_command_line_get_cwd:
 * @cmdline: a #GApplicationCommandLine
 * 
 * Gets the working directory of the command line invocation.  The
 * string may contain non-utf8 data.
 *
 * It is possible that the remote application did not send a working
 * directory, so this may be %NULL.
 *
 * The return value should not be modified or freed and is valid for as
 * long as @cmdline exists.
 *
 * Returns: the current directory, or %NULL
 *
 * Since: 2.28
 **/
const gchar *
g_application_command_line_get_cwd (GApplicationCommandLine *cmdline)
{
  if (cmdline->priv->cwd)
    return g_variant_get_bytestring (cmdline->priv->cwd);
  else
    return NULL;
}

/**
 * g_application_command_line_get_is_remote:
 * @cmdline: a #GApplicationCommandLine
 *
 * Determines if @cmdline represents a remote invocation.
 *
 * Returns: %TRUE if the invocation was remote
 *
 * Since: 2.28
 **/
gboolean
g_application_command_line_get_is_remote (GApplicationCommandLine *cmdline)
{
  return IS_REMOTE (cmdline);
}

/**
 * g_application_command_line_print:
 * @cmdline: a #GApplicationCommandLine
 * @format: a printf-style format string
 * @...: arguments, as per @format
 *
 * Formats a message and prints it using the stdout print handler in the
 * invoking process.
 *
 * If @cmdline is a local invocation then this is exactly equivalent to
 * g_print().  If @cmdline is remote then this is equivalent to calling
 * g_print() in the invoking process.
 *
 * Since: 2.28
 **/
void
g_application_command_line_print (GApplicationCommandLine *cmdline,
                                  const gchar             *format,
                                  ...)
{
  gchar *message;
  va_list ap;

  g_return_if_fail (G_IS_APPLICATION_COMMAND_LINE (cmdline));
  g_return_if_fail (format != NULL);

  va_start (ap, format);
  message = g_strdup_vprintf (format, ap);
  va_end (ap);

  G_APPLICATION_COMMAND_LINE_GET_CLASS (cmdline)
    ->print_literal (cmdline, message);
  g_free (message);
}

/**
 * g_application_command_line_printerr:
 * @cmdline: a #GApplicationCommandLine
 * @format: a printf-style format string
 * @...: arguments, as per @format
 *
 * Formats a message and prints it using the stderr print handler in the
 * invoking process.
 *
 * If @cmdline is a local invocation then this is exactly equivalent to
 * g_printerr().  If @cmdline is remote then this is equivalent to
 * calling g_printerr() in the invoking process.
 *
 * Since: 2.28
 **/
void
g_application_command_line_printerr (GApplicationCommandLine *cmdline,
                                     const gchar             *format,
                                     ...)
{
  gchar *message;
  va_list ap;

  g_return_if_fail (G_IS_APPLICATION_COMMAND_LINE (cmdline));
  g_return_if_fail (format != NULL);

  va_start (ap, format);
  message = g_strdup_vprintf (format, ap);
  va_end (ap);

  G_APPLICATION_COMMAND_LINE_GET_CLASS (cmdline)
    ->printerr_literal (cmdline, message);
  g_free (message);
}

/**
 * g_application_command_line_set_exit_status:
 * @cmdline: a #GApplicationCommandLine
 * @exit_status: the exit status
 *
 * Sets the exit status that will be used when the invoking process
 * exits.
 *
 * The return value of the <varname>command-line</varname> signal is
 * passed to this function when the handler returns.  This is the usual
 * way of setting the exit status.
 *
 * In the event that you want the remote invocation to continue running
 * and want to decide on the exit status in the future, you can use this
 * call.  For the case of a remote invocation, the remote process will
 * typically exit when the last reference is dropped on @cmdline.  The
 * exit status of the remote process will be equal to the last value
 * that was set with this function.
 *
 * In the case that the commandline invocation is local, the situation
 * is slightly more complicated.  If the commandline invocation results
 * in the mainloop running (ie: because the use-count of the application
 * increased to a non-zero value) then the application is considered to
 * have been 'successful' in a certain sense, and the exit status is
 * always zero.  If the application use count is zero, though, the exit
 * status of the local #GApplicationCommandLine is used.
 *
 * Since: 2.28
 **/
void
g_application_command_line_set_exit_status (GApplicationCommandLine *cmdline,
                                            int                      exit_status)
{
  g_return_if_fail (G_IS_APPLICATION_COMMAND_LINE (cmdline));

  cmdline->priv->exit_status = exit_status;
}

/**
 * g_application_command_line_get_exit_status:
 * @cmdline: a #GApplicationCommandLine
 *
 * Gets the exit status of @cmdline.  See
 * g_application_command_line_set_exit_status() for more information.
 *
 * Returns: the exit status
 *
 * Since: 2.28
 **/
int
g_application_command_line_get_exit_status (GApplicationCommandLine *cmdline)
{
  g_return_val_if_fail (G_IS_APPLICATION_COMMAND_LINE (cmdline), -1);

  return cmdline->priv->exit_status;
}

/**
 * g_application_command_line_get_platform_data:
 * @cmdline: #GApplicationCommandLine
 *
 * Gets the platform data associated with the invocation of @cmdline.
 *
 * This is a #GVariant dictionary containing information about the
 * context in which the invocation occured.  It typically contains
 * information like the current working directory and the startup
 * notification ID.
 *
 * For local invocation, it will be %NULL.
 *
 * Returns: the platform data, or %NULL
 *
 * Since: 2.28
 **/
GVariant *
g_application_command_line_get_platform_data (GApplicationCommandLine *cmdline)
{
  g_return_val_if_fail (G_IS_APPLICATION_COMMAND_LINE (cmdline), NULL);

  if (cmdline->priv->platform_data)
    return g_variant_ref (cmdline->priv->platform_data);
  else
      return NULL;
}
