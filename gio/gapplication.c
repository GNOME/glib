/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2010 Red Hat, Inc
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Colin Walters <walters@verbum.org>
 *          Emmanuele Bassi <ebassi@linux.intel.com>
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include <gobject/gvaluecollector.h>

#include "gapplication.h"
#include "gio-marshal.h"
#include "glibintl.h"

#include "gioerror.h"
#include "ginitable.h"

#include "gdbusconnection.h"
#include "gdbusintrospection.h"
#include "gdbusmethodinvocation.h"

#include "gioalias.h"

/**
 * SECTION: gapplication
 * @title: GApplication
 * @short_description: Core application class
 *
 * A #GApplication is the foundation of an application, unique for a
 * given application identifier.  The #GApplication wraps some
 * low-level platform-specific services and is intended to act as the
 * foundation for higher-level application classes such as
 * #GtkApplication or #MxApplication.  In general, you should not use
 * this class outside of a higher level framework.  By default,
 * g_application_register_with_data() will invoke g_error() if it is
 * run in a context where it cannot support its core features.  Note
 * that g_error() is by default fatal.
 *
 * One of the core features that #GApplication provides is process
 * uniqueness, in the context of a "session".  The session concept is
 * platform-dependent, but corresponds roughly to a graphical desktop
 * login.  When your application is launched again, its arguments
 * are passed through platform communication to the already running
 * program.
 *
 * In addition, #GApplication provides support for 'actions', which
 * can be presented to the user in a platform-specific way
 * (e.g. Windows 7 jump lists). Note that these are just simple
 * actions without parameters. For more flexible scriptability,
 * implementing a a separate D-Bus interface is recommended, see e.g.
 * <xref linkend="gdbus-convenience"/>.
 * 
 * Finally, #GApplication acts as a basic lifecycle root; see the
 * g_application_run() and g_application_quit_with_data() methods.
 *
 * Before using #GApplication, you must choose an "application identifier".
 * The expected form of an application identifier is very close to that of
 * of a <ulink url="http://dbus.freedesktop.org/doc/dbus-specification.html#message-protocol-names-interface">DBus bus name</ulink>.
 * Examples include: "com.example.MyApp" "org.example.internal-apps.Calculator"
 * For convenience, the restrictions on application identifiers are reproduced
 * here:
 * <itemizedlist>
 *   <listitem>Application identifiers must contain only the ASCII characters "[A-Z][a-z][0-9]_-" and must not begin with a digit.</listitem>
 *   <listitem>Application identifiers must contain at least one '.' (period) character (and thus at least two elements).</listitem>
 *   <listitem>Application identifiers must not begin with a '.' (period) character.</listitem>
 *   <listitem>Application identifiers must not exceed 255 characters.</listitem>
 * </itemizedlist>
 *
 * <refsect2><title>D-Bus implementation</title>
 * <para>
 * On UNIX systems using D-Bus, #GApplication is implemented by claiming the
 * application identifier as a bus name on the session bus. The implementation
 * exports an object at the object path that is created by replacing '.' with
 * '/' in the application identifier (e.g. the object path for the
 * application id 'org.gtk.TestApp' is '/org/gtk/TestApp'). The object
 * implements the org.gtk.Application interface.
 * </para>
 * <classsynopsis class="interface">
 *   <ooclass><interfacename>org.gtk.Application</interfacename></ooclass>
 *   <methodsynopsis>
 *     <void/>
 *     <methodname>Activate</methodname>
 *     <methodparam><modifier>in</modifier><type>aay</type><parameter>arguments</parameter></methodparam>
 *     <methodparam><modifier>in</modifier><type>a{sv}</type><parameter>data</parameter></methodparam>
 *   </methodsynopsis>
 *   <methodsynopsis>
 *     <void/>
 *     <methodname>InvokeAction</methodname>
 *     <methodparam><modifier>in</modifier><type>s</type><parameter>action</parameter></methodparam>
 *     <methodparam><modifier>in</modifier><type>a{sv}</type><parameter>data</parameter></methodparam>
 *   </methodsynopsis>
 *   <methodsynopsis>
 *     <type>a{s(sb)}</type>
 *     <methodname>ListActions</methodname>
 *     <void/>
 *   </methodsynopsis>
 *   <methodsynopsis>
 *     <void/>
 *     <methodname>Quit</methodname>
 *     <methodparam><modifier>in</modifier><type>a{sv}</type><parameter>data</parameter></methodparam>
 *   </methodsynopsis>
 *   <methodsynopsis>
 *     <modifier>Signal</modifier>
 *     <void/>
 *     <methodname>ActionsChanged</methodname>
 *     <void/>
 *   </methodsynopsis>
 * </classsynopsis>
 * <para>
 * The <methodname>Activate</methodname> function is called on the existing
 * application instance when a second instance fails to take the bus name.
 * @arguments contains the commandline arguments given to the second instance
 * and @data contains platform-specific additional data.
 *
 * On all platforms, @data will have a key "cwd" of type signature
 * "ay" which contains the working directory of the invoked
 * executable; this data is defined to be in the default GLib
 * filesystem encoding for the platform.  See g_filename_to_utf8().
 *
 * </para>
 * <para>
 * The <methodname>InvokeAction</methodname> function can be called to
 * invoke one of the actions exported by the application.  On X11
 * platforms, the platform_data argument should have a "timestamp"
 * parameter of type "u" with the server time of the initiating event.
 * </para>
 * <para>
 * The <methodname>ListActions</methodname> function returns a dictionary
 * with the exported actions of the application. The keys of the dictionary
 * are the action names, and the values are structs containing the description
 * for the action and a boolean that represents if the action is enabled or not.
 * </para>
 * <para>
 * The <methodname>Quit</methodname> function can be called to
 * terminate the application. The @data parameter contains
 * platform-specific data.  On X11 platforms, the platform_data
 * argument should have a "timestamp" parameter of type "u" with the
 * server time of the initiating event.
 * </para>
 * <para>
 * The <methodname>ActionsChanged</methodname> signal is emitted when the
 * exported actions change (i.e. an action is added, removed, enabled,
 * disabled, or otherwise changed).
 * </para>
 * <para>
 * #GApplication is supported since Gio 2.26.
 * </para>
 * </refsect2>
 */

static void initable_iface_init       (GInitableIface      *initable_iface);

G_DEFINE_TYPE_WITH_CODE (GApplication, g_application, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init));


enum
{
  PROP_0,

  PROP_APPLICATION_ID,
  PROP_REGISTER,
  PROP_DEFAULT_QUIT,
  PROP_IS_REMOTE,
  PROP_ARGV,
  PROP_PLATFORM_DATA
};

enum
{
  QUIT_WITH_DATA,
  ACTION_WITH_DATA,
  PREPARE_ACTIVATION,

  LAST_SIGNAL
};

static guint application_signals[LAST_SIGNAL] = { 0 };

typedef struct {
  gchar *name;
  gchar *description;
  guint enabled : 1;
} GApplicationAction;

struct _GApplicationPrivate
{
  gchar *appid;
  GHashTable *actions; /* name -> GApplicationAction */
  GMainLoop *mainloop;

  GVariant *argv;
  GVariant *platform_data;

  guint do_register  : 1;
  guint default_quit : 1;
  guint is_remote    : 1;

  guint actions_changed_id;

#ifdef G_OS_UNIX
  gchar *dbus_path;
  GDBusConnection *session_bus;
#endif
};

static GApplication *primary_application = NULL;
static GHashTable *instances_for_appid = NULL;

static gboolean initable_init (GInitable     *initable,
			       GCancellable  *cancellable,
			       GError       **error);

static gboolean _g_application_platform_init                    (GApplication  *app,
								 GCancellable  *cancellable,
								 GError       **error); 
static gboolean _g_application_platform_register                (GApplication  *app,
								 gboolean      *unique,
								 GCancellable  *cancellable,
								 GError       **error); 

static void     _g_application_platform_remote_invoke_action    (GApplication  *app,
                                                                 const gchar   *action,
                                                                 GVariant      *platform_data);
static void     _g_application_platform_remote_quit             (GApplication  *app,
                                                                 GVariant      *platform_data);
static void     _g_application_platform_on_actions_changed      (GApplication  *app);

static void
initable_iface_init (GInitableIface *initable_iface)
{
  initable_iface->init = initable_init;
}

#ifdef G_OS_UNIX
#include "gdbusapplication.c"
#else
#include "gnullapplication.c"
#endif

static gboolean
_g_application_validate_id (const char *id)
{
  gboolean allow_dot;

  if (strlen (id) > 255)
    return FALSE;

  if (!g_ascii_isalpha (*id))
    return FALSE;

  id++;
  allow_dot = FALSE;
  for (; *id; id++)
    {
      if (g_ascii_isalnum (*id) || (*id == '-') || (*id == '_'))
        allow_dot = TRUE;
      else if (allow_dot && *id == '.')
        allow_dot = FALSE;
      else
        return FALSE;
    }
  return TRUE;
}

static gpointer
init_appid_statics (gpointer data)
{
  instances_for_appid = g_hash_table_new (g_str_hash, g_str_equal);
  return NULL;
}

static GApplication *
application_for_appid (const char *appid)
{
  static GOnce appid_once = G_ONCE_INIT;

  g_once (&appid_once, init_appid_statics, NULL);

  return g_hash_table_lookup (instances_for_appid, appid);
}

static gboolean
g_application_default_quit_with_data (GApplication *application,
				      GVariant     *platform_data)
{
  g_return_val_if_fail (application->priv->mainloop != NULL, FALSE);
  g_main_loop_quit (application->priv->mainloop);

  return TRUE;
}

static void
g_application_default_run (GApplication *application)
{
  if (application->priv->mainloop == NULL)
    application->priv->mainloop = g_main_loop_new (NULL, TRUE);

  g_main_loop_run (application->priv->mainloop);
}

static GVariant *
append_cwd_to_platform_data (GVariant *platform_data)
{
  GVariantBuilder builder;
  gchar *cwd;
  GVariant *result;

  cwd = g_get_current_dir ();

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));
  if (cwd)
    g_variant_builder_add (&builder, "{sv}",
			   "cwd",
			   g_variant_new_byte_array (cwd, -1));
  g_free (cwd);

  if (platform_data)
    {
      GVariantIter iter;
      GVariant *item;

      g_variant_iter_init (&iter, platform_data);
      while (g_variant_iter_next (&iter, "@{sv}", &item))
	{
	  g_variant_builder_add_value (&builder, item);
	  g_variant_unref (item);
	}
    }
  result = g_variant_builder_end (&builder);
  return result;
}

static GVariant *
variant_from_argv (int    argc,
		   char **argv)
{
  GVariantBuilder builder;
  int i;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("aay"));

  for (i = 1; i < argc; i++)
    {
      guint8 *argv_bytes;

      argv_bytes = (guint8*) argv[i];
      g_variant_builder_add_value (&builder,
				   g_variant_new_byte_array (argv_bytes, -1));
    }
  
  return g_variant_builder_end (&builder);
}

static gboolean
timeout_handle_actions_changed (gpointer user_data)
{
  GApplication *application = user_data;

  application->priv->actions_changed_id = 0;

  _g_application_platform_on_actions_changed (application);

  return FALSE;
}

static inline void
queue_actions_change_notification (GApplication *application)
{
  GApplicationPrivate *priv = application->priv;

  if (priv->actions_changed_id == 0)
    priv->actions_changed_id = g_timeout_add (0, timeout_handle_actions_changed, application);
}

static gboolean
initable_init (GInitable     *initable,
               GCancellable  *cancellable,
               GError       **error)
{
  GApplication *app = G_APPLICATION (initable);
  gboolean unique;

  if (!_g_application_platform_init (app, cancellable, error))
    return FALSE;

  if (app->priv->do_register &&
      !_g_application_platform_register (app, &unique, cancellable ,error))
    return FALSE;

  return TRUE;
}

static void
g_application_action_free (gpointer data)
{
  if (G_LIKELY (data != NULL))
    {
      GApplicationAction *action = data;

      g_free (action->name);
      g_free (action->description);

      g_slice_free (GApplicationAction, action);
    }
}

/**
 * g_application_new:
 * @appid: System-dependent application identifier
 * @argc: Number of arguments in @argv
 * @argv: (allow-none) (array length=argc): Argument vector, usually from the <parameter>argv</parameter> parameter of main() 
 *
 * Create a new #GApplication.  This uses a platform-specific
 * mechanism to ensure the current process is the unique owner of the
 * application (as defined by the @appid). If successful, the
 * #GApplication:is-remote property will be %FALSE, and it is safe to
 * continue creating other resources such as graphics windows.
 *
 * If the given @appid is already running in another process, the the
 * GApplication::activate_with_data signal will be emitted in the
 * remote process, with the data from @argv and other
 * platform-specific data available.  Subsequently the
 * #GApplication:default-exit property will be evaluated.  If it's
 * %TRUE, then the current process will terminate.  If %FALSE, then
 * the application remains in the #GApplication:is-remote state, and
 * you can e.g. call g_application_invoke_action().
 *
 * This function may do synchronous I/O to obtain unique ownership
 * of the application id, and will block the calling thread in this
 * case.
 *
 * If the environment does not support the basic functionality of
 * #GApplication, this function will invoke g_error(), which by
 * default is a fatal operation.  This may arise for example on
 * UNIX systems using D-Bus when the session bus is not available.
 *
 * As a convenience, this function is defined to call g_type_init() as
 * its very first action.
 *
 * Returns: (transfer full): An application instance
 *
 * Since: 2.26
 */
GApplication *
g_application_new (const gchar *appid,
		   int          argc,
		   char       **argv)
{
  GObject *app;
  GError *error = NULL;
  GVariant *argv_variant;

  g_type_init ();

  g_return_val_if_fail (appid != NULL, NULL);
  
  argv_variant = variant_from_argv (argc, argv);
  
  app = g_initable_new (G_TYPE_APPLICATION, 
			NULL,
			&error,
			"application-id", appid, 
			"argv", argv_variant, 
			NULL);
  if (!app)
    {
      g_error ("%s", error->message);
      g_clear_error (&error);
      return NULL;
    }
  return G_APPLICATION (app);
}

/**
 * g_application_try_new:
 * @appid: System-dependent application identifier
 * @argc: Number of arguments in @argv
 * @argv: (allow-none) (array length=argc): Argument vector, usually from the <parameter>argv</parameter> parameter of main() 
 * @error: a #GError
 *
 * This function is similar to g_application_new(), but allows for
 * more graceful fallback if the environment doesn't support the
 * basic #GApplication functionality.
 *
 * Returns: (transfer full): An application instance
 *
 * Since: 2.26
 */
GApplication *
g_application_try_new (const gchar *appid,
		       int          argc,
		       char       **argv,
		       GError     **error)
{
  GVariant *argv_variant;

  g_type_init ();

  g_return_val_if_fail (appid != NULL, NULL);
  
  argv_variant = variant_from_argv (argc, argv);
  
  return G_APPLICATION (g_initable_new (G_TYPE_APPLICATION, 
					NULL,
					error,
					"application-id", appid, 
					"argv", argv_variant, 
					NULL));
}

/**
 * g_application_unregistered_try_new:
 * @appid: System-dependent application identifier
 * @argc: Number of arguments in @argv
 * @argv: (allow-none) (array length=argc): Argument vector, usually from the <parameter>argv</parameter> parameter of main() 
 * @error: a #GError
 *
 * This function is similar to g_application_try_new(), but also
 * sets the GApplication:register property to %FALSE.  You can later
 * call g_application_register() to complete initialization.
 *
 * Returns: (transfer full): An application instance
 *
 * Since: 2.26
 */
GApplication *
g_application_unregistered_try_new (const gchar *appid,
				    int          argc,
				    char       **argv,
				    GError     **error)
{
  GVariant *argv_variant;

  g_type_init ();

  g_return_val_if_fail (appid != NULL, NULL);
  
  argv_variant = variant_from_argv (argc, argv);
  
  return G_APPLICATION (g_initable_new (G_TYPE_APPLICATION, 
					NULL,
					error,
					"application-id", appid, 
					"argv", argv_variant, 
					"register", FALSE,
					NULL));
}

/**
 * g_application_register:
 * @application: a #GApplication
 *
 * By default, #GApplication ensures process uniqueness when
 * initialized, but this behavior is controlled by the
 * GApplication:register property.  If it was given as %FALSE at
 * construction time, this function allows you to later attempt
 * to ensure uniqueness.
 *
 * Returns: %TRUE if registration was successful
 */
gboolean
g_application_register (GApplication *application)
{
  gboolean unique;

  g_return_val_if_fail (G_IS_APPLICATION (application), FALSE);
  g_return_val_if_fail (application->priv->is_remote, FALSE);

  if (!_g_application_platform_register (application, &unique, NULL, NULL))
    return FALSE;
  return unique;
}

/**
 * g_application_add_action:
 * @application: a #GApplication
 * @name: the action name
 * @description: the action description; can be a translatable
 *   string
 *
 * Adds an action @name to the list of exported actions of @application.
 *
 * It is an error to call this function if @application is a proxy for
 * a remote application.
 *
 * You can invoke an action using g_application_invoke_action().
 *
 * The newly added action is enabled by default; you can call
 * g_application_set_action_enabled() to disable it.
 *
 * Since: 2.26
 */
void
g_application_add_action (GApplication *application,
                          const gchar  *name,
                          const gchar  *description)
{
  GApplicationPrivate *priv;
  GApplicationAction *action;

  g_return_if_fail (G_IS_APPLICATION (application));
  g_return_if_fail (name != NULL && *name != '\0');
  g_return_if_fail (!application->priv->is_remote);

  priv = application->priv;

  g_return_if_fail (g_hash_table_lookup (priv->actions, name) == NULL);

  action = g_slice_new (GApplicationAction);
  action->name = g_strdup (name);
  action->description = g_strdup (description);
  action->enabled = TRUE;

  g_hash_table_insert (priv->actions, action->name, action);
  queue_actions_change_notification (application);
}

/**
 * g_application_remove_action:
 * @application: a #GApplication
 * @name: the name of the action to remove
 *
 * Removes the action @name from the list of exported actions of @application.
 *
 * It is an error to call this function if @application is a proxy for
 * a remote application.
 *
 * Since: 2.26
 */
void
g_application_remove_action (GApplication *application,
                             const gchar  *name)
{
  GApplicationPrivate *priv;

  g_return_if_fail (G_IS_APPLICATION (application));
  g_return_if_fail (name != NULL && *name != '\0');
  g_return_if_fail (!application->priv->is_remote);

  priv = application->priv;

  g_return_if_fail (g_hash_table_lookup (priv->actions, name) != NULL);

  g_hash_table_remove (priv->actions, name);
  queue_actions_change_notification (application);
}

/**
 * g_application_invoke_action:
 * @application: a #GApplication
 * @name: the name of the action to invoke
 * @platform_data: (allow-none): platform-specific event data
 *
 * Invokes the action @name of the passed #GApplication.
 *
 * This function has different behavior depending on whether @application
 * is acting as a proxy for another process.  In the normal case where
 * the current process is hosting the application, and the specified
 * action exists and is enabled, the #GApplication::action signal will
 * be emitted.
 *
 * If @application is a proxy, then the specified action will be invoked
 * in the remote process. It is not necessary to call
 * g_application_add_action() in the current process in order to invoke
 * one remotely.
 *
 * Since: 2.26
 */
void
g_application_invoke_action (GApplication *application,
                             const gchar  *name,
                             GVariant     *platform_data)
{
  GApplicationPrivate *priv;
  GApplicationAction *action;

  g_return_if_fail (G_IS_APPLICATION (application));
  g_return_if_fail (name != NULL);
  g_return_if_fail (platform_data == NULL
                    || g_variant_is_of_type (platform_data, G_VARIANT_TYPE ("a{sv}")));
  
  if (platform_data == NULL)
    platform_data = g_variant_new_array (G_VARIANT_TYPE ("{sv}"), NULL, 0);
  else
    g_variant_ref (platform_data);

  priv = application->priv;
  
  if (priv->is_remote)
    {
      _g_application_platform_remote_invoke_action (application, name, platform_data);
      goto out;
    }

  action = g_hash_table_lookup (priv->actions, name);
  g_return_if_fail (action != NULL);
  if (!action->enabled)
    goto out;

  g_signal_emit (application, application_signals[ACTION_WITH_DATA],
                 g_quark_from_string (name),
                 name,
                 platform_data);

 out:
  g_variant_unref (platform_data);
}

/**
 * g_application_list_actions:
 * @application: a #GApplication
 *
 * Retrieves the list of action names currently exported by @application.
 *
 * It is an error to call this function if @application is a proxy for
 * a remote application.
 *
 * Return value: (transfer full): a newly allocation, %NULL-terminated array
 *   of strings containing action names; use g_strfreev() to free the
 *   resources used by the returned array
 *
 * Since: 2.26
 */
gchar **
g_application_list_actions (GApplication *application)
{
  GApplicationPrivate *priv;
  GHashTableIter iter;
  gpointer key;
  gchar **retval;
  gint i;

  g_return_val_if_fail (G_IS_APPLICATION (application), NULL);
  g_return_val_if_fail (!application->priv->is_remote, NULL);

  priv = application->priv;

  retval = g_new (gchar*, g_hash_table_size (priv->actions));

  i = 0;
  g_hash_table_iter_init (&iter, priv->actions);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    retval[i++] = g_strdup (key);

  retval[i] = NULL;

  return retval;
}

/**
 * g_application_set_action_enabled:
 * @application: a #GApplication
 * @name: the name of the application
 * @enabled: whether to enable or disable the action @name
 *
 * Sets whether the action @name inside @application should be enabled
 * or disabled.
 *
 * It is an error to call this function if @application is a proxy for
 * a remote application.
 *
 * Invoking a disabled action will not result in the #GApplication::action
 * signal being emitted.
 *
 * Since: 2.26
 */
void
g_application_set_action_enabled (GApplication *application,
                                  const gchar  *name,
                                  gboolean      enabled)
{
  GApplicationAction *action;

  g_return_if_fail (G_IS_APPLICATION (application));
  g_return_if_fail (name != NULL);
  g_return_if_fail (!application->priv->is_remote);

  enabled = !!enabled;

  action = g_hash_table_lookup (application->priv->actions, name);
  g_return_if_fail (action != NULL);
  if (action->enabled == enabled)
    return;

  action->enabled = enabled;

  queue_actions_change_notification (application);
}


/**
 * g_application_get_action_description:
 * @application: a #GApplication
 * @name: Action name
 *
 * Gets the description of the action @name.
 *
 * It is an error to call this function if @application is a proxy for
 * a remote application.
 *
 * Returns: Description for the given action named @name
 *
 * Since: 2.26
 */
G_CONST_RETURN gchar *
g_application_get_action_description (GApplication *application,
                                      const gchar  *name)
{
  GApplicationAction *action;
  
  g_return_val_if_fail (G_IS_APPLICATION (application), NULL);
  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (!application->priv->is_remote, NULL);

  action = g_hash_table_lookup (application->priv->actions, name);
  g_return_val_if_fail (action != NULL, NULL);

  return action->description;
}


/**
 * g_application_get_action_enabled:
 * @application: a #GApplication
 * @name: the name of the action
 *
 * Retrieves whether the action @name is enabled or not.
 *
 * See g_application_set_action_enabled().
 *
 * It is an error to call this function if @application is a proxy for
 * a remote application.
 *
 * Return value: %TRUE if the action was enabled, and %FALSE otherwise
 *
 * Since: 2.26
 */
gboolean
g_application_get_action_enabled (GApplication *application,
                                  const gchar  *name)
{
  GApplicationAction *action;

  g_return_val_if_fail (G_IS_APPLICATION (application), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);
  g_return_val_if_fail (!application->priv->is_remote, FALSE);

  action = g_hash_table_lookup (application->priv->actions, name);
  g_return_val_if_fail (action != NULL, FALSE);

  return action->enabled;
}

/**
 * g_application_run:
 * @application: a #GApplication
 *
 * Starts the application.
 *
 * The default implementation of this virtual function will simply run
 * a main loop.
 *
 * It is an error to call this function if @application is a proxy for
 * a remote application.
 *
 * Since: 2.26
 */
void
g_application_run (GApplication *application)
{
  g_return_if_fail (G_IS_APPLICATION (application));
  g_return_if_fail (!application->priv->is_remote);

  G_APPLICATION_GET_CLASS (application)->run (application);
}

/**
 * g_application_quit_with_data:
 * @application: a #GApplication
 * @platform_data: (allow-none): platform-specific data
 *
 * Request that the application quits.
 *
 * This function has different behavior depending on whether @application
 * is acting as a proxy for another process.  In the normal case where
 * the current process is hosting the application, the default
 * implementation will quit the main loop created by g_application_run().
 *
 * If @application is a proxy, then the remote process will be asked
 * to quit.
 *
 * Returns: %TRUE if the application accepted the request, %FALSE otherwise
 *
 * Since: 2.26
 */
gboolean
g_application_quit_with_data (GApplication *application,
			      GVariant     *platform_data)
{
  gboolean retval = FALSE;

  g_return_val_if_fail (G_IS_APPLICATION (application), FALSE);
  g_return_val_if_fail (platform_data == NULL
			|| g_variant_is_of_type (platform_data, G_VARIANT_TYPE ("a{sv}")), FALSE);

  if (platform_data == NULL)
    platform_data = g_variant_new_array (G_VARIANT_TYPE ("{sv}"), NULL, 0);
  else
    g_variant_ref (platform_data);

  if (application->priv->is_remote)
    {
       _g_application_platform_remote_quit (application, platform_data);
       retval = TRUE;
    }
  else
    g_signal_emit (application, application_signals[QUIT_WITH_DATA], 0, platform_data, &retval);

  g_variant_unref (platform_data);

  return retval;
}

/**
 * g_application_get_instance:
 *
 * In the normal case where there is exactly one #GApplication instance
 * in this process, return that instance.  If there are multiple, the
 * first one created will be returned.  Otherwise, return %NULL.
 *
 * Returns: (transfer none): The primary instance of #GApplication,
 *   or %NULL if none is set
 *
 * Since: 2.26
 */
GApplication *
g_application_get_instance (void)
{
  return primary_application;
}

/**
 * g_application_get_id:
 * @application: a #GApplication
 *
 * Retrieves the platform-specific identifier for the #GApplication.
 *
 * Return value: The platform-specific identifier. The returned string
 *   is owned by the #GApplication instance and it should never be
 *   modified or freed
 *
 * Since: 2.26
 */
G_CONST_RETURN gchar *
g_application_get_id (GApplication *application)
{
  g_return_val_if_fail (G_IS_APPLICATION (application), NULL);

  return application->priv->appid;
}

/**
 * g_application_is_remote:
 * @application: a #GApplication
 *
 * Returns whether the object represents a proxy for a remote application.
 *
 * Returns: %TRUE if this object represents a proxy for a remote application.
 */
gboolean
g_application_is_remote (GApplication *application)
{
  g_return_val_if_fail (G_IS_APPLICATION (application), FALSE);

  return application->priv->is_remote;
}

static void
g_application_init (GApplication *app)
{
  app->priv = G_TYPE_INSTANCE_GET_PRIVATE (app,
                                           G_TYPE_APPLICATION,
                                           GApplicationPrivate);

  app->priv->actions = g_hash_table_new_full (g_str_hash, g_str_equal,
                                              NULL,
                                              g_application_action_free);
  app->priv->default_quit = TRUE;
  app->priv->do_register = TRUE;
  app->priv->is_remote = TRUE;
  app->priv->platform_data = g_variant_new_array (G_VARIANT_TYPE ("{sv}"), NULL, 0);
}

static void
g_application_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GApplication *app = G_APPLICATION (object);

  switch (prop_id)
    {
    case PROP_APPLICATION_ID:
      g_value_set_string (value, g_application_get_id (app));
      break;

    case PROP_DEFAULT_QUIT:
      g_value_set_boolean (value, app->priv->default_quit);
      break;

    case PROP_IS_REMOTE:
      g_value_set_boolean (value, g_application_is_remote (app));
      break;

    case PROP_REGISTER:
      g_value_set_boolean (value, app->priv->do_register);
      break;

    case PROP_ARGV:
      g_value_set_variant (value, app->priv->argv);
      break;

    case PROP_PLATFORM_DATA:
      g_value_set_variant (value, app->priv->platform_data);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_application_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GApplication *app = G_APPLICATION (object);

  switch (prop_id)
    {
    case PROP_APPLICATION_ID:
      g_return_if_fail (_g_application_validate_id (g_value_get_string (value)));
      app->priv->appid = g_value_dup_string (value);
      break;

    case PROP_DEFAULT_QUIT:
      app->priv->default_quit = g_value_get_boolean (value);
      break;

    case PROP_REGISTER:
      app->priv->do_register = g_value_get_boolean (value);
      break;

    case PROP_ARGV:
      app->priv->argv = g_value_dup_variant (value);
      break;

    case PROP_PLATFORM_DATA:
      {
	GVariant *platform_data = g_value_get_variant (value);
	if (app->priv->platform_data)
	  g_variant_unref (app->priv->platform_data);
	app->priv->platform_data = g_variant_ref_sink (append_cwd_to_platform_data (platform_data));
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static GObject*
g_application_constructor (GType                  type,
                           guint                  n_construct_properties,
                           GObjectConstructParam *construct_params)
{
  GApplication *app;
  GObject *object;
  const char *appid = NULL;
  guint i;

  for (i = 0; i < n_construct_properties; i++)
    {
      GObjectConstructParam *param = &construct_params[i];
      if (strcmp (param->pspec->name, "application-id") == 0)
        appid = g_value_get_string (param->value);
    }

  g_return_val_if_fail (appid != NULL, NULL);

  app = application_for_appid (appid);
  if (app != NULL)
    return g_object_ref (app);

  object = (* G_OBJECT_CLASS (g_application_parent_class)->constructor) (type,
                                                                         n_construct_properties,
                                                                         construct_params);
  app = G_APPLICATION (object);

  if (primary_application == NULL)
    primary_application = app;
  g_hash_table_insert (instances_for_appid, g_strdup (appid), app);

  return object;
}

static void
g_application_finalize (GObject *object)
{
  GApplication *app = G_APPLICATION (object);

  g_free (app->priv->appid);
  if (app->priv->actions)
    g_hash_table_unref (app->priv->actions);
  if (app->priv->actions_changed_id)
    g_source_remove (app->priv->actions_changed_id);
  if (app->priv->mainloop)
    g_main_loop_unref (app->priv->mainloop);

#ifdef G_OS_UNIX
  g_free (app->priv->dbus_path);
  if (app->priv->session_bus)
    g_object_unref (app->priv->session_bus);
#endif

  G_OBJECT_CLASS (g_application_parent_class)->finalize (object);
}

static void
g_application_class_init (GApplicationClass *klass)
{
  GObjectClass *gobject_class G_GNUC_UNUSED = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GApplicationPrivate));

  gobject_class->constructor = g_application_constructor;
  gobject_class->set_property = g_application_set_property;
  gobject_class->get_property = g_application_get_property;

  gobject_class->finalize = g_application_finalize;

  klass->run = g_application_default_run;
  klass->quit_with_data = g_application_default_quit_with_data;

  /**
   * GApplication::quit-with-data:
   * @application: the object on which the signal is emitted
   * @platform_data: Platform-specific data, or %NULL
   *
   * This signal is emitted when the Quit action is invoked on the
   * application.
   *
   * The default handler for this signal exits the mainloop of the
   * application.
   *
   * Returns: %TRUE if the signal has been handled, %FALSE to continue
   *   signal emission
   */
  application_signals[QUIT_WITH_DATA] =
    g_signal_new (g_intern_static_string ("quit-with-data"),
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GApplicationClass, quit_with_data),
                  g_signal_accumulator_true_handled, NULL,
                  _gio_marshal_BOOLEAN__VARIANT,
                  G_TYPE_BOOLEAN, 1,
                  G_TYPE_VARIANT);

  /**
   * GApplication::action-with-data:
   * @application: the object on which the signal is emitted
   * @name: The name of the activated action
   * @platform_data: Platform-specific data, or %NULL
   *
   * This signal is emitted when an action is activated. The action name
   * is passed as the first argument, but also as signal detail, so it
   * is possible to connect to this signal for individual actions.
   *
   * The signal is never emitted for disabled actions.
   */
  application_signals[ACTION_WITH_DATA] =
    g_signal_new (g_intern_static_string ("action-with-data"),
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE | G_SIGNAL_DETAILED,
                  G_STRUCT_OFFSET (GApplicationClass, action_with_data),
                  NULL, NULL,
                  _gio_marshal_VOID__STRING_VARIANT,
                  G_TYPE_NONE, 2,
                  G_TYPE_STRING,
                  G_TYPE_VARIANT);

   /**
   * GApplication::prepare-activation:
   * @application: the object on which the signal is emitted
   * @arguments: A #GVariant with the signature "aay"
   * @platform_data: A #GVariant with the signature "a{sv}", or %NULL
   *
   * This signal is emitted when a non-primary process for a given
   * application is invoked while your application is running; for
   * example, when a file browser launches your program to open a
   * file.  The raw operating system arguments are passed in the
   * @arguments variant.  Additional platform-dependent data is
   * stored in @platform_data.
   */
  application_signals[PREPARE_ACTIVATION] =
    g_signal_new (g_intern_static_string ("prepare-activation"),
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GApplicationClass, prepare_activation),
                  NULL, NULL,
                  _gio_marshal_VOID__VARIANT_VARIANT,
                  G_TYPE_NONE, 2,
                  G_TYPE_VARIANT,
                  G_TYPE_VARIANT);

  /**
   * GApplication:application-id:
   *
   * The unique identifier for this application.  See the documentation for
   * #GApplication for more information about this property.
   *
   */
  g_object_class_install_property (gobject_class,
                                   PROP_APPLICATION_ID,
                                   g_param_spec_string ("application-id",
                                                        P_("Application ID"),
                                                        P_("Identifier for this application"),
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GApplication:argv:
   *
   * The argument vector given to this application.  It must be a #GVariant
   * with a type signature "aay".
   *
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ARGV,
                                   g_param_spec_variant ("argv",
						        P_("Argument vector"),
						        P_("System argument vector with type signature aay"),
						        G_VARIANT_TYPE ("aay"),
                                                        NULL,
						        G_PARAM_READWRITE |
						        G_PARAM_CONSTRUCT_ONLY |
						        G_PARAM_STATIC_STRINGS));

  /**
   * GApplication:platform-data:
   *
   * Platform-specific data retrieved from the operating system
   * environment.  It must be a #GVariant with type signature "a{sv}".
   *
   */
  g_object_class_install_property (gobject_class,
                                   PROP_PLATFORM_DATA,
                                   g_param_spec_variant ("platform-data",
						         P_("Platform data"),
						         P_("Environmental data, must have type signature a{sv}"),
						         G_VARIANT_TYPE ("a{sv}"),
                                                         NULL,
						         G_PARAM_READWRITE |
						         G_PARAM_CONSTRUCT_ONLY |
						         G_PARAM_STATIC_STRINGS));

  /**
   * GApplication:default-quit:
   *
   * By default, if a different process is running this application, the
   * process will be exited.  Set this property to %FALSE to allow custom
   * interaction with the remote process.
   *
   */
  g_object_class_install_property (gobject_class,
                                   PROP_DEFAULT_QUIT,
                                   g_param_spec_boolean ("default-quit",
                                                         P_("Default Quit"),
                                                         P_("Exit the process by default"),
                                                         TRUE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_CONSTRUCT_ONLY |
                                                         G_PARAM_STATIC_STRINGS));


  /**
   * GApplication:is-remote:
   *
   * This property is %TRUE if this application instance represents a proxy
   * to the instance of this application in another process.
   *
   */
  g_object_class_install_property (gobject_class,
                                   PROP_IS_REMOTE,
                                   g_param_spec_boolean ("is-remote",
                                                         P_("Is Remote"),
                                                         P_("Whether this application is a proxy for another process"),
                                                         TRUE,
                                                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GApplication:register:
   *
   * If this property is %FALSE, the application construction will not attempt
   * to ensure process uniqueness, and the application is guaranteed to be in the
   * remote state.  See GApplication:is-remote.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_REGISTER,
                                   g_param_spec_boolean ("register",
                                                         P_("Register"),
                                                         P_("If false, do not "),
                                                         TRUE,
                                                         G_PARAM_READWRITE | 
							 G_PARAM_CONSTRUCT_ONLY |
							 G_PARAM_STATIC_STRINGS));
}

#define __G_APPLICATION_C__
#include "gioaliasdef.c"
