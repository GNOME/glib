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
  gchar *base_path;

  GSettingsSchema *schema;
  gchar *schema_name;

  guint handler_id;
  guint unapplied_handler;
  gboolean delayed;
};

enum
{
  PROP_0,
  PROP_STORAGE,
  PROP_SCHEMA_NAME,
  PROP_SCHEMA,
  PROP_BASE_PATH,
  PROP_DELAY_APPLY,
  PROP_HAS_UNAPPLIED,
};

enum
{
  SIGNAL_CHANGES,
  SIGNAL_CHANGED,
  SIGNAL_DESTROYED,
  N_SIGNALS
};

static guint g_settings_signals[N_SIGNALS];

G_DEFINE_TYPE (GSettings, g_settings, G_TYPE_OBJECT)

static void
g_settings_storage_changed (GSettingsBackend    *backend,
                            const gchar         *prefix,
                            gchar const * const *names,
                            gint                 n_names,
                            gpointer             origin_tag,
                            gpointer             user_data)
{
  GSettings *settings = G_SETTINGS (user_data);
  gchar **changes;
  GQuark *quarks;
  gint i, c;

  g_assert (settings->priv->backend == backend);

  /* slow and stupid. */
  changes = g_new (gchar *, n_names);
  quarks = g_new (GQuark, n_names);
  for (i = 0; i < n_names; i++)
    changes[i] = g_strconcat (prefix, names[i], NULL);

  /* check which are for us */
  c = 0;
  for (i = 0; i < n_names; i++)
    if (g_str_has_prefix (changes[i], settings->priv->base_path))
      {
        const gchar *rel = changes[i] + strlen (settings->priv->base_path);

        if (strchr (rel, '/') == NULL)
          /* XXX also check schema to ensure it's really a key */
          quarks[c++] = g_quark_from_string (rel);
      }

  g_settings_changes (settings, quarks, c);

  for (i = 0; i < n_names; i++)
    g_free (changes[i]);
  g_free (changes);
  g_free (quarks);
}

/**
 * g_settings_changes:
 * @settings: a #GSettings object
 * @keys: an array of #GQuark key names
 * @n_keys: the length of @keys
 *
 * Emits the #GSettings::changes signal on a #GSettings object.
 *
 * It is an error to call this function with a quark in @keys that is
 * not a valid key for @settings (according to its schema).
 *
 * Since: 2.26
 */
void
g_settings_changes (GSettings    *settings,
                    const GQuark *keys,
                    gint          n_keys)
{
  g_return_if_fail (settings != NULL);

  if (n_keys == 0)
    return;

  g_return_if_fail (keys != NULL);
  g_return_if_fail (n_keys > 0);

  g_signal_emit (settings,
                 g_settings_signals[SIGNAL_CHANGES], 0,
                 keys, n_keys);
}

static void
g_settings_real_changes (GSettings    *settings,
                         const GQuark *keys,
                         gint          n_keys)
{
  gint i;

  for (i = 0; i < n_keys; i++)
    g_signal_emit (settings,
                   g_settings_signals[SIGNAL_CHANGED],
                   keys[i], g_quark_to_string (keys[i]));
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

      backend = g_delayed_settings_backend_new (settings->priv->backend,
                                                settings->priv->base_path);
      g_settings_backend_subscribe (backend, "");
      g_settings_backend_unsubscribe (settings->priv->backend,
                                      settings->priv->base_path);
      g_signal_handler_disconnect (settings->priv->backend,
                                   settings->priv->handler_id);
      g_object_unref (settings->priv->backend);

      settings->priv->backend = backend;
      settings->priv->unapplied_handler =
        g_signal_connect_swapped (backend, "notify::has-unapplied",
                                  G_CALLBACK (g_settings_notify_unapplied),
                                  settings);
      settings->priv->handler_id =
        g_signal_connect (settings->priv->backend, "changed",
                          G_CALLBACK (g_settings_storage_changed),
                          settings);

      g_free (settings->priv->base_path);
      settings->priv->base_path = g_strdup ("");

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
     case PROP_SCHEMA_NAME:
      g_assert (settings->priv->schema_name == NULL);
      settings->priv->schema_name = g_value_dup_string (value);
      break;

     case PROP_SCHEMA:
      g_assert (settings->priv->schema == NULL);
      settings->priv->schema = g_value_dup_object (value);
      break;

     case PROP_DELAY_APPLY:
      g_settings_set_delay_apply (settings, g_value_get_boolean (value));
      break;

     case PROP_BASE_PATH:
      settings->priv->base_path = g_value_dup_string (value);
      break;

     case PROP_STORAGE:
      settings->priv->backend = g_value_dup_object (value);
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

  g_signal_handler_disconnect (settings->priv->backend, settings->priv->handler_id);
  g_object_unref (settings->priv->backend);
  g_object_unref (settings->priv->schema);
  g_free (settings->priv->schema_name);
  g_free (settings->priv->base_path);
}

static void
g_settings_constructed (GObject *object)
{
  GSettings *settings = G_SETTINGS (object);

  if (settings->priv->backend == NULL)
    settings->priv->backend = g_settings_backend_get_with_context (NULL);

  settings->priv->handler_id =
    g_signal_connect (settings->priv->backend, "changed",
                      G_CALLBACK (g_settings_storage_changed),
                      settings);

  if (settings->priv->schema != NULL && settings->priv->schema_name != NULL)
    g_error ("Schema and schema name specified");

  if (settings->priv->schema == NULL && settings->priv->schema_name != NULL)
    settings->priv->schema = g_settings_schema_new (settings->priv->schema_name);

  if (settings->priv->schema == NULL)
    settings->priv->schema = g_settings_schema_new ("empty");

  if (settings->priv->base_path == NULL && settings->priv->schema == NULL)
    g_error ("Attempting to make settings with no schema or path");

  if (settings->priv->base_path != NULL && settings->priv->schema != NULL)
    {
      const gchar *schema_path, *given_path;

      schema_path = g_settings_schema_get_path (settings->priv->schema);
      given_path = settings->priv->base_path;

      if (schema_path && strcmp (schema_path, given_path) != 0)
        g_error ("Specified path of '%s' but schema says '%s'",
                 given_path, schema_path);
     }

  if (settings->priv->base_path == NULL)
    settings->priv->base_path = g_strdup (
      g_settings_schema_get_path (settings->priv->schema));

  if (settings->priv->base_path == NULL)
    g_error ("No base path given and none from schema");

  g_settings_backend_subscribe (settings->priv->backend,
                                settings->priv->base_path);
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
  return g_object_new (G_TYPE_SETTINGS, "schema-name", schema, NULL);
}

/**
 * g_settings_new_with_path:
 * @schema: the name of the schema
 * @path: the path to use
 * @returns: a new #GSettings object
 *
 * Creates a new #GSettings object with a given schema and path.
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
  return g_object_new (G_TYPE_SETTINGS, "schema-name", schema, "base-path", path, NULL);
}

#if 0
static GType
g_settings_get_gtype_for_schema (GSettingsSchema *schema)
{
  GVariantIter iter;
  const gchar *item;
  GVariant *list;

  list = g_settings_schema_get_inheritance (schema);
  g_variant_iter_init (&iter, list);
  g_variant_unref (list);

  while (g_variant_iter_next (&iter, "s", &item))
    if (strcmp (item, "list") == 0)
      return G_TYPE_SETTINGS_LIST;

  return G_TYPE_SETTINGS;
}

GSettings *
g_settings_get_settings (GSettings   *settings,
                         const gchar *name)
{
  const gchar *schema_name;
  GSettingsSchema *schema;
  GSettings *child;
  gchar *path;

  schema_name = g_settings_schema_get_schema (settings->priv->schema, name);

  if (schema_name == NULL)
    {
      schema_name = g_settings_schema_get_schema (settings->priv->schema,
                                                  "/default");

      if G_UNLIKELY (schema_name == NULL)
        g_error ("Schema has no child schema named '%s' and no "
                 "default child schema", name);
    }

  schema = g_settings_schema_new (schema_name);
  path = g_strdup_printf ("%s%s/", settings->priv->base_path, name);

  child = g_object_new (g_settings_get_gtype_for_schema (schema),
                        "schema", schema,
                        "backend", settings->priv->backend,
                        "base-path", path,
                        NULL);

  g_free (path);

  return child;
}
#endif

static void
g_settings_class_init (GSettingsClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  class->changes = g_settings_real_changes;

  object_class->set_property = g_settings_set_property;
  object_class->get_property = g_settings_get_property;
  object_class->constructed = g_settings_constructed;
  object_class->finalize = g_settings_finalize;

  g_type_class_add_private (object_class, sizeof (GSettingsPrivate));

  /**
   * GSettings::changes:
   * @settings: the object on which the signal was emitted
   * @keys: an array of #GQuark<!-- -->s for the changed keys
   * @n_keys: the length of the @keys array
   *
   * The "changes" signal is emitted when a set of keys changes.
   */
  g_settings_signals[SIGNAL_CHANGES] =
    g_signal_new ("changes", G_TYPE_SETTINGS,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GSettingsClass, changes),
                  NULL, NULL,
                  _gio_marshal_VOID__POINTER_INT,
                  G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_INT);

  /**
   * GSettings::chnaged:
   * @settings: the object on which the signal was emitted
   * @key:  the changed key
   *
   * The "changed" signal is emitted for each changed key.
   */
  g_settings_signals[SIGNAL_CHANGED] =
    g_signal_new ("changed", G_TYPE_SETTINGS,
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  G_STRUCT_OFFSET (GSettingsClass, changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1, G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GSettings::destroyed:
   * @settings: the object on which this signal was emitted
   *
   * The "destroyed" signal is emitted when the root of the settings
   * is removed from the backend storage.
   */
  g_settings_signals[SIGNAL_DESTROYED] =
    g_signal_new ("destroyed", G_TYPE_SETTINGS,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GSettingsClass, destroyed),
                  NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  /**
   * GSettings:backend:
   *
   * The #GSettingsBackend object that provides the backing storage
   * for this #GSettings object.
   */
  g_object_class_install_property (object_class, PROP_STORAGE,
    g_param_spec_object ("backend",
                         P_("Backend storage"),
                         P_("The GSettingsBackend object for this settings object"),
                         G_TYPE_SETTINGS_BACKEND,
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GSettings:schema-name:
   *
   * The name of the schema that describes the types of keys
   * for this #GSettings object.
   */
  g_object_class_install_property (object_class, PROP_SCHEMA_NAME,
    g_param_spec_string ("schema-name",
                         P_("Schema name"),
                         P_("The name of the schema for this settings object"),
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

   /**
    * GSettings:schema:
    *
    * The #GSettingsSchema object that describes the types of
    * keys for this #GSettings object.
    */
   g_object_class_install_property (object_class, PROP_SCHEMA,
     g_param_spec_object ("schema",
                          P_("Schema"),
                          P_("The GSettingsSchema object for this settings object"),
                          G_TYPE_OBJECT,
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

   /**
    * GSettings:base-path:
    *
    * The path within the backend where the settings are stored.
    */
   g_object_class_install_property (object_class, PROP_BASE_PATH,
     g_param_spec_string ("base-path",
                          P_("Base path"),
                          P_("The path within the backend where the settings are"),
                          NULL,
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

   /**
    * GSettings:delay-apply:
    *
    * If this property is %TRUE, the #GSettings object is in 'delay-apply'
    * mode and will not apply changes until g_settings_apply() is called.
    */
   g_object_class_install_property (object_class, PROP_DELAY_APPLY,
     g_param_spec_boolean ("delay-apply",
                           P_("Delayed apply"),
                           P_("If TRUE, you must call apply() to write changes"),
                           FALSE,
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

  path = g_strconcat (settings->priv->base_path, key, NULL);
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
  GTree *tree;

  sval = g_settings_schema_get_value (settings->priv->schema, key, NULL);
  correct_type = g_variant_is_of_type (value, g_variant_get_type (sval));
  g_variant_unref (sval);

  g_return_if_fail (correct_type);

  tree = g_settings_backend_create_tree ();
  g_tree_insert (tree, strdup (key), g_variant_ref_sink (value));
  g_settings_backend_write (settings->priv->backend, key, tree, NULL);
  g_tree_unref (tree);
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

  path = g_strconcat (settings->priv->base_path, name, NULL);
  writable = g_settings_backend_get_writable (settings->priv->backend, path);
  g_free (path);

  return writable;
}

void
g_settings_destroy (GSettings *settings)
{
  g_signal_emit (settings, g_settings_signals[SIGNAL_DESTROYED], 0);
}

#if 0
typedef struct
{
  GSettings *settings;
  GObject *object;

  guint property_handler_id;
  const GParamSpec *property;
  guint key_handler_id;
  const GVariantType *type;
  const gchar *key;

  /* prevent recursion */
  gboolean running;
} GSettingsBinding;

static void
g_settings_binding_free (gpointer data)
{
  GSettingsBinding *binding = data;

  g_assert (!binding->running);

  if (binding->key_handler_id)
    g_signal_handler_disconnect (binding->settings,
                                 binding->key_handler_id);

  if (binding->property_handler_id)
  g_signal_handler_disconnect (binding->object,
                               binding->property_handler_id);

  g_object_unref (binding->settings);

  g_slice_free (GSettingsBinding, binding);
}

static GQuark
g_settings_binding_quark (const char *property)
{
  GQuark quark;
  gchar *tmp;

  tmp = g_strdup_printf ("gsettingsbinding-%s", property);
  quark = g_quark_from_string (tmp);
  g_free (tmp);

  return quark;
}

static void
g_settings_binding_key_changed (GSettings   *settings,
                                const gchar *key,
                                gpointer     user_data)
{
  GSettingsBinding *binding = user_data;
  GValue value = {  };
  GVariant *variant;

  g_assert (settings == binding->settings);
  g_assert (key == binding->key);

  if (binding->running)
    return;

  binding->running = TRUE;

  g_value_init (&value, binding->property->value_type);
  variant = g_settings_get_value (settings, key);
  if (g_value_deserialise (&value, variant))
    g_object_set_property (binding->object,
                           binding->property->name,
                           &value);
  g_value_unset (&value);

  binding->running = FALSE;
}

static void
g_settings_binding_property_changed (GObject          *object,
                                     const GParamSpec *pspec,
                                     gpointer          user_data)
{
  GSettingsBinding *binding = user_data;
  GValue value = {  };
  GVariant *variant;

  g_assert (object == binding->object);
  g_assert (pspec == binding->property);

  if (binding->running)
    return;

  binding->running = TRUE;

  g_value_init (&value, pspec->value_type);
  g_object_get_property (object, pspec->name, &value);
  if ((variant = g_value_serialise (&value, binding->type)))
    {
      g_settings_set_value (binding->settings,
                            binding->key,
                            variant);
      g_variant_unref (variant);
    }
  g_value_unset (&value);

  binding->running = FALSE;
}

void
g_settings_bind (GSettings          *settings,
                 const gchar        *key,
                 gpointer            object,
                 const gchar        *property,
                 GSettingsBindFlags  flags)
{
  GSettingsBinding *binding;
  GObjectClass *objectclass;
  gchar *detailed_signal;
  GQuark binding_quark;
  gboolean insensitive;

  objectclass = G_OBJECT_GET_CLASS (object);

  binding = g_slice_new (GSettingsBinding);
  binding->settings = g_object_ref (settings);
  binding->object = object;
  binding->key = g_intern_string (key);
  binding->property = g_object_class_find_property (objectclass, property);
  binding->running = FALSE;

  if (!(flags & (G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET)))
    flags |= G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET;

  if (binding->property == NULL)
    {
      g_critical ("g_settings_bind: no property '%s' on class '%s'",
                  property, G_OBJECT_TYPE_NAME (object));
      return;
    }

  binding->type = g_settings_schema_get_key_type (settings->priv->schema,
                                                  key);

  if (binding->type == NULL)
    {
      g_critical ("g_settings_bind: no key '%s' on schema '%s'",
                  key, settings->priv->schema_name);
      return;
    }

  if (!g_type_serialiser_check (G_PARAM_SPEC_VALUE_TYPE (binding->property),
                                binding->type))
    {
      g_critical ("g_settings_bind: property '%s' on class '%s' has type"
                  "'%s' which is not compatible with type '%s' of key '%s'"
                  "on schema '%s'", property, G_OBJECT_TYPE_NAME (object),
                  g_type_name (binding->property->value_type),
                  g_variant_type_dup_string (binding->type), key,
                  settings->priv->schema_name);
      return;
    }

  if (~flags & G_SETTINGS_BIND_NO_SENSITIVITY)
    {
      GParamSpec *sensitive;

      sensitive = g_object_class_find_property (objectclass, "sensitive");
      if (sensitive && sensitive->value_type == G_TYPE_BOOLEAN)
        {
          insensitive = !g_settings_is_writable (settings, key);
          g_object_set (object, "sensitive", !insensitive, NULL);
        }
      else
        insensitive = FALSE;
    }
  else
    insensitive = FALSE;

  if (!insensitive && (flags & G_SETTINGS_BIND_SET))
    {
      detailed_signal = g_strdup_printf ("notify::%s", property);
      binding->property_handler_id =
        g_signal_connect (object, detailed_signal,
                          G_CALLBACK (g_settings_binding_property_changed),
                          binding);
      g_free (detailed_signal);

      if (~flags & G_SETTINGS_BIND_GET)
        g_settings_binding_property_changed (object,
                                             binding->property,
                                             binding);
    }

  if (flags & G_SETTINGS_BIND_GET)
    {
      detailed_signal = g_strdup_printf ("changed::%s", key);
      binding->key_handler_id =
        g_signal_connect (settings, detailed_signal,
                          G_CALLBACK (g_settings_binding_key_changed),
                          binding);
      g_free (detailed_signal);

      g_settings_binding_key_changed (settings, binding->key, binding);
    }

  binding_quark = g_settings_binding_quark (property);
  g_object_set_qdata_full (object, binding_quark,
                           binding, g_settings_binding_free);
}

void
g_settings_unbind (gpointer     object,
                   const gchar *property)
{
  GQuark binding_quark;

  binding_quark = g_settings_binding_quark (property);
  g_object_set_qdata (object, binding_quark, NULL);
}
#endif
