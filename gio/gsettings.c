/*
 * Copyright © 2009 Codethink Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of version 3 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * See the included COPYING file for more information.
 */

#include "config.h"
#include <glib.h>
#include <glibintl.h>

#include "gsettings.h"

#include "gdelayedsettingsbackend.h"
#include "gsettingsbackendinternal.h"
#include "gio-marshal.h"
#include "gsettingsschema.h"

#include <string.h>

#include "gioalias.h"

/**
 * SECTION:gsettings
 * @short_description: a high-level API for application settings
 *
 * The #GSettings class provides a convenient API for storing and retrieving
 * application settings.
 */

struct _GSettingsPrivate {
  GSettingsBackend *backend;
  GSettingsSchema *schema;
  gchar *schema_name;
  gchar *context;
  gchar *path;

  guint unapplied_handler;
  gboolean delayed;
};

enum
{
  PROP_0,
  PROP_BACKEND,
  PROP_SCHEMA,
  PROP_CONTEXT,
  PROP_PATH,
  PROP_HAS_UNAPPLIED,
};

enum
{
  SIGNAL_ALL_WRITABLE_CHANGED,
  SIGNAL_ALL_CHANGED,
  SIGNAL_KEYS_CHANGED,
  SIGNAL_WRITABLE_CHANGED,
  SIGNAL_CHANGED,
  SIGNAL_DESTROYED,
  N_SIGNALS
};

static guint g_settings_signals[N_SIGNALS];

G_DEFINE_TYPE (GSettings, g_settings, G_TYPE_OBJECT)

static void
settings_backend_changed (GSettingsBackend    *backend,
                          const gchar         *key,
                          gpointer             origin_tag,
                          gpointer             user_data)
{
  GSettings *settings = G_SETTINGS (user_data);
  gint i;

  g_assert (settings->priv->backend == backend);

  for (i = 0; key[i] == settings->priv->path[i]; i++);

  if (settings->priv->path[i] == '\0' &&
      g_settings_schema_has_key (settings->priv->schema, key + i))
    {
      GQuark quark;

      quark = g_quark_from_string (key + i);
      g_signal_emit (settings, g_settings_signals[SIGNAL_CHANGED],
                     quark, g_quark_to_string (quark));
    }
}

static void
settings_backend_path_changed (GSettingsBackend *backend,
                               const gchar      *path,
                               gpointer          origin_tag,
                               gpointer          user_data)
{
  GSettings *settings = G_SETTINGS (user_data);

  g_assert (settings->priv->backend == backend);

  if (g_str_has_prefix (settings->priv->path, path))
    g_signal_emit (settings, g_settings_signals[SIGNAL_ALL_CHANGED], 0);
}

static void
settings_backend_keys_changed (GSettingsBackend    *backend,
                               const gchar         *path,
                               const gchar * const *items,
                               gpointer             origin_tag,
                               gpointer             user_data)
{
  GSettings *settings = G_SETTINGS (user_data);
  gint i;

  g_assert (settings->priv->backend == backend);

  for (i = 0; settings->priv->path[i] &&
              settings->priv->path[i] == path[i]; i++);

  if (path[i] == '\0')
    {
      GQuark quarks[256];
      gint j, l = 0;

      for (j = 0; items[j]; j++)
         {
           const gchar *item = items[j];
           gint k;

           for (k = 0; item[k] == settings->priv->path[i + k]; k++);

           if (settings->priv->path[i + k] == '\0' &&
               g_settings_schema_has_key (settings->priv->schema, item + k))
             quarks[l++] = g_quark_from_string (item + k);

           /* "256 quarks ought to be enough for anybody!"
            * If this bites you, I'm sorry.  Please file a bug.
            */
           g_assert (l < 256);
         }

      if (l > 0)
        g_signal_emit (settings, g_settings_signals[SIGNAL_KEYS_CHANGED],
                       0, quarks, l);
    }
}

static void
settings_backend_path_writable_changed (GSettingsBackend *backend,
                                        const gchar      *path,
                                        gpointer          user_data)
{
  GSettings *settings = G_SETTINGS (user_data);

  g_assert (settings->priv->backend == backend);

  if (g_str_has_prefix (settings->priv->path, path))
    g_signal_emit (settings,
                   g_settings_signals[SIGNAL_ALL_WRITABLE_CHANGED], 0);
}

static void
settings_backend_writable_changed (GSettingsBackend *backend,
                                   const gchar      *key,
                                   gpointer          user_data)
{
  GSettings *settings = G_SETTINGS (user_data);
  gint i;

  g_assert (settings->priv->backend == backend);

  for (i = 0; key[i] == settings->priv->path[i]; i++);

  if (settings->priv->path[i] == '\0' &&
      g_settings_schema_has_key (settings->priv->schema, key + i))
    {
      GQuark quark;

      quark = g_quark_from_string (key + i);
      g_signal_emit (settings, g_settings_signals[SIGNAL_CHANGED],
                     quark, g_quark_to_string (quark));
    }
}

static void
g_settings_constructed (GObject *object)
{
  GSettings *settings = G_SETTINGS (object);
  const gchar *schema_path;

  settings->priv->schema = g_settings_schema_new (settings->priv->schema_name);
  schema_path = g_settings_schema_get_path (settings->priv->schema);

  if (settings->priv->path && schema_path && strcmp (settings->priv->path, schema_path) != 0)
    g_error ("settings object created with schema '%s' and path '%s', but "
             "path '%s' is specified by schema\n",
             settings->priv->schema_name, settings->priv->path, schema_path);

  if (settings->priv->path == NULL)
    {
      if (schema_path == NULL)
        g_error ("attempting to create schema '%s' without a path\n",
                 settings->priv->schema_name);

      settings->priv->path = g_strdup (schema_path);
    }

  settings->priv->backend = g_settings_backend_get_with_context (settings->priv->context);
  g_settings_backend_watch (settings->priv->backend,
                            settings_backend_changed,
                            settings_backend_path_changed,
                            settings_backend_keys_changed,
                            settings_backend_writable_changed,
                            settings_backend_path_writable_changed,
                            settings);
  g_settings_backend_subscribe (settings->priv->backend,
                                settings->priv->path);
}

static void
g_settings_init (GSettings *settings)
{
  settings->priv = G_TYPE_INSTANCE_GET_PRIVATE (settings,
                                                G_TYPE_SETTINGS,
                                                GSettingsPrivate);
}

static void
g_settings_notify_unapplied (GSettings *settings)
{
  g_object_notify (G_OBJECT (settings), "has-unapplied");
}

/**
 * g_settings_set_delay_apply:
 * @settings: a #GSettings object
 * @delayed: %TRUE to delay applying of changes
 *
 * Changes the #GSettings object into 'delay-apply' mode. In this
 * mode, changes to @settings are not immediately propagated to the
 * backend, but kept locally until g_settings_apply() is called.
 *
 * Since: 2.26
 */
void
g_settings_set_delay_apply (GSettings *settings,
                            gboolean   delayed)
{
  delayed = !!delayed;

  if (delayed != settings->priv->delayed)
    {
      GSettingsBackend *backend;

      g_assert (delayed);

      backend = g_delayed_settings_backend_new (settings->priv->backend);
      g_settings_backend_unwatch (settings->priv->backend, settings);
      g_settings_backend_watch (backend,
                                settings_backend_changed,
                                settings_backend_path_changed,
                                settings_backend_keys_changed,
                                settings_backend_writable_changed,
                                settings_backend_path_writable_changed,
                                settings);
      g_object_unref (settings->priv->backend);

      settings->priv->backend = backend;
      settings->priv->unapplied_handler =
        g_signal_connect_swapped (backend, "notify::has-unapplied",
                                  G_CALLBACK (g_settings_notify_unapplied),
                                  settings);

      settings->priv->delayed = TRUE;
    }
}

/**
 * g_settings_get_delay_apply:
 * @settings: a #GSettings object
 * @returns: %TRUE if changes in @settings are not applied immediately
 *
 * Returns whether the #GSettings object is in 'delay-apply' mode.
 *
 * Since: 2.26
 */
gboolean
g_settings_get_delay_apply (GSettings *settings)
{
  return settings->priv->delayed;
}

/**
 * g_settings_apply:
 * @settings: a #GSettings instance
 *
 * Applies any changes that have been made to the settings.  This
 * function does nothing unless @settings is in 'delay-apply' mode;
 * see g_settings_set_delay_apply().  In the normal case settings are
 * always applied immediately.
 **/
void
g_settings_apply (GSettings *settings)
{
  if (settings->priv->delayed)
    {
      GDelayedSettingsBackend *delayed;

      delayed = G_DELAYED_SETTINGS_BACKEND (settings->priv->backend);
      g_delayed_settings_backend_apply (delayed);
    }
}

/**
 * g_settings_revert:
 * @settings: a #GSettings instance
 *
 * Reverts all non-applied changes to the settings.  This function
 * does nothing unless @settings is in 'delay-apply' mode; see
 * g_settings_set_delay_apply().  In the normal case settings are
 * always applied immediately.
 *
 * Change notifications will be emitted for affected keys.
 **/
void
g_settings_revert (GSettings *settings)
{
  if (settings->priv->delayed)
    {
      GDelayedSettingsBackend *delayed;

      delayed = G_DELAYED_SETTINGS_BACKEND (settings->priv->backend);
      g_delayed_settings_backend_revert (delayed);
    }
}

static void
g_settings_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GSettings *settings = G_SETTINGS (object);

  switch (prop_id)
    {
    case PROP_SCHEMA:
      g_assert (settings->priv->schema_name == NULL);
      settings->priv->schema_name = g_value_dup_string (value);
      break;

    case PROP_PATH:
      settings->priv->path = g_value_dup_string (value);
      break;

    case PROP_CONTEXT:
      settings->priv->context = g_value_dup_string (value);
      break;

    default:
      g_assert_not_reached ();
    }
}

/**
 * g_settings_get_has_unapplied:
 * @settings: a #GSettings object
 * @returns: %TRUE if @settings has unapplied changes
 *
 * Returns whether the #GSettings object has any unapplied
 * changes.  This can only be the case if it is in 'delayed-apply' mode.
 *
 * Since: 2.26
 */
gboolean
g_settings_get_has_unapplied (GSettings *settings)
{
  return settings->priv->delayed &&
         g_delayed_settings_backend_get_has_unapplied (
           G_DELAYED_SETTINGS_BACKEND (settings->priv->backend));
}

static void
g_settings_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  GSettings *settings = G_SETTINGS (object);

  switch (prop_id)
    {
     case PROP_SCHEMA:
      g_value_set_object (value, settings->priv->schema);
      break;

     case PROP_HAS_UNAPPLIED:
      g_value_set_boolean (value, g_settings_get_has_unapplied (settings));
      break;

     default:
      g_assert_not_reached ();
    }
}

static void
g_settings_finalize (GObject *object)
{
  GSettings *settings = G_SETTINGS (object);

  g_settings_backend_unwatch (settings->priv->backend, settings);
  g_settings_backend_unsubscribe (settings->priv->backend,
                                  settings->priv->path);
  g_object_unref (settings->priv->backend);
  g_object_unref (settings->priv->schema);
  g_free (settings->priv->schema_name);
  g_free (settings->priv->path);
}

static void
g_settings_class_init (GSettingsClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
/*
  class->all_writable_changed = g_settings_real_all_writable_changed;
  class->all_changed = g_settings_real_all_changed;
  class->keys_changed = g_settings_real_keys_changed;
  class->writable_changed = g_settings_real_writable_changed;
  class->changed = g_settings_real_changed;
  class->destroyed = g_settings_real_destroyed;
*/
  object_class->set_property = g_settings_set_property;
  object_class->get_property = g_settings_get_property;
  object_class->constructed = g_settings_constructed;
  object_class->finalize = g_settings_finalize;

  g_type_class_add_private (object_class, sizeof (GSettingsPrivate));

  /**
   * GSettings::all-changed:
   * @settings: the object on which the signal was emitted
   *
   * The "all-changed" signal is emitted when potentially every key in
   * the settings object has changed.  This occurs, for example, if
   * g_settings_reset() is called.
   *
   * The default implementation for this signal is to emit the
   * GSettings::changed signal for each key in the schema.
   */
  g_settings_signals[SIGNAL_ALL_CHANGED] =
    g_signal_new ("all-changed", G_TYPE_SETTINGS,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GSettingsClass, all_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  /**
   * GSettings::keys-changed:
   * @settings: the object on which the signal was emitted
   * @keys: an array of #GQuark<!-- -->s for the changed keys
   * @n_keys: the length of the @keys array
   *
   * The "changes" signal is emitting when a number of keys have
   * potentially been changed.  This occurs, for example, if
   * g_settings_apply() is called on a delayed-apply #GSettings.
   *
   * The default implemenetation for this signal is to emit the
   * GSettings::changed signal for each item in the list of keys.
   */
  g_settings_signals[SIGNAL_KEYS_CHANGED] =
    g_signal_new ("keys-changed", G_TYPE_SETTINGS,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GSettingsClass, keys_changed),
                  NULL, NULL,
                  _gio_marshal_VOID__POINTER_INT,
                  G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_INT);

  /**
   * GSettings::changed:
   * @settings: the object on which the signal was emitted
   * @key: the changed key
   *
   * The "changed" signal is emitted when a key has potentially been
   * changed.
   */
  g_settings_signals[SIGNAL_CHANGED] =
    g_signal_new ("changed", G_TYPE_SETTINGS,
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  G_STRUCT_OFFSET (GSettingsClass, changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1, G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GSettings::all-writable-changed:
   *
   * The "all-writable-changes" signal is emitted when the writability
   * of potentially every key has changed.  This occurs, for example, if
   * an entire subpath is locked down in the settings backend.
   *
   * The default implementation for this signal is to emit the
   * GSettings:writable-changed signal for each key in the schema.
   */
  g_settings_signals[SIGNAL_ALL_WRITABLE_CHANGED] =
    g_signal_new ("all-writable-changed", G_TYPE_SETTINGS,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GSettingsClass, all_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
  /**
   * GSettings::writable-changed:
   * @settings: the object on which the signal was emitted
   * @key: the key
   *
   * The "writable-changed" signal is emitted when the writability of a
   * key has potentially changed.  You should call
   * g_settings_is_writable() in order to determine the new status.
   */
  g_settings_signals[SIGNAL_WRITABLE_CHANGED] =
    g_signal_new ("writable-changed", G_TYPE_SETTINGS,
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  G_STRUCT_OFFSET (GSettingsClass, changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1, G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GSettings:schema-name:
   *
   * The name of the schema that describes the types of keys
   * for this #GSettings object.
   */
  g_object_class_install_property (object_class, PROP_SCHEMA,
    g_param_spec_string ("schema",
                         P_("Schema name"),
                         P_("The name of the schema for this settings object"),
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

   /**
    * GSettings:base-path:
    *
    * The path within the backend where the settings are stored.
    */
   g_object_class_install_property (object_class, PROP_PATH,
     g_param_spec_string ("path",
                          P_("Base path"),
                          P_("The path within the backend where the settings are"),
                          NULL,
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

   /**
    * GSettings:has-unapplied:
    *
    * If this property is %TRUE, the #GSettings object has outstanding
    * changes that will be applied when g_settings_apply() is called.
    */
   g_object_class_install_property (object_class, PROP_HAS_UNAPPLIED,
     g_param_spec_boolean ("has-unapplied",
                           P_("Has unapplied changes"),
                           P_("TRUE if there are outstanding changes to apply()"),
                           FALSE,
                           G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

}

/**
 * g_settings_get_value:
 * @settings: a #GSettings object
 * @key: the key to get the value for
 * @returns: a new #GVariant
 *
 * Gets the value that is stored in @settings for @key.
 *
 * It is a programmer error to give a @key that isn't valid for
 * @settings.
 *
 * Since: 2.26
 */
GVariant *
g_settings_get_value (GSettings   *settings,
                      const gchar *key)
{
  const GVariantType *type;
  GVariant *value;
  GVariant *sval;
  gchar *path;

  path = g_strconcat (settings->priv->path, key, NULL);
  sval = g_settings_schema_get_value (settings->priv->schema, key, NULL);
  type = g_variant_get_type (sval);
  value = g_settings_backend_read (settings->priv->backend, path, type);
  g_free (path);

  if (value && !g_variant_is_of_type (value, type))
    {
      g_variant_unref (value);
      value = NULL;
    }

  if (value == NULL)
    value = g_variant_ref (sval);

  g_variant_unref (sval);

  return value;
}

/**
 * g_settings_set_value:
 * @settings: a #GSettings object
 * @key: the name of the key to set
 * @value: a #GVariant of the correct type
 *
 * Sets @key in @settings to @value.
 *
 * It is a programmer error to give a @key that isn't valid for
 * @settings.  It is a programmer error to give a @value of the
 * incorrect type.
 *
 * If @value is floating then this function consumes the reference.
 *
 * Since: 2.26
 **/
void
g_settings_set_value (GSettings   *settings,
                      const gchar *key,
                      GVariant    *value)
{
  gboolean correct_type;
  GVariant *sval;

  sval = g_settings_schema_get_value (settings->priv->schema, key, NULL);
  correct_type = g_variant_is_of_type (value, g_variant_get_type (sval));
  g_variant_unref (sval);

  g_return_if_fail (correct_type);

  g_settings_backend_write (settings->priv->backend, key, value, NULL);
}

/**
 * g_settings_get:
 * @settings: a #GSettings object
 * @key: the key to get the value for
 * @format_string: a #GVariant format string
 * @...: arguments as per @format
 *
 * Gets the value that is stored at @key in @settings.
 *
 * A convenience function that combines g_settings_get_value() with
 * g_variant_get().
 *
 * It is a programmer error to pass a @key that isn't valid for
 * @settings or a @format_string that doesn't match the type of @key according
 * to the schema of @settings.
 *
 * Since: 2.26
 */
void
g_settings_get (GSettings   *settings,
                const gchar *key,
                const gchar *format_string,
                ...)
{
  GVariant *value;
  va_list ap;

  value = g_settings_get_value (settings, key);

  va_start (ap, format_string);
  g_variant_get_va (value, format_string, NULL, &ap);
  va_end (ap);

  g_variant_unref (value);
}

/**
 * g_settings_set:
 * @settings: a #GSettings object
 * @key: the name of the key to set
 * @format: a #GVariant format string
 * @...: arguments as per @format
 *
 * Sets @key in @settings to @value.
 *
 * A convenience function that combines g_settings_set_value() with
 * g_variant_new().
 *
 * It is a programmer error to pass a @key that isn't valid for
 * @settings or a @format that doesn't match the type of @key according
 * to the schema of @settings.
 *
 * Since: 2.26
 */
void
g_settings_set (GSettings   *settings,
                const gchar *key,
                const gchar *format,
                ...)
{
  GVariant *value;
  va_list ap;

  va_start (ap, format);
  value = g_variant_new_va (format, NULL, &ap);
  va_end (ap);

  g_settings_set_value (settings, key, value);
}

/**
 * g_settings_is_writable:
 * @settings: a #GSettings object
 * @name: the name of a key
 * @returns: %TRUE if the key @name is writable
 *
 * Finds out if a key can be written or not
 *
 * Since: 2.26
 */
gboolean
g_settings_is_writable (GSettings   *settings,
                        const gchar *name)
{
  gboolean writable;
  gchar *path;

  path = g_strconcat (settings->priv->path, name, NULL);
  writable = g_settings_backend_get_writable (settings->priv->backend, path);
  g_free (path);

  return writable;
}

/**
 * g_settings_new:
 * @schema: the name of the schema
 * @returns: a new #GSettings object
 *
 * Creates a new #GSettings object with a given schema.
 *
 * Since: 2.26
 */
GSettings *
g_settings_new (const gchar *schema)
{
  return g_object_new (G_TYPE_SETTINGS,
                       "schema", schema,
                       NULL);
}

/**
 * g_settings_new_with_path:
 * @schema: the name of the schema
 * @path: the path to use
 * @returns: a new #GSettings object
 *
 * Creates a new #GSettings object with a given schema and path.
 *
 * You only need to do this if you want to directly create a settings
 * object with a schema that doesn't have a specified path of its own.
 * That's quite rare.
 *
 * It is a programmer error to call this function for a schema that
 * has an explicitly specified path.
 *
 * Since: 2.26
 */
GSettings *
g_settings_new_with_path (const gchar *schema,
                          const gchar *path)
{
  return g_object_new (G_TYPE_SETTINGS,
                       "schema", schema,
                       "path", path,
                       NULL);
}

/**
 * g_settings_new_with_context:
 * @schema: the name of the schema
 * @context: the context to use
 * @returns: a new #GSettings object
 *
 * Creates a new #GSettings object with a given schema and context.
 *
 * Creating settings objects with a context allow accessing settings
 * from a database other than the usual one.  For example, it may make
 * sense to specify "defaults" in order to get a settings object that
 * modifies the system default settings instead of the settings for this
 * user.
 *
 * It is a programmer error to call this function for an unsupported
 * context.  Use g_settings_supports_context() to determine if a context
 * is supported if you are unsure.
 *
 * Since: 2.26
 */
GSettings *
g_settings_new_with_context (const gchar *schema,
                             const gchar *context)
{
  return g_object_new (G_TYPE_SETTINGS,
                       "schema", schema,
                       "context", context,
                       NULL);
}

/**
 * g_settings_new_with_context_and_path:
 * @schema: the name of the schema
 * @path: the path to use
 * @returns: a new #GSettings object
 *
 * Creates a new #GSettings object with a given schema, context and
 * path.
 *
 * This is a mix of g_settings_new_with_context() and
 * g_settings_new_with_path().
 *
 * Since: 2.26
 */
GSettings *
g_settings_new_with_context_and_path (const gchar *schema,
                                      const gchar *context,
                                      const gchar *path)
{
  return g_object_new (G_TYPE_SETTINGS,
                       "schema", schema,
                        "context", context,
                        "path", path,
                        NULL);
}
