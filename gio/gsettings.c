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
#include <locale.h>

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
 *
 * When creating a GSettings instance, you have to specify a schema
 * that describes the keys in your settings and their types and default
 * values, as well as some other information.
 *
 * Normally, a schema has as fixed path that determines where the settings
 * are stored in the conceptual global tree of settings. However, schemas
 * can also be 'relocatable', i.e. not equipped with a fixed path. This is
 * useful e.g. when the schema describes an 'account', and you want to be
 * able to store a arbitrary number of accounts.
 *
 * Unlike other configuration systems (like GConf), GSettings does not
 * restrict keys to basic types like strings and numbers. GSettings stores
 * values as #GVariant, and allows any #GVariantType for keys. Key names
 * are restricted to lowercase characters, numbers and '-'. Furthermore,
 * the names must begin with a lowercase character, must not end
 * with a '-', and must not contain consecutive dashes. Key names can
 * be up to 32 characters long.
 *
 * Similar to GConf, the default values in GSettings schemas can be
 * localized, but the localized values are stored in gettext catalogs
 * and looked up with the domain that is specified in the gettext-domain
 * attribute of the <tag>schemalist</tag> or <tag>schema</tag> elements
 * and the category that is specified in the l10n attribute of the
 * <tag>key</tag> element.
 *
 * GSettings uses schemas in a compact binary form that is created
 * by the gschema-compile utility. The input is a schema description in
 * an XML format that can be described by the following DTD:
 * |[<![CDATA[
 * <!ELEMENT schemalist (schema*) >
 * <!ATTLIST schemalist gettext-domain #IMPLIED >
 *
 * <!ELEMENT schema (key|child)* >
 * <!ATTLIST schema id             CDATA #REQUIRED
 *                  path           CDATA #IMPLIED
 *                  gettext-domain CDATA #IMPLIED >
 *
 * <!ELEMENT key (default|summary?|description?|range?|choices?) >
 * <!-- name can only contain lowercase letters, numbers and '-' -->
 * <!-- type must be a GVariant type string -->
 * <!ATTLIST key name CDATA #REQUIRED
 *               type CDATA #REQUIRED >
 *
 * <!-- the default value is specified a a serialized GVariant,
 *      i.e. you have to include the quotes when specifying a string -->
 * <!ELEMENT default (#PCDATA) >
 * <!-- the presence of the l10n attribute marks a default value for
 *      translation, its value is the gettext category to use -->
 * <!ATTLIST default l10n (messages|time) #IMPLIED >
 *
 * <!ELEMENT summary (#PCDATA) >
 * <!ELEMENT description (#PCDATA) >
 *
 * <!ELEMENT range (min,max)  >
 * <!ELEMENT min (#PCDATA) >
 * <!ELEMENT max (#PCDATA) >
 *
 * <!ELEMENT choices (choice+) >
 * <!ELEMENT choice (alias?) >
 * <!ATTLIST choice value CDATA #REQUIRED >
 * <!ELEMENT choice (alias?) >
 * <!ELEMENT alias EMPTY >
 * <!ATTLIST alias value CDATA #REQUIRED >
 *
 * <!ELEMENT child EMPTY >
 * <!ATTLIST child name  CDATA #REQUIRED
 *                 schema CDATA #REQUIRED >
 * ]]>
 * ]|
 */

/* TODO:
 * Talk about schema naming conventions, explain child settings
 */

struct _GSettingsPrivate
{
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
      const gchar *string;
      GQuark quark;

      string = g_quark_to_string (quark);

      quark = g_quark_from_string (key + i);
      g_signal_emit (settings, g_settings_signals[SIGNAL_WRITABLE_CHANGED],
                     quark, string);

      /* if 'writeable' changes the key value may change as a
       * side-effect (consider the admin setting a mandatory key over
       * top of an existing key).  so, signal that possibility.
       */
      g_signal_emit (settings, g_settings_signals[SIGNAL_CHANGED],
                     quark, string);
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
    {
      g_signal_emit (settings,
                     g_settings_signals[SIGNAL_ALL_WRITABLE_CHANGED], 0);
      g_signal_emit (settings, g_settings_signals[SIGNAL_ALL_CHANGED], 0);
    }
}


static void
real_all_writable_changed (GSettings *settings)
{
  const GQuark *list;
  gint n_items;
  gint i;

  list = g_settings_schema_list (settings->priv->schema, &n_items);

  for (i = 0; i < n_items; i++)
    g_signal_emit (settings, g_settings_signals[SIGNAL_WRITABLE_CHANGED],
                   list[i], g_quark_to_string (list[i]));
}

static void
real_all_changed (GSettings *settings)
{
  const GQuark *list;
  gint n_items;
  gint i;

  list = g_settings_schema_list (settings->priv->schema, &n_items);

  for (i = 0; i < n_items; i++)
    g_signal_emit (settings, g_settings_signals[SIGNAL_CHANGED],
                   list[i], g_quark_to_string (list[i]));
}

static void
real_keys_changed (GSettings    *settings,
                   const GQuark *items,
                   gint          n_items)
{
  gint i;

  for (i = 0; i < n_items; i++)
    g_signal_emit (settings, g_settings_signals[SIGNAL_CHANGED],
                   items[i], g_quark_to_string (items[i]));
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
g_settings_delay (GSettings *settings)
{
  if (settings->priv->delayed)
    return;

  settings->priv->delayed =
    g_delayed_settings_backend_new (settings->priv->backend, settings);
  g_settings_backend_unwatch (settings->priv->backend, settings);
  g_object_unref (settings->priv->backend);

  settings->priv->backend = G_SETTINGS_BACKEND (settings->priv->delayed);
  g_settings_backend_watch (settings->priv->backend,
                            settings_backend_changed,
                            settings_backend_path_changed,
                            settings_backend_keys_changed,
                            settings_backend_writable_changed,
                            settings_backend_path_writable_changed,
                            settings);
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
  class->all_writable_changed = real_all_writable_changed;
  class->all_changed = real_all_changed;
  class->keys_changed = real_keys_changed;

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
   * #GSettings::changed signal for each key in the schema.
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
   * The "keys-changed" signal is emitting when a number of keys have
   * potentially been changed.  This occurs, for example, if
   * g_settings_apply() is called on a delayed-apply #GSettings.
   *
   * The default implementation for this signal is to emit the
   * #GSettings::changed signal for each item in the list of keys.
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
   * The "all-writable-changed" signal is emitted when the writability
   * of potentially every key has changed.  This occurs, for example, if
   * an entire subpath is locked down in the settings backend.
   *
   * The default implementation for this signal is to emit the
   * #GSettings:writable-changed signal for each key in the schema.
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
   * GSettings:context:
   *
   * The name of the context that the settings are stored in.
   */
  g_object_class_install_property (object_class, PROP_CONTEXT,
    g_param_spec_string ("context",
                         P_("Context name"),
                         P_("The name of the context for this settings object"),
                         "", G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GSettings:schema:
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
    * GSettings:path:
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
  const gchar *unparsed = NULL;
  GVariant *value, *options;
  const GVariantType *type;
  gchar lc_char = '\0';
  GVariant *sval;
  gchar *path;

  sval = g_settings_schema_get_value (settings->priv->schema, key, &options);

  if G_UNLIKELY (sval == NULL)
    g_error ("schema '%s' does not contain a key named '%s'\n",
             settings->priv->schema_name, key);

  path = g_strconcat (settings->priv->path, key, NULL);
  type = g_variant_get_type (sval);
  value = g_settings_backend_read (settings->priv->backend, path, type);
  g_free (path);

  if (options != NULL)
    {
      GVariantIter iter;
      const gchar *key;
      GVariant *value;

      g_variant_iter_init (&iter, options);
      while (g_variant_iter_loop (&iter, "{&sv}", &key, &value))
        {
          if (strcmp (key, "l10n") == 0)
            g_variant_get (value, "(y&s)", &lc_char, &unparsed);
          else
            g_warning ("unknown schema extension '%s'", key);
        }
    }

  if (value && !g_variant_is_of_type (value, type))
    {
      g_variant_unref (value);
      value = NULL;
    }

  if (value == NULL && lc_char != '\0')
  /* we will need to translate the schema default value */
    {
      const gchar *translated;
      GError *error = NULL;
      const gchar *domain;
      gint lc_category;

      domain = g_settings_schema_get_gettext_domain (settings->priv->schema);

      if (lc_char == 't')
        lc_category = LC_TIME;
      else
        lc_category = LC_MESSAGES;

      translated = dcgettext (domain, unparsed, lc_category);

      if (translated != unparsed)
        /* it was translated, so we need to re-parse it */
        {
          value = g_variant_parse (g_variant_get_type (sval),
                                   translated, NULL, NULL, &error);

          if (value == NULL)
            {
              g_warning ("Failed to parse translated string `%s' for "
                         "key `%s' in schema `%s': %s", unparsed, key,
                         settings->priv->schema_name, error->message);
              g_warning ("Using untranslated default instead.");
              g_error_free (error);
            }
        }
    }

  if (value == NULL)
    /* either translation failed or there was none to do.
     * use the pre-compiled default.
     */
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
  gchar *path;

  sval = g_settings_schema_get_value (settings->priv->schema, key, NULL);
  correct_type = g_variant_is_of_type (value, g_variant_get_type (sval));
  g_variant_unref (sval);

  g_return_if_fail (correct_type);

  path = g_strconcat (settings->priv->path, key, NULL);
  g_settings_backend_write (settings->priv->backend, path, value, NULL);
  g_free (path);
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
 * g_settings_get_child:
 * @settings: a #GSettings object
 * @name: the name of the 'child' schema
 * @returns: a 'child' settings object
 *
 * Creates a 'child' settings object which has a base path of
 * <replaceable>base-path</replaceable>/@name", where
 * <replaceable>base-path</replaceable> is the base path of @settings.
 *
 * The schema for the child settings object must have been declared
 * in the schema of @settings using a <tag>child</tag> element.
 *
 * Since: 2.26
 */
GSettings *
g_settings_get_child (GSettings   *settings,
                      const gchar *name)
{
  GVariant *child_schema;
  gchar *child_path;
  gchar *child_name;
  GSettings *child;

  child_name = g_strconcat (name, "/", NULL);
  child_schema = g_settings_schema_get_value (settings->priv->schema,
                                              child_name, NULL);
  if (child_schema == NULL ||
      !g_variant_is_of_type (child_schema, G_VARIANT_TYPE_STRING))
    g_error ("Schema '%s' has no child '%s'\n",
             settings->priv->schema_name, name);

  child_path = g_strconcat (settings->priv->path, child_name, NULL);
  child = g_object_new (G_TYPE_SETTINGS,
                        "schema", g_variant_get_string (child_schema, NULL),
                        "path", child_path,
                        NULL);
  g_variant_unref (child_schema);
  g_free (child_path);
  g_free (child_name);

  return child;
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

typedef struct
{
  GSettings *settings;
  GObject *object;

  GSettingsBindGetMapping get_mapping;
  GSettingsBindSetMapping set_mapping;
  gpointer user_data;

  guint property_handler_id;
  const GParamSpec *property;
  guint key_handler_id;
  GVariantType *type;
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

  g_variant_type_free (binding->type);
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
  if (binding->get_mapping (&value, variant, binding->user_data))
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
  if ((variant = binding->set_mapping (&value, binding->type,
                                       binding->user_data)))
    {
      g_settings_set_value (binding->settings,
                            binding->key,
                            variant);
      g_variant_unref (variant);
    }
  g_value_unset (&value);

  binding->running = FALSE;
}

static GVariant *
g_settings_set_mapping (const GValue       *value,
                        const GVariantType *expected_type,
                        gpointer            user_data)
{
  if (g_variant_type_is_subtype_of (expected_type, G_VARIANT_TYPE_BOOLEAN))
    return g_variant_new_boolean (g_value_get_boolean (value));

  g_error ("Sorry; non-boolean bindings are not supported\n");
}

static gboolean
g_settings_get_mapping (GValue   *value,
                        GVariant *variant,
                        gpointer  user_data)
{
  if (g_variant_is_of_type (variant, G_VARIANT_TYPE_BOOLEAN))
    g_value_set_boolean (value, g_variant_get_boolean (variant));
  else
    g_error ("Sorry; non-boolean bindings are not supported\n");

  return TRUE;
}

void
g_settings_bind (GSettings          *settings,
                 const gchar        *key,
                 gpointer            object,
                 const gchar        *property,
                 GSettingsBindFlags  flags)
{
  g_settings_bind_with_mapping (settings, key, object, property,
                                flags, NULL, NULL, NULL);
}

void
g_settings_bind_with_mapping (GSettings               *settings,
                              const gchar             *key,
                              gpointer                 object,
                              const gchar             *property,
                              GSettingsBindFlags       flags,
                              GSettingsBindGetMapping  get_mapping,
                              GSettingsBindSetMapping  set_mapping,
                              gpointer                 user_data)
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
  binding->user_data = user_data;
  binding->get_mapping = get_mapping ? get_mapping : g_settings_get_mapping;
  binding->set_mapping = set_mapping ? set_mapping : g_settings_set_mapping;

  if (!(flags & (G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET)))
    flags |= G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET;

  if (binding->property == NULL)
    {
      g_critical ("g_settings_bind: no property '%s' on class '%s'",
                  property, G_OBJECT_TYPE_NAME (object));
      return;
    }

  {
    GVariant *value;

    value = g_settings_schema_get_value (settings->priv->schema, key, NULL);
    binding->type = g_variant_type_copy (g_variant_get_type (value));
    g_variant_unref (value);
  }

  if (binding->type == NULL)
    {
      g_critical ("g_settings_bind: no key '%s' on schema '%s'",
                  key, settings->priv->schema_name);
      return;
    }

  if (get_mapping == NULL || set_mapping == NULL)
    {
      /* XXX do some simple checks for type compatibility 

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
*/
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
