#include "gapplicationcommandline.h"

#include <string.h>
#include <stdio.h>

G_DEFINE_TYPE (GApplicationCommandLine, g_application_command_line, G_TYPE_OBJECT)

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

#define IS_REMOTE(cmdline) ((cmdline)->priv->platform_data != NULL)

void
g_application_command_line_get_argc_argv (GApplicationCommandLine   *cmdline,
                                          int                       *argc,
                                          char                    ***argv)
{
  gsize len;

  g_return_if_fail (G_IS_APPLICATION_COMMAND_LINE (cmdline));
  g_return_if_fail (argc != NULL && argv != NULL);

  *argv = g_variant_dup_bytestring_array (cmdline->priv->arguments, &len);
  *argc = len;
}

GVariant *
g_application_command_line_get_arguments (GApplicationCommandLine *cmdline)
{
  return g_variant_ref (cmdline->priv->arguments);
}

const gchar *
g_application_command_line_get_cwd (GApplicationCommandLine *cmdline)
{
  if (cmdline->priv->cwd)
    return g_variant_get_bytestring (cmdline->priv->cwd);
  else
    return NULL;
}

GVariant *
g_application_command_line_get_cwd_variant (GApplicationCommandLine *cmdline)
{
  if (cmdline->priv->cwd)
    return g_variant_ref (cmdline->priv->cwd);
  else
    return NULL;
}

gboolean
g_application_command_line_get_is_remote (GApplicationCommandLine *cmdline)
{
  return IS_REMOTE (cmdline);
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

void
g_application_command_line_set_exit_status (GApplicationCommandLine *cmdline,
                                            int                      exit_status)
{
  g_return_if_fail (G_IS_APPLICATION_COMMAND_LINE (cmdline));

  cmdline->priv->exit_status = exit_status;
}

int
g_application_command_line_get_exit_status (GApplicationCommandLine *cmdline)
{
  g_return_val_if_fail (G_IS_APPLICATION_COMMAND_LINE (cmdline), -1);

  return cmdline->priv->exit_status;
}

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
g_application_command_line_set_property (GObject *object, guint prop_id,
                                         const GValue *value, GParamSpec *pspec)
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

GVariant *
g_application_command_line_get_platform_data (GApplicationCommandLine *cmdline)
{
  g_return_val_if_fail (G_IS_APPLICATION_COMMAND_LINE (cmdline), NULL);
  g_return_val_if_fail (IS_REMOTE (cmdline), NULL);

  return g_variant_ref (cmdline->priv->platform_data);
}
