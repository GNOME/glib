/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
 * Copyright (C) 2014 Руслан Ижбулатов
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Alexander Larsson <alexl@redhat.com>
 *          Руслан Ижбулатов  <lrn1986@gmail.com>
 */

#include "config.h"

#include <string.h>

#include "gcontenttype.h"
#include "gwin32appinfo.h"
#include "gappinfo.h"
#include "gioerror.h"
#include "gfile.h"
#include <glib/gstdio.h>
#include "glibintl.h"
#include <gio/gwin32registrykey.h>

#include <windows.h>

#include <glib/gstdioprivate.h>
#include "glib-private.h"

/* We need to watch 8 places:
 * 0) HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations
 *    (anything below that key)
 *    On change: re-enumerate subkeys, read their values.
 * 1) HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts
 *    (anything below that key)
 *    On change: re-enumerate subkeys
 * 2) HKEY_CURRENT_USER\\Software\\Clients (anything below that key)
 *    On change: re-read the whole hierarchy of handlers
 * 3) HKEY_LOCAL_MACHINE\\Software\\Clients (anything below that key)
 *    On change: re-read the whole hierarchy of handlers
 * 4) HKEY_LOCAL_MACHINE\\Software\\RegisteredApplications (values of that key)
 *    On change: re-read the value list of registered applications
 * 5) HKEY_CURRENT_USER\\Software\\RegisteredApplications (values of that key)
 *    On change: re-read the value list of registered applications
 * 6) HKEY_CLASSES_ROOT\\Applications (anything below that key)
 *    On change: re-read the whole hierarchy of apps
 * 7) HKEY_CLASSES_ROOT (only its subkeys)
 *    On change: re-enumerate subkeys, try to filter out wrong names.
 *
 *
 * About verbs. A registry key (the name of that key is known as ProgID)
 * can contain a "shell" subkey, which can then contain a number of verb
 * subkeys (the most common being the "open" verb), and each of these
 * contains a "command" subkey, which has a default string value that
 * is the command to be run.
 * Most ProgIDs are in HKEY_CLASSES_ROOT, but some are nested deeper in
 * the registry (such as HKEY_CURRENT_USER\\Software\\<softwarename>).
 *
 * Verb selection works like this (according to https://docs.microsoft.com/en-us/windows/win32/shell/context ):
 * 1) If "open" verb is available, that verb is used.
 * 2) If the Shell subkey has a default string value, and if a verb subkey
 *    with that name exists, that verb is used.
 * 3) The first subkey found in the list of verb subkeys is used.
 * 4) The "openwith" verb is used
 *
 * Testing suggests that Windows never reaches the point 4 in any realistic
 * circumstances. If a "command" subkey is missing for a verb, or if it has
 * an empty string as its default value, the app launch fails
 * (the "openwith" verb is not used, even if it's present).
 * If the command is present, but is not valid (runs nonexisting executable,
 * for example), then other verbs are not checked.
 * It seems that when the documentation said "openwith verb", it meant
 * that Windows invokes the default "Open with..." dialog (it does not
 * look at the "openwith" verb subkey, even if it's there).
 * If a verb subkey that is supposed to be used is present, but it lacks
 * a command subkey, an error message is shown and nothing else happens.
 */

#define _verb_idx(array,index) ((GWin32AppInfoShellVerb *) g_ptr_array_index (array, index))

#define _lookup_by_verb(array, verb, dst, itemtype) do { \
  gsize _index; \
  itemtype *_v; \
  for (_index = 0; array && _index < array->len; _index++) \
    { \
      _v = (itemtype *) g_ptr_array_index (array, _index); \
      if (_wcsicmp (_v->verb_name, (verb)) == 0) \
        { \
          *(dst) = _v; \
          break; \
        } \
    } \
  if (array == NULL || _index >= array->len) \
    *(dst) = NULL; \
} while (0)

#define _verb_lookup(array, verb, dst) _lookup_by_verb (array, verb, dst, GWin32AppInfoShellVerb)

/* Because with subcommands a verb would have
 * a name like "foo\\bar", but the key its command
 * should be looked for is "shell\\foo\\shell\\bar\\command"
 */
typedef struct _reg_verb {
  gunichar2 *name;
  gunichar2 *shellpath;
} reg_verb;

typedef struct _GWin32AppInfoURLSchema GWin32AppInfoURLSchema;
typedef struct _GWin32AppInfoFileExtension GWin32AppInfoFileExtension;
typedef struct _GWin32AppInfoShellVerb GWin32AppInfoShellVerb;
typedef struct _GWin32AppInfoHandler GWin32AppInfoHandler;
typedef struct _GWin32AppInfoApplication GWin32AppInfoApplication;

typedef struct _GWin32AppInfoURLSchemaClass GWin32AppInfoURLSchemaClass;
typedef struct _GWin32AppInfoFileExtensionClass GWin32AppInfoFileExtensionClass;
typedef struct _GWin32AppInfoShellVerbClass GWin32AppInfoShellVerbClass;
typedef struct _GWin32AppInfoHandlerClass GWin32AppInfoHandlerClass;
typedef struct _GWin32AppInfoApplicationClass GWin32AppInfoApplicationClass;

struct _GWin32AppInfoURLSchemaClass
{
  GObjectClass parent_class;
};

struct _GWin32AppInfoFileExtensionClass
{
  GObjectClass parent_class;
};

struct _GWin32AppInfoHandlerClass
{
  GObjectClass parent_class;
};

struct _GWin32AppInfoApplicationClass
{
  GObjectClass parent_class;
};

struct _GWin32AppInfoShellVerbClass
{
  GObjectClass parent_class;
};

struct _GWin32AppInfoURLSchema {
  GObject parent_instance;

  /* url schema (stuff before ':') */
  gunichar2 *schema;

  /* url schema (stuff before ':'), in UTF-8 */
  gchar *schema_u8;

  /* url schema (stuff before ':'), in UTF-8, folded */
  gchar *schema_u8_folded;

  /* Handler currently selected for this schema. Can be NULL. */
  GWin32AppInfoHandler *chosen_handler;

  /* Maps folded handler IDs -> to GWin32AppInfoHandlers for this schema.
   * Includes the chosen handler, if any.
   */
  GHashTable *handlers;
};

struct _GWin32AppInfoHandler {
  GObject parent_instance;

  /* Usually a class name in HKCR */
  gunichar2 *handler_id;

  /* Registry object obtained by opening @handler_id. Can be used to watch this handler. */
  GWin32RegistryKey *key;

  /* @handler_id, in UTF-8, folded */
  gchar *handler_id_folded;

  /* Icon of the application for this handler */
  GIcon *icon;

  /* Verbs that this handler supports */
  GPtrArray *verbs; /* of GWin32AppInfoShellVerb */
};

struct _GWin32AppInfoShellVerb {
  GObject parent_instance;

  /* The verb that is used to invoke this handler. */
  gunichar2 *verb_name;

  /* User-friendly (localized) verb name. */
  gchar *verb_displayname;

  /* shell/verb/command */
  gunichar2 *command;

  /* Same as @command, but in UTF-8 */
  gchar *command_utf8;

  /* Executable of the program (UTF-8) */
  gchar *executable;

  /* Executable of the program (for matching, in folded form; UTF-8) */
  gchar *executable_folded;

  /* Pointer to a location within @executable */
  gchar *executable_basename;

  /* If not NULL, then @executable and its derived fields contain the name
   * of a DLL file (without the name of the function that rundll32.exe should
   * invoke), and this field contains the name of the function to be invoked.
   * The application is then invoked as 'rundll32.exe "dll_path",dll_function other_arguments...'.
   */
  gchar *dll_function;

  /* The application that is linked to this verb. */
  GWin32AppInfoApplication *app;
};

struct _GWin32AppInfoFileExtension {
  GObject parent_instance;

  /* File extension (with leading '.') */
  gunichar2 *extension;

  /* File extension (with leading '.'), in UTF-8 */
  gchar *extension_u8;

  /* handler currently selected for this extension. Can be NULL. */
  GWin32AppInfoHandler *chosen_handler;

  /* Maps folded handler IDs -> to GWin32AppInfoHandlers for this extension.
   * Includes the chosen handler, if any.
   */
  GHashTable *handlers;
};

struct _GWin32AppInfoApplication {
  GObject parent_instance;

  /* Canonical name (used for key names).
   * For applications tracked by id this is the root registry
   * key path for the application.
   * For applications tracked by executable name this is the
   * basename of the executable.
   * For fake applications this is the full filename of the
   * executable (as far as it can be inferred from a command line,
   * meaning that it can also be a basename, if that's
   * all that a commandline happen to give us).
   */
  gunichar2 *canonical_name;

  /* @canonical_name, in UTF-8 */
  gchar *canonical_name_u8;

  /* @canonical_name, in UTF-8, folded */
  gchar *canonical_name_folded;

  /* Human-readable name in English. Can be NULL */
  gunichar2 *pretty_name;

  /* Human-readable name in English, UTF-8. Can be NULL */
  gchar *pretty_name_u8;

  /* Human-readable name in user's language. Can be NULL  */
  gunichar2 *localized_pretty_name;

  /* Human-readable name in user's language, UTF-8. Can be NULL  */
  gchar *localized_pretty_name_u8;

  /* Description, could be in user's language. Can be NULL */
  gunichar2 *description;

  /* Description, could be in user's language, UTF-8. Can be NULL */
  gchar *description_u8;

  /* Verbs that this application supports */
  GPtrArray *verbs; /* of GWin32AppInfoShellVerb */

  /* Explicitly supported URLs, hashmap from map-owned gchar ptr (schema,
   * UTF-8, folded) -> to a GWin32AppInfoHandler
   * Schema can be used as a key in the urls hashmap.
   */
  GHashTable *supported_urls;

  /* Explicitly supported extensions, hashmap from map-owned gchar ptr
   * (.extension, UTF-8, folded) -> to a GWin32AppInfoHandler
   * Extension can be used as a key in the extensions hashmap.
   */
  GHashTable *supported_exts;

  /* Icon of the application (remember, handler can have its own icon too) */
  GIcon *icon;

  /* Set to TRUE to prevent this app from appearing in lists of apps for
   * opening files. This will not prevent it from appearing in lists of apps
   * just for running, or lists of apps for opening exts/urls for which this
   * app reports explicit support.
   */
  gboolean no_open_with;

  /* Set to TRUE for applications from HKEY_CURRENT_USER.
   * Give them priority over applications from HKEY_LOCAL_MACHINE, when all
   * other things are equal.
   */
  gboolean user_specific;

  /* Set to TRUE for applications that are machine-wide defaults (i.e. default
   * browser) */
  gboolean default_app;
};

#define G_TYPE_WIN32_APPINFO_URL_SCHEMA           (g_win32_appinfo_url_schema_get_type ())
#define G_WIN32_APPINFO_URL_SCHEMA(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_WIN32_APPINFO_URL_SCHEMA, GWin32AppInfoURLSchema))

#define G_TYPE_WIN32_APPINFO_FILE_EXTENSION       (g_win32_appinfo_file_extension_get_type ())
#define G_WIN32_APPINFO_FILE_EXTENSION(obj)       (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_WIN32_APPINFO_FILE_EXTENSION, GWin32AppInfoFileExtension))

#define G_TYPE_WIN32_APPINFO_HANDLER              (g_win32_appinfo_handler_get_type ())
#define G_WIN32_APPINFO_HANDLER(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_WIN32_APPINFO_HANDLER, GWin32AppInfoHandler))

#define G_TYPE_WIN32_APPINFO_APPLICATION          (g_win32_appinfo_application_get_type ())
#define G_WIN32_APPINFO_APPLICATION(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_WIN32_APPINFO_APPLICATION, GWin32AppInfoApplication))

#define G_TYPE_WIN32_APPINFO_SHELL_VERB           (g_win32_appinfo_shell_verb_get_type ())
#define G_WIN32_APPINFO_SHELL_VERB(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_WIN32_APPINFO_SHELL_VERB, GWin32AppInfoShellVerb))

GType g_win32_appinfo_url_schema_get_type (void) G_GNUC_CONST;
GType g_win32_appinfo_file_extension_get_type (void) G_GNUC_CONST;
GType g_win32_appinfo_shell_verb_get_type (void) G_GNUC_CONST;
GType g_win32_appinfo_handler_get_type (void) G_GNUC_CONST;
GType g_win32_appinfo_application_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (GWin32AppInfoURLSchema, g_win32_appinfo_url_schema, G_TYPE_OBJECT)
G_DEFINE_TYPE (GWin32AppInfoFileExtension, g_win32_appinfo_file_extension, G_TYPE_OBJECT)
G_DEFINE_TYPE (GWin32AppInfoShellVerb, g_win32_appinfo_shell_verb, G_TYPE_OBJECT)
G_DEFINE_TYPE (GWin32AppInfoHandler, g_win32_appinfo_handler, G_TYPE_OBJECT)
G_DEFINE_TYPE (GWin32AppInfoApplication, g_win32_appinfo_application, G_TYPE_OBJECT)

static void
g_win32_appinfo_url_schema_dispose (GObject *object)
{
  GWin32AppInfoURLSchema *url = G_WIN32_APPINFO_URL_SCHEMA (object);

  g_clear_pointer (&url->schema, g_free);
  g_clear_pointer (&url->schema_u8, g_free);
  g_clear_pointer (&url->schema_u8_folded, g_free);
  g_clear_object (&url->chosen_handler);
  g_clear_pointer (&url->handlers, g_hash_table_destroy);
  G_OBJECT_CLASS (g_win32_appinfo_url_schema_parent_class)->dispose (object);
}


static void
g_win32_appinfo_handler_dispose (GObject *object)
{
  GWin32AppInfoHandler *handler = G_WIN32_APPINFO_HANDLER (object);

  g_clear_pointer (&handler->handler_id, g_free);
  g_clear_pointer (&handler->handler_id_folded, g_free);
  g_clear_object (&handler->key);
  g_clear_object (&handler->icon);
  g_clear_pointer (&handler->verbs, g_ptr_array_unref);
  G_OBJECT_CLASS (g_win32_appinfo_handler_parent_class)->dispose (object);
}

static void
g_win32_appinfo_file_extension_dispose (GObject *object)
{
  GWin32AppInfoFileExtension *ext = G_WIN32_APPINFO_FILE_EXTENSION (object);

  g_clear_pointer (&ext->extension, g_free);
  g_clear_pointer (&ext->extension_u8, g_free);
  g_clear_object (&ext->chosen_handler);
  g_clear_pointer (&ext->handlers, g_hash_table_destroy);
  G_OBJECT_CLASS (g_win32_appinfo_file_extension_parent_class)->dispose (object);
}

static void
g_win32_appinfo_shell_verb_dispose (GObject *object)
{
  GWin32AppInfoShellVerb *shverb = G_WIN32_APPINFO_SHELL_VERB (object);

  g_clear_pointer (&shverb->verb_name, g_free);
  g_clear_pointer (&shverb->verb_displayname, g_free);
  g_clear_pointer (&shverb->command, g_free);
  g_clear_pointer (&shverb->command_utf8, g_free);
  g_clear_pointer (&shverb->executable_folded, g_free);
  g_clear_pointer (&shverb->executable, g_free);
  g_clear_pointer (&shverb->dll_function, g_free);
  g_clear_object (&shverb->app);
  G_OBJECT_CLASS (g_win32_appinfo_shell_verb_parent_class)->dispose (object);
}

static void
g_win32_appinfo_application_dispose (GObject *object)
{
  GWin32AppInfoApplication *app = G_WIN32_APPINFO_APPLICATION (object);

  g_clear_pointer (&app->canonical_name_u8, g_free);
  g_clear_pointer (&app->canonical_name_folded, g_free);
  g_clear_pointer (&app->canonical_name, g_free);
  g_clear_pointer (&app->pretty_name, g_free);
  g_clear_pointer (&app->localized_pretty_name, g_free);
  g_clear_pointer (&app->description, g_free);
  g_clear_pointer (&app->pretty_name_u8, g_free);
  g_clear_pointer (&app->localized_pretty_name_u8, g_free);
  g_clear_pointer (&app->description_u8, g_free);
  g_clear_pointer (&app->supported_urls, g_hash_table_destroy);
  g_clear_pointer (&app->supported_exts, g_hash_table_destroy);
  g_clear_object (&app->icon);
  g_clear_pointer (&app->verbs, g_ptr_array_unref);
  G_OBJECT_CLASS (g_win32_appinfo_application_parent_class)->dispose (object);
}

static const gchar *
g_win32_appinfo_application_get_some_name (GWin32AppInfoApplication *app)
{
  if (app->localized_pretty_name_u8)
    return app->localized_pretty_name_u8;

  if (app->pretty_name_u8)
    return app->pretty_name_u8;

  return app->canonical_name_u8;
}

static void
g_win32_appinfo_url_schema_class_init (GWin32AppInfoURLSchemaClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = g_win32_appinfo_url_schema_dispose;
}

static void
g_win32_appinfo_file_extension_class_init (GWin32AppInfoFileExtensionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = g_win32_appinfo_file_extension_dispose;
}

static void
g_win32_appinfo_shell_verb_class_init (GWin32AppInfoShellVerbClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = g_win32_appinfo_shell_verb_dispose;
}

static void
g_win32_appinfo_handler_class_init (GWin32AppInfoHandlerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = g_win32_appinfo_handler_dispose;
}

static void
g_win32_appinfo_application_class_init (GWin32AppInfoApplicationClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = g_win32_appinfo_application_dispose;
}

static void
g_win32_appinfo_url_schema_init (GWin32AppInfoURLSchema *self)
{
  self->handlers = g_hash_table_new_full (g_str_hash,
                                          g_str_equal,
                                          g_free,
                                          g_object_unref);
}

static void
g_win32_appinfo_shell_verb_init (GWin32AppInfoShellVerb *self)
{
}

static void
g_win32_appinfo_file_extension_init (GWin32AppInfoFileExtension *self)
{
  self->handlers = g_hash_table_new_full (g_str_hash,
                                          g_str_equal,
                                          g_free,
                                          g_object_unref);
}

static void
g_win32_appinfo_handler_init (GWin32AppInfoHandler *self)
{
  self->verbs = g_ptr_array_new ();
  g_ptr_array_set_free_func (self->verbs, g_object_unref);
}

static void
g_win32_appinfo_application_init (GWin32AppInfoApplication *self)
{
  self->supported_urls = g_hash_table_new_full (g_str_hash,
                                                g_str_equal,
                                                g_free,
                                                g_object_unref);
  self->supported_exts = g_hash_table_new_full (g_str_hash,
                                                g_str_equal,
                                                g_free,
                                                g_object_unref);
  self->verbs = g_ptr_array_new ();
  g_ptr_array_set_free_func (self->verbs, g_object_unref);
}

G_LOCK_DEFINE_STATIC (gio_win32_appinfo);

/* Map of owned ".ext" (with '.', UTF-8, folded)
 * to GWin32AppInfoFileExtension ptr
 */
static GHashTable *extensions = NULL;

/* Map of owned "schema" (without ':', UTF-8, folded)
 * to GWin32AppInfoURLSchema ptr
 */
static GHashTable *urls = NULL;

/* Map of owned "appID" (UTF-8, folded) to
 * a GWin32AppInfoApplication
 */
static GHashTable *apps_by_id = NULL;

/* Map of owned "app.exe" (UTF-8, folded) to
 * a GWin32AppInfoApplication.
 * This map and its values are separate from apps_by_id. The fact that an app
 * with known ID has the same executable [base]name as an app in this map does
 * not mean that they are the same application.
 */
static GHashTable *apps_by_exe = NULL;

/* Map of owned "path:\to\app.exe" (UTF-8, folded) to
 * a GWin32AppInfoApplication.
 * The app objects in this map are fake - they are linked to
 * handlers that do not have any apps associated with them.
 */
static GHashTable *fake_apps = NULL;

/* Map of owned "handler id" (UTF-8, folded)
 * to a GWin32AppInfoHandler
 */
static GHashTable *handlers = NULL;

/* Watch this whole subtree */
static GWin32RegistryKey *url_associations_key;

/* Watch this whole subtree */
static GWin32RegistryKey *file_exts_key;

/* Watch this whole subtree */
static GWin32RegistryKey *user_clients_key;

/* Watch this whole subtree */
static GWin32RegistryKey *system_clients_key;

/* Watch this key */
static GWin32RegistryKey *user_registered_apps_key;

/* Watch this key */
static GWin32RegistryKey *system_registered_apps_key;

/* Watch this whole subtree */
static GWin32RegistryKey *applications_key;

/* Watch this key */
static GWin32RegistryKey *classes_root_key;

#define URL_ASSOCIATIONS L"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\"
#define USER_CHOICE L"\\UserChoice"
#define OPEN_WITH_PROGIDS L"\\OpenWithProgids"
#define FILE_EXTS L"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\"
#define HKCR L"HKEY_CLASSES_ROOT\\"
#define HKCU L"HKEY_CURRENT_USER\\"
#define HKLM L"HKEY_LOCAL_MACHINE\\"
#define REG_PATH_MAX 256
#define REG_PATH_MAX_SIZE (REG_PATH_MAX * sizeof (gunichar2))

/* for g_wcsdup(),
 *     _g_win32_extract_executable(),
 *     _g_win32_fixup_broken_microsoft_rundll_commandline()
 */
#include "giowin32-private.c"

static void
read_handler_icon (GWin32RegistryKey  *key,
                   GIcon             **icon_out)
{
  GWin32RegistryKey *icon_key;
  GWin32RegistryValueType default_type;
  gchar *default_value;

  g_assert (icon_out);

  *icon_out = NULL;

  icon_key = g_win32_registry_key_get_child_w (key, L"DefaultIcon", NULL);

  if (icon_key == NULL)
    return;

  if (g_win32_registry_key_get_value (icon_key,
                                      NULL,
                                      TRUE,
                                      "",
                                      &default_type,
                                      (gpointer *) &default_value,
                                      NULL,
                                      NULL))
    {
      if (default_type == G_WIN32_REGISTRY_VALUE_STR ||
          default_value[0] != '\0')
        *icon_out = g_themed_icon_new (default_value);

      g_clear_pointer (&default_value, g_free);
    }

  g_object_unref (icon_key);
}

static void
reg_verb_free (gpointer p)
{
  if (p == NULL)
    return;

  g_free (((reg_verb *) p)->name);
  g_free (((reg_verb *) p)->shellpath);
  g_free (p);
}

#define is_open(x) ( \
  ((x)[0] == L'o' || (x)[0] == L'O') && \
  ((x)[1] == L'p' || (x)[1] == L'P') && \
  ((x)[2] == L'e' || (x)[2] == L'E') && \
  ((x)[3] == L'n' || (x)[3] == L'N') \
)

/* default verb (if any) comes first,
 * then "open", then the rest of the verbs
 * are sorted alphabetically
 */
static gint
compare_verbs (gconstpointer a,
               gconstpointer b,
               gpointer user_data)
{
  const reg_verb *ca = (const reg_verb *) a;
  const reg_verb *cb = (const reg_verb *) b;
  const gunichar2 *def = (const gunichar2 *) user_data;
  gboolean is_open_ca;
  gboolean is_open_cb;

  if (def != NULL)
    {
      if (_wcsicmp (ca->name, def) == 0)
        return 1;
      else if (_wcsicmp (cb->name, def) == 0)
        return -1;
    }

  is_open_ca = is_open (ca->name);
  is_open_cb = is_open (cb->name);

  if (is_open_ca && !is_open_cb)
    return 1;
  else if (is_open_ca && !is_open_cb)
    return -1;

  return _wcsicmp (ca->name, cb->name);
}

static gboolean build_registry_path (gunichar2 *output, gsize output_size, ...) G_GNUC_NULL_TERMINATED;
static gboolean build_registry_pathv (gunichar2 *output, gsize output_size, va_list components);

static GWin32RegistryKey *_g_win32_registry_key_build_and_new_w (GError **error, ...) G_GNUC_NULL_TERMINATED;

/* Called by process_verbs_commands.
 * @verb is a verb name
 * @command_line is the commandline of that verb
 * @command_line_utf8 is the UTF-8 version of @command_line
 * @verb_displayname is the prettier display name of the verb (might be NULL)
 * @verb_is_preferred is TRUE if the verb is the preferred one
 * @invent_new_verb_name is TRUE when the verb should be added
 *                       even if a verb with such
 *                       name already exists (in which case
 *                       a new name is invented), unless
 *                       the existing verb runs exactly the same
 *                       commandline.
 */
typedef void (*verb_command_func) (gpointer         handler_data1,
                                   gpointer         handler_data2,
                                   const gunichar2 *verb,
                                   const gunichar2 *command_line,
                                   const gchar     *command_line_utf8,
                                   const gchar     *verb_displayname,
                                   gboolean         verb_is_preferred,
                                   gboolean         invent_new_verb_name);

static gunichar2 *                 decide_which_id_to_use (const gunichar2    *program_id,
                                                           GWin32RegistryKey **return_key,
                                                           gchar             **return_handler_id_u8,
                                                           gchar             **return_handler_id_u8_folded);

static GWin32AppInfoURLSchema *    get_schema_object      (const gunichar2 *schema,
                                                           const gchar     *schema_u8,
                                                           const gchar     *schema_u8_folded);

static GWin32AppInfoHandler *      get_handler_object     (const gchar       *handler_id_u8_folded,
                                                           GWin32RegistryKey *handler_key,
                                                           const gunichar2   *handler_id);

static GWin32AppInfoFileExtension *get_ext_object         (const gunichar2 *ext,
                                                           const gchar     *ext_u8,
                                                           const gchar     *ext_u8_folded);


static void                        process_verbs_commands (GList             *verbs,
                                                           const reg_verb    *preferred_verb,
                                                           const gunichar2   *path_to_progid,
                                                           const gunichar2   *progid,
                                                           gboolean           autoprefer_first_verb,
                                                           verb_command_func  handler,
                                                           gpointer           handler_data1,
                                                           gpointer           handler_data2);

static void                        handler_add_verb       (gpointer           handler_data1,
                                                           gpointer           handler_data2,
                                                           const gunichar2   *verb,
                                                           const gunichar2   *command_line,
                                                           const gchar       *command_line_utf8,
                                                           const gchar       *verb_displayname,
                                                           gboolean           verb_is_preferred,
                                                           gboolean           invent_new_verb_name);

/* output_size is in *bytes*, not gunichar2s! */
static gboolean
build_registry_path (gunichar2 *output, gsize output_size, ...)
{
  va_list ap;
  gboolean result;

  va_start (ap, output_size);

  result = build_registry_pathv (output, output_size, ap);

  va_end (ap);

  return result;
}

/* output_size is in *bytes*, not gunichar2s! */
static gboolean
build_registry_pathv (gunichar2 *output, gsize output_size, va_list components)
{
  va_list lentest;
  gunichar2 *p;
  gunichar2 *component;
  gsize length;

  if (output == NULL)
    return FALSE;

  G_VA_COPY (lentest, components);

  for (length = 0, component = va_arg (lentest, gunichar2 *);
       component != NULL;
       component = va_arg (lentest, gunichar2 *))
    {
      length += wcslen (component);
    }

  va_end (lentest);

  if ((length >= REG_PATH_MAX_SIZE) ||
      (length * sizeof (gunichar2) >= output_size))
    return FALSE;

  output[0] = L'\0';

  for (p = output, component = va_arg (components, gunichar2 *);
       component != NULL;
       component = va_arg (components, gunichar2 *))
    {
      length = wcslen (component);
      wcscat (p, component);
      p += length;
    }

  return TRUE;
}


static GWin32RegistryKey *
_g_win32_registry_key_build_and_new_w (GError **error, ...)
{
  va_list ap;
  gunichar2 key_path[REG_PATH_MAX_SIZE + 1];
  GWin32RegistryKey *key;

  va_start (ap, error);

  key = NULL;

  if (build_registry_pathv (key_path, sizeof (key_path), ap))
    key = g_win32_registry_key_new_w (key_path, error);

  va_end (ap);

  return key;
}

/* Gets the list of shell verbs (a GList of reg_verb, put into @verbs)
 * from the @program_id_key.
 * If one of the verbs should be preferred,
 * a pointer to this verb (in the GList) will be
 * put into @preferred_verb.
 * Does not automatically assume that the first verb
 * is preferred (when no other preferences exist).
 * @verbname_prefix is prefixed to the name of the verb
 * (this is used for subcommands) and is initially an
 * empty string.
 * Returns TRUE on success, FALSE on failure.
 */
static gboolean
get_verbs (GWin32RegistryKey  *program_id_key,
           const reg_verb    **preferred_verb,
           GList             **verbs,
           const gunichar2    *verbname_prefix,
           const gunichar2    *verbshell_prefix)
{
  GWin32RegistrySubkeyIter iter;
  GWin32RegistryKey *key;
  GWin32RegistryValueType val_type;
  gunichar2 *default_verb;
  gsize shellprefix_len;
  gsize nameprefix_len;
  GList *i;

  g_assert (program_id_key && verbs && preferred_verb);

  *verbs = NULL;
  *preferred_verb = NULL;

  key = g_win32_registry_key_get_child_w (program_id_key,
                                          verbshell_prefix,
                                          NULL);

  if (key == NULL)
    return FALSE;

  if (!g_win32_registry_subkey_iter_init (&iter, key, NULL))
    {
      g_object_unref (key);

      return FALSE;
    }

  shellprefix_len = g_utf16_len (verbshell_prefix);
  nameprefix_len = g_utf16_len (verbname_prefix);

  while (g_win32_registry_subkey_iter_next (&iter, TRUE, NULL))
    {
      gunichar2 *name;
      gsize name_len;
      GWin32RegistryKey *subkey;
      gboolean has_subcommands;
      const reg_verb *tmp;
      GWin32RegistryValueType subc_type;
      reg_verb *rverb;
      const gunichar2 *shell = L"Shell";
      const gsize shell_len = g_utf16_len (shell);

      if (!g_win32_registry_subkey_iter_get_name_w (&iter, &name, &name_len, NULL))
        continue;

      subkey = g_win32_registry_key_get_child_w (key,
                                                 name,
                                                 NULL);

      g_assert (subkey != NULL);
      has_subcommands = FALSE;
      if (g_win32_registry_key_get_value_w (subkey,
                                            NULL,
                                            TRUE,
                                            L"Subcommands",
                                            &subc_type,
                                            NULL,
                                            NULL,
                                            NULL) &&
          subc_type == G_WIN32_REGISTRY_VALUE_STR)
        {
          gunichar2 *new_nameprefix = g_malloc ((nameprefix_len + name_len + 1 + 1) * sizeof (gunichar2));
          gunichar2 *new_shellprefix = g_malloc ((shellprefix_len + 1 + name_len + 1 + shell_len + 1) * sizeof (gunichar2));
          memcpy (&new_shellprefix[0], verbshell_prefix, shellprefix_len * sizeof (gunichar2));
          memcpy (&new_shellprefix[shellprefix_len], L"\\", sizeof (gunichar2));
          memcpy (&new_shellprefix[shellprefix_len + 1], name, name_len * sizeof (gunichar2));
          memcpy (&new_shellprefix[shellprefix_len + 1 + name_len], L"\\", sizeof (gunichar2));
          memcpy (&new_shellprefix[shellprefix_len + 1 + name_len + 1], shell, (shell_len + 1) * sizeof (gunichar2));

          memcpy (&new_nameprefix[0], verbname_prefix, nameprefix_len * sizeof (gunichar2));
          memcpy (&new_nameprefix[nameprefix_len], name, (name_len) * sizeof (gunichar2));
          memcpy (&new_nameprefix[nameprefix_len + name_len], L"\\", (1 + 1) * sizeof (gunichar2));
          has_subcommands = get_verbs (program_id_key, &tmp, verbs, new_nameprefix, new_shellprefix);
          g_free (new_shellprefix);
          g_free (new_nameprefix);
        }

      g_clear_object (&subkey);

      if (has_subcommands)
        continue;

      rverb = g_new0 (reg_verb, 1);
      rverb->name = g_malloc ((nameprefix_len + name_len + 1) * sizeof (gunichar2));
      memcpy (&rverb->name[0], verbname_prefix, nameprefix_len * sizeof (gunichar2));
      memcpy (&rverb->name[nameprefix_len], name, (name_len + 1) * sizeof (gunichar2));
      rverb->shellpath = g_malloc ((shellprefix_len + 1 + name_len + 1) * sizeof (gunichar2));
      memcpy (&rverb->shellpath[0], verbshell_prefix, shellprefix_len * sizeof (gunichar2));
      memcpy (&rverb->shellpath[shellprefix_len], L"\\", sizeof (gunichar2));
      memcpy (&rverb->shellpath[shellprefix_len + 1], name, (name_len + 1) * sizeof (gunichar2));
      *verbs = g_list_append (*verbs, rverb);
    }

  g_win32_registry_subkey_iter_clear (&iter);

  if (*verbs == NULL)
    {
      g_object_unref (key);

      return FALSE;
    }

  default_verb = NULL;

  if (g_win32_registry_key_get_value_w (key,
                                        NULL,
                                        TRUE,
                                        L"",
                                        &val_type,
                                        (void **) &default_verb,
                                        NULL,
                                        NULL) &&
      (val_type != G_WIN32_REGISTRY_VALUE_STR ||
       g_utf16_len (default_verb) <= 0))
    g_clear_pointer (&default_verb, g_free);

  g_object_unref (key);

  /* Only sort at the top level */
  if (verbname_prefix[0] == 0)
    {
      *verbs = g_list_sort_with_data (*verbs, compare_verbs, default_verb);

      for (i = *verbs; default_verb && *preferred_verb == NULL && i; i = i->next)
        if (_wcsicmp (default_verb, ((const reg_verb *) i->data)->name) == 0)
          *preferred_verb = (const reg_verb *) i->data;
    }

  g_clear_pointer (&default_verb, g_free);

  return TRUE;
}

/* Grabs a URL association (from HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\
 * or from an application with Capabilities, or just a schema subkey in HKCR).
 * @program_id is a ProgID of the handler for the URL.
 * @schema is the schema for the URL.
 * @schema_u8 and @schema_u8_folded are UTF-8 and folded UTF-8
 * respectively.
 * @app is the app to which the URL handler belongs (can be NULL).
 * @is_user_choice is TRUE if this association is clearly preferred
 */
static void
get_url_association (const gunichar2          *program_id,
                     const gunichar2          *schema,
                     const gchar              *schema_u8,
                     const gchar              *schema_u8_folded,
                     GWin32AppInfoApplication *app,
                     gboolean                  is_user_choice)
{
  GWin32AppInfoURLSchema *schema_rec;
  GWin32AppInfoHandler *handler_rec;
  gunichar2 *handler_id;
  GList *verbs;
  const reg_verb *preferred_verb;
  gchar *handler_id_u8;
  gchar *handler_id_u8_folded;
  GWin32RegistryKey *handler_key;

  if ((handler_id = decide_which_id_to_use (program_id,
                                            &handler_key,
                                            &handler_id_u8,
                                            &handler_id_u8_folded)) == NULL)
    return;

  if (!get_verbs (handler_key, &preferred_verb, &verbs, L"", L"Shell"))
    {
      g_clear_pointer (&handler_id, g_free);
      g_clear_pointer (&handler_id_u8, g_free);
      g_clear_pointer (&handler_id_u8_folded, g_free);
      g_clear_object (&handler_key);

      return;
    }

  schema_rec = get_schema_object (schema,
                                  schema_u8,
                                  schema_u8_folded);

  handler_rec = get_handler_object (handler_id_u8_folded,
                                    handler_key,
                                    handler_id);

  if (is_user_choice || schema_rec->chosen_handler == NULL)
    g_set_object (&schema_rec->chosen_handler, handler_rec);

  g_hash_table_insert (schema_rec->handlers,
                       g_strdup (handler_id_u8_folded),
                       g_object_ref (handler_rec));

  g_clear_object (&handler_key);

  if (app)
    g_hash_table_insert (app->supported_urls,
                         g_strdup (schema_rec->schema_u8_folded),
                         g_object_ref (handler_rec));

  process_verbs_commands (verbs, /* consumes verbs */
                          preferred_verb,
                          HKCR,
                          handler_id,
                          TRUE,
                          handler_add_verb,
                          handler_rec,
                          app);

  g_clear_pointer (&handler_id_u8, g_free);
  g_clear_pointer (&handler_id_u8_folded, g_free);
  g_clear_pointer (&handler_id, g_free);
}

/* Grabs a file extension association (from HKCR\.ext or similar).
 * @program_id is a ProgID of the handler for the extension.
 * @file_extension is the extension (with the leading '.')
 * @app is the app to which the extension handler belongs (can be NULL).
 * @is_user_choice is TRUE if this is clearly the preferred association
 */
static void
get_file_ext (const gunichar2            *program_id,
              const gunichar2            *file_extension,
              GWin32AppInfoApplication   *app,
              gboolean                    is_user_choice)
{
  GWin32AppInfoHandler *handler_rec;
  gunichar2 *handler_id;
  const reg_verb *preferred_verb;
  GList *verbs;
  gchar *handler_id_u8;
  gchar *handler_id_u8_folded;
  GWin32RegistryKey *handler_key;
  GWin32AppInfoFileExtension *file_extn;
  gchar *file_extension_u8;
  gchar *file_extension_u8_folded;

  if ((handler_id = decide_which_id_to_use (program_id,
                                            &handler_key,
                                            &handler_id_u8,
                                            &handler_id_u8_folded)) == NULL)
    return;

  if (!g_utf16_to_utf8_and_fold (file_extension,
                                 -1,
                                 &file_extension_u8,
                                 &file_extension_u8_folded))
    {
      g_clear_pointer (&handler_id, g_free);
      g_clear_pointer (&handler_id_u8, g_free);
      g_clear_pointer (&handler_id_u8_folded, g_free);
      g_clear_object (&handler_key);

      return;
    }

  if (!get_verbs (handler_key, &preferred_verb, &verbs, L"", L"Shell"))
    {
      g_clear_pointer (&handler_id, g_free);
      g_clear_pointer (&handler_id_u8, g_free);
      g_clear_pointer (&handler_id_u8_folded, g_free);
      g_clear_object (&handler_key);
      g_clear_pointer (&file_extension_u8, g_free);
      g_clear_pointer (&file_extension_u8_folded, g_free);

      return;
    }

  file_extn = get_ext_object (file_extension, file_extension_u8, file_extension_u8_folded);

  handler_rec = get_handler_object (handler_id_u8_folded,
                                    handler_key,
                                    handler_id);

  if (is_user_choice || file_extn->chosen_handler == NULL)
    g_set_object (&file_extn->chosen_handler, handler_rec);

  g_hash_table_insert (file_extn->handlers,
                       g_strdup (handler_id_u8_folded),
                       g_object_ref (handler_rec));

  if (app)
    g_hash_table_insert (app->supported_exts,
                         g_strdup (file_extension_u8_folded),
                         g_object_ref (handler_rec));

  g_clear_pointer (&file_extension_u8, g_free);
  g_clear_pointer (&file_extension_u8_folded, g_free);
  g_clear_object (&handler_key);

  process_verbs_commands (verbs, /* consumes verbs */
                          preferred_verb,
                          HKCR,
                          handler_id,
                          TRUE,
                          handler_add_verb,
                          handler_rec,
                          app);

  g_clear_pointer (&handler_id, g_free);
  g_clear_pointer (&handler_id_u8, g_free);
  g_clear_pointer (&handler_id_u8_folded, g_free);
}

/* Returns either a @program_id or the string from
 * the default value of the program_id key (which is a name
 * of a proxy class), or NULL.
 * Does not check that proxy represents a valid
 * record, just checks that it exists.
 * Can return the class key (HKCR/program_id or HKCR/proxy_id).
 * Can convert returned value to UTF-8 and fold it.
 */
static gunichar2 *
decide_which_id_to_use (const gunichar2    *program_id,
                        GWin32RegistryKey **return_key,
                        gchar             **return_handler_id_u8,
                        gchar             **return_handler_id_u8_folded)
{
  GWin32RegistryKey *key;
  GWin32RegistryValueType val_type;
  gunichar2 *proxy_id;
  const gunichar2 *return_id;
  gboolean got_value;
  gchar *handler_id_u8;
  gchar *handler_id_u8_folded;
  g_assert (program_id);

  if (return_key)
    *return_key = NULL;

  /* Check the proxy first */
  key = g_win32_registry_key_get_child_w (classes_root_key, program_id, NULL);

  if (key == NULL)
    return NULL;

  proxy_id = NULL;
  got_value = g_win32_registry_key_get_value_w (key,
                                                NULL,
                                                TRUE,
                                                L"",
                                                &val_type,
                                                (void **) &proxy_id,
                                                NULL,
                                                NULL);
  if (got_value && val_type != G_WIN32_REGISTRY_VALUE_STR)
    g_clear_pointer (&proxy_id, g_free);

  return_id = program_id;

  if (proxy_id)
    {
      GWin32RegistryKey *proxy_key;
      proxy_key = g_win32_registry_key_get_child_w (classes_root_key, proxy_id, NULL);

      if (proxy_key)
        {
          if (return_key)
            *return_key = proxy_key;
          else
            g_object_unref (proxy_key);

          return_id = proxy_id;
        }
      else
        g_clear_pointer (&proxy_id, g_free);
    }

  if ((return_handler_id_u8 ||
       return_handler_id_u8_folded) &&
      !g_utf16_to_utf8_and_fold (return_id,
                                 -1,
                                 &handler_id_u8,
                                 &handler_id_u8_folded))
    {
      g_clear_object (&key);
      if (return_key)
        g_clear_object (return_key);
      g_clear_pointer (&proxy_id, g_free);

      return NULL;
    }

  if (return_handler_id_u8)
    *return_handler_id_u8 = g_steal_pointer (&handler_id_u8);
  if (return_handler_id_u8_folded)
    *return_handler_id_u8_folded = g_steal_pointer (&handler_id_u8_folded);

  if (proxy_id == NULL && return_key)
    *return_key = key;
  else
    g_clear_object (&key);

  if (proxy_id == NULL)
    return g_wcsdup (program_id, -1);

  return proxy_id;
}

/* Grabs the command for each verb from @verbs,
 * and invokes @handler for it. Consumes @verbs.
 * @path_to_progid and @progid are concatenated to
 * produce a path to the key where Shell/verb/command
 * subkeys are looked up.
 * @preferred_verb, if not NULL, will be used to inform
 * the @handler that a verb is preferred.
 * @autoprefer_first_verb will automatically make the first
 * verb to be preferred, if @preferred_verb is NULL.
 * @handler_data1 and @handler_data2 are passed to @handler as-is.
 */
static void
process_verbs_commands (GList             *verbs,
                        const reg_verb    *preferred_verb,
                        const gunichar2   *path_to_progid,
                        const gunichar2   *progid,
                        gboolean           autoprefer_first_verb,
                        verb_command_func  handler,
                        gpointer           handler_data1,
                        gpointer           handler_data2)
{
  GList *i;
  gboolean got_value;

  g_assert (handler != NULL);
  g_assert (verbs != NULL);
  g_assert (progid != NULL);

  for (i = verbs; i; i = i->next)
    {
      const reg_verb *verb = (const reg_verb *) i->data;
      GWin32RegistryKey *key;
      GWin32RegistryKey *verb_key;
      gunichar2 *command_value;
      gchar *command_value_utf8;
      GWin32RegistryValueType val_type;
      gunichar2 *verb_displayname;
      gchar *verb_displayname_u8;

      key = _g_win32_registry_key_build_and_new_w (NULL, path_to_progid, progid,
                                                   L"\\", verb->shellpath, L"\\command", NULL);

      if (key == NULL)
        {
          g_debug ("%S%S\\shell\\%S does not have a \"command\" subkey",
                   path_to_progid, progid, verb->shellpath);
          continue;
        }

      command_value = NULL;
      got_value = g_win32_registry_key_get_value_w (key,
                                                    NULL,
                                                    TRUE,
                                                    L"",
                                                    &val_type,
                                                    (void **) &command_value,
                                                    NULL,
                                                    NULL);
      g_clear_object (&key);

      if (!got_value ||
          val_type != G_WIN32_REGISTRY_VALUE_STR ||
          (command_value_utf8 = g_utf16_to_utf8 (command_value,
                                                 -1,
                                                 NULL,
                                                 NULL,
                                                 NULL)) == NULL)
        {
          g_clear_pointer (&command_value, g_free);
          continue;
        }

      verb_displayname = NULL;
      verb_displayname_u8 = NULL;
      verb_key = _g_win32_registry_key_build_and_new_w (NULL, path_to_progid, progid,
                                                        L"\\", verb->shellpath, NULL);

      if (verb_key)
        {
          gsize verb_displayname_len;

          got_value = g_win32_registry_key_get_value_w (verb_key,
                                                        g_win32_registry_get_os_dirs_w (),
                                                        TRUE,
                                                        L"MUIVerb",
                                                        &val_type,
                                                        (void **) &verb_displayname,
                                                        &verb_displayname_len,
                                                        NULL);

          if (got_value &&
              val_type == G_WIN32_REGISTRY_VALUE_STR &&
              verb_displayname_len > sizeof (gunichar2))
            verb_displayname_u8 = g_utf16_to_utf8 (verb_displayname, -1, NULL, NULL, NULL);

          g_clear_pointer (&verb_displayname, g_free);

          if (verb_displayname_u8 == NULL)
            {
              got_value = g_win32_registry_key_get_value_w (verb_key,
                                                            NULL,
                                                            TRUE,
                                                            L"",
                                                            &val_type,
                                                            (void **) &verb_displayname,
                                                            &verb_displayname_len,
                                                            NULL);

              if (got_value &&
                  val_type == G_WIN32_REGISTRY_VALUE_STR &&
                  verb_displayname_len > sizeof (gunichar2))
                verb_displayname_u8 = g_utf16_to_utf8 (verb_displayname, -1, NULL, NULL, NULL);
            }

          g_clear_pointer (&verb_displayname, g_free);
          g_clear_object (&verb_key);
        }

      handler (handler_data1, handler_data2, verb->name, command_value, command_value_utf8,
               verb_displayname_u8,
               (preferred_verb && _wcsicmp (verb->name, preferred_verb->name) == 0) ||
               (!preferred_verb && autoprefer_first_verb && i == verbs),
               FALSE);

      g_clear_pointer (&command_value, g_free);
      g_clear_pointer (&command_value_utf8, g_free);
      g_clear_pointer (&verb_displayname_u8, g_free);
    }

  g_list_free_full (verbs, reg_verb_free);
}

/* Looks up a schema object identified by
 * @shema_u8_folded in the urls hash table.
 * If such object doesn't exist,
 * creates it and puts it into the urls hash table.
 * Returns the object.
 */
static GWin32AppInfoURLSchema *
get_schema_object (const gunichar2 *schema,
                   const gchar     *schema_u8,
                   const gchar     *schema_u8_folded)
{
  GWin32AppInfoURLSchema *schema_rec;

  schema_rec = g_hash_table_lookup (urls, schema_u8_folded);

  if (schema_rec != NULL)
    return schema_rec;

  schema_rec = g_object_new (G_TYPE_WIN32_APPINFO_URL_SCHEMA, NULL);
  schema_rec->schema = g_wcsdup (schema, -1);
  schema_rec->schema_u8 = g_strdup (schema_u8);
  schema_rec->schema_u8_folded = g_strdup (schema_u8_folded);
  g_hash_table_insert (urls, g_strdup (schema_rec->schema_u8_folded), schema_rec);

  return schema_rec;
}

/* Looks up a handler object identified by
 * @handler_id_u8_folded in the handlers hash table.
 * If such object doesn't exist,
 * creates it and puts it into the handlers hash table.
 * Returns the object.
 */
static GWin32AppInfoHandler *
get_handler_object (const gchar       *handler_id_u8_folded,
                    GWin32RegistryKey *handler_key,
                    const gunichar2   *handler_id)
{
  GWin32AppInfoHandler *handler_rec;

  handler_rec = g_hash_table_lookup (handlers, handler_id_u8_folded);

  if (handler_rec != NULL)
    return handler_rec;

  handler_rec = g_object_new (G_TYPE_WIN32_APPINFO_HANDLER, NULL);
  handler_rec->key = g_object_ref (handler_key);
  handler_rec->handler_id = g_wcsdup (handler_id, -1);
  handler_rec->handler_id_folded = g_strdup (handler_id_u8_folded);
  read_handler_icon (handler_key, &handler_rec->icon);
  g_hash_table_insert (handlers, g_strdup (handler_id_u8_folded), handler_rec);

  return handler_rec;
}

static void
handler_add_verb (gpointer           handler_data1,
                  gpointer           handler_data2,
                  const gunichar2   *verb,
                  const gunichar2   *command_line,
                  const gchar       *command_line_utf8,
                  const gchar       *verb_displayname,
                  gboolean           verb_is_preferred,
                  gboolean           invent_new_verb_name)
{
  GWin32AppInfoHandler *handler_rec = (GWin32AppInfoHandler *) handler_data1;
  GWin32AppInfoApplication *app_rec = (GWin32AppInfoApplication *) handler_data2;
  GWin32AppInfoShellVerb *shverb;

  _verb_lookup (handler_rec->verbs, verb, &shverb);

  if (shverb != NULL)
    return;

  shverb = g_object_new (G_TYPE_WIN32_APPINFO_SHELL_VERB, NULL);
  shverb->verb_name = g_wcsdup (verb, -1);
  shverb->verb_displayname = g_strdup (verb_displayname);
  shverb->command = g_wcsdup (command_line, -1);
  shverb->command_utf8 = g_strdup (command_line_utf8);
  if (app_rec)
    shverb->app = g_object_ref (app_rec);

  _g_win32_extract_executable (shverb->command,
                               &shverb->executable,
                               &shverb->executable_basename,
                               &shverb->executable_folded,
                               NULL,
                               &shverb->dll_function);

  if (shverb->dll_function != NULL)
    _g_win32_fixup_broken_microsoft_rundll_commandline (shverb->command);

  if (!verb_is_preferred)
    g_ptr_array_add (handler_rec->verbs, shverb);
  else
    g_ptr_array_insert (handler_rec->verbs, 0, shverb);

}

/* Tries to generate a new name for a verb that looks
 * like "verb (%x)", where %x is an integer in range of [0;255).
 * On success puts new verb (and new verb displayname) into
 * @new_verb and @new_displayname and return TRUE.
 * On failure puts NULL into both and returns FALSE.
 */
static gboolean
generate_new_verb_name (GPtrArray        *verbs,
                        const gunichar2  *verb,
                        const gchar      *verb_displayname,
                        gunichar2       **new_verb,
                        gchar           **new_displayname)
{
  gsize counter;
  GWin32AppInfoShellVerb *shverb;
  gsize orig_len = g_utf16_len (verb);
  gsize new_verb_name_len = orig_len + strlen (" ()") + 2 + 1;
  gunichar2 *new_verb_name = g_malloc (new_verb_name_len * sizeof (gunichar2));

  *new_verb = NULL;
  *new_displayname = NULL;

  memcpy (new_verb_name, verb, orig_len * sizeof (gunichar2));
  for (counter = 0; counter < 255; counter++)
  {
    _snwprintf (&new_verb_name[orig_len], new_verb_name_len, L" (%x)", counter);
    _verb_lookup (verbs, new_verb_name, &shverb);

    if (shverb == NULL)
      {
        *new_verb = new_verb_name;
        if (verb_displayname != NULL)
          *new_displayname = g_strdup_printf ("%s (%x)", verb_displayname, counter);

        return TRUE;
      }
  }

  return FALSE;
}

static void
app_add_verb (gpointer           handler_data1,
              gpointer           handler_data2,
              const gunichar2   *verb,
              const gunichar2   *command_line,
              const gchar       *command_line_utf8,
              const gchar       *verb_displayname,
              gboolean           verb_is_preferred,
              gboolean           invent_new_verb_name)
{
  gunichar2 *new_verb = NULL;
  gchar *new_displayname = NULL;
  GWin32AppInfoApplication *app_rec = (GWin32AppInfoApplication *) handler_data2;
  GWin32AppInfoShellVerb *shverb;

  _verb_lookup (app_rec->verbs, verb, &shverb);

  /* Special logic for fake apps - do our best to
   * collate all possible verbs in the app,
   * including the verbs that have the same name but
   * different commandlines, in which case a new
   * verb name has to be invented.
   */
  if (shverb != NULL)
    {
      gsize vi;

      if (!invent_new_verb_name)
        return;

      for (vi = 0; vi < app_rec->verbs->len; vi++)
        {
          GWin32AppInfoShellVerb *app_verb;

          app_verb = _verb_idx (app_rec->verbs, vi);

          if (_wcsicmp (command_line, app_verb->command) == 0)
            break;
        }

      if (vi < app_rec->verbs->len ||
          !generate_new_verb_name (app_rec->verbs,
                                   verb,
                                   verb_displayname,
                                   &new_verb,
                                   &new_displayname))
        return;
    }

  shverb = g_object_new (G_TYPE_WIN32_APPINFO_SHELL_VERB, NULL);
  if (new_verb == NULL)
    shverb->verb_name = g_wcsdup (verb, -1);
  else
    shverb->verb_name = new_verb;
  if (new_displayname == NULL)
    shverb->verb_displayname = g_strdup (verb_displayname);
  else
    shverb->verb_displayname = new_displayname;

  shverb->command = g_wcsdup (command_line, -1);
  shverb->command_utf8 = g_strdup (command_line_utf8);
  shverb->app = g_object_ref (app_rec);

  _g_win32_extract_executable (shverb->command,
                               &shverb->executable,
                               &shverb->executable_basename,
                               &shverb->executable_folded,
                               NULL,
                               &shverb->dll_function);

  if (shverb->dll_function != NULL)
    _g_win32_fixup_broken_microsoft_rundll_commandline (shverb->command);

  if (!verb_is_preferred)
    g_ptr_array_add (app_rec->verbs, shverb);
  else
    g_ptr_array_insert (app_rec->verbs, 0, shverb);
}

/* Looks up a file extnsion object identified by
 * @ext_u8_folded in the extensions hash table.
 * If such object doesn't exist,
 * creates it and puts it into the extensions hash table.
 * Returns the object.
 */
static GWin32AppInfoFileExtension *
get_ext_object (const gunichar2 *ext,
                const gchar     *ext_u8,
                const gchar     *ext_u8_folded)
{
  GWin32AppInfoFileExtension *file_extn;

  if (g_hash_table_lookup_extended (extensions,
                                    ext_u8_folded,
                                    NULL,
                                    (void **) &file_extn))
    return file_extn;

  file_extn = g_object_new (G_TYPE_WIN32_APPINFO_FILE_EXTENSION, NULL);
  file_extn->extension = g_wcsdup (ext, -1);
  file_extn->extension_u8 = g_strdup (ext_u8);
  g_hash_table_insert (extensions, g_strdup (ext_u8_folded), file_extn);

  return file_extn;
}

/* Iterates over HKCU\\Software\\Clients or HKLM\\Software\\Clients,
 * (depending on @user_registry being TRUE or FALSE),
 * collecting applications listed there.
 * Puts the path to the client key for each client into @priority_capable_apps
 * (only for clients with file or URL associations).
 */
static void
collect_capable_apps_from_clients (GPtrArray *capable_apps,
                                   GPtrArray *priority_capable_apps,
                                   gboolean   user_registry)
{
  GWin32RegistryKey *clients;
  GWin32RegistrySubkeyIter clients_iter;

  gunichar2 *client_type_name;
  gsize client_type_name_len;


  if (user_registry)
    clients =
        g_win32_registry_key_new_w (L"HKEY_CURRENT_USER\\Software\\Clients",
                                     NULL);
  else
    clients =
        g_win32_registry_key_new_w (L"HKEY_LOCAL_MACHINE\\Software\\Clients",
                                     NULL);

  if (clients == NULL)
    return;

  if (!g_win32_registry_subkey_iter_init (&clients_iter, clients, NULL))
    {
      g_object_unref (clients);
      return;
    }

  while (g_win32_registry_subkey_iter_next (&clients_iter, TRUE, NULL))
    {
      GWin32RegistrySubkeyIter subkey_iter;
      GWin32RegistryKey *system_client_type;
      GWin32RegistryValueType default_type;
      gunichar2 *default_value = NULL;
      gunichar2 *client_name;
      gsize client_name_len;

      if (!g_win32_registry_subkey_iter_get_name_w (&clients_iter,
                                                    &client_type_name,
                                                    &client_type_name_len,
                                                    NULL))
        continue;

      system_client_type = g_win32_registry_key_get_child_w (clients,
                                                             client_type_name,
                                                             NULL);

      if (system_client_type == NULL)
        continue;

      if (g_win32_registry_key_get_value_w (system_client_type,
                                            NULL,
                                            TRUE,
                                            L"",
                                            &default_type,
                                            (gpointer *) &default_value,
                                            NULL,
                                            NULL))
        {
          if (default_type != G_WIN32_REGISTRY_VALUE_STR ||
              default_value[0] == L'\0')
            g_clear_pointer (&default_value, g_free);
        }

      if (!g_win32_registry_subkey_iter_init (&subkey_iter,
                                              system_client_type,
                                              NULL))
        {
          g_clear_pointer (&default_value, g_free);
          g_object_unref (system_client_type);
          continue;
        }

      while (g_win32_registry_subkey_iter_next (&subkey_iter, TRUE, NULL))
        {
          GWin32RegistryKey *system_client;
          GWin32RegistryKey *system_client_assoc;
          gboolean add;
          gunichar2 *keyname;

          if (!g_win32_registry_subkey_iter_get_name_w (&subkey_iter,
                                                        &client_name,
                                                        &client_name_len,
                                                        NULL))
            continue;

          system_client = g_win32_registry_key_get_child_w (system_client_type,
                                                            client_name,
                                                            NULL);

          if (system_client == NULL)
            continue;

          add = FALSE;

          system_client_assoc = g_win32_registry_key_get_child_w (system_client,
                                                                  L"Capabilities\\FileAssociations",
                                                                  NULL);

          if (system_client_assoc != NULL)
            {
              add = TRUE;
              g_object_unref (system_client_assoc);
            }
          else
            {
              system_client_assoc = g_win32_registry_key_get_child_w (system_client,
                                                                      L"Capabilities\\UrlAssociations",
                                                                      NULL);

              if (system_client_assoc != NULL)
                {
                  add = TRUE;
                  g_object_unref (system_client_assoc);
                }
            }

          if (add)
            {
              keyname = g_wcsdup (g_win32_registry_key_get_path_w (system_client), -1);

              if (default_value && wcscmp (default_value, client_name) == 0)
                g_ptr_array_add (priority_capable_apps, keyname);
              else
                g_ptr_array_add (capable_apps, keyname);
            }

          g_object_unref (system_client);
        }

      g_win32_registry_subkey_iter_clear (&subkey_iter);
      g_clear_pointer (&default_value, g_free);
      g_object_unref (system_client_type);
    }

  g_win32_registry_subkey_iter_clear (&clients_iter);
  g_object_unref (clients);
}

/* Iterates over HKCU\\Software\\RegisteredApplications or HKLM\\Software\\RegisteredApplications,
 * (depending on @user_registry being TRUE or FALSE),
 * collecting applications listed there.
 * Puts the path to the app key for each app into @capable_apps.
 */
static void
collect_capable_apps_from_registered_apps (GPtrArray *capable_apps,
                                           gboolean   user_registry)
{
  GWin32RegistryValueIter iter;
  const gunichar2 *reg_path;

  gunichar2 *value_data;
  gsize      value_data_size;
  GWin32RegistryValueType value_type;
  GWin32RegistryKey *registered_apps;

  if (user_registry)
    reg_path = L"HKEY_CURRENT_USER\\Software\\RegisteredApplications";
  else
    reg_path = L"HKEY_LOCAL_MACHINE\\Software\\RegisteredApplications";

  registered_apps =
      g_win32_registry_key_new_w (reg_path, NULL);

  if (!registered_apps)
    return;

  if (!g_win32_registry_value_iter_init (&iter, registered_apps, NULL))
    {
      g_object_unref (registered_apps);

      return;
    }

  while (g_win32_registry_value_iter_next (&iter, TRUE, NULL))
    {
      gunichar2 possible_location[REG_PATH_MAX_SIZE + 1];
      GWin32RegistryKey *location;
      gunichar2 *p;

      if ((!g_win32_registry_value_iter_get_value_type (&iter,
                                                        &value_type,
                                                        NULL)) ||
          (value_type != G_WIN32_REGISTRY_VALUE_STR) ||
          (!g_win32_registry_value_iter_get_data_w (&iter, TRUE,
                                                    (void **) &value_data,
                                                    &value_data_size,
                                                    NULL)) ||
          (value_data_size < sizeof (gunichar2)) ||
          (value_data[0] == L'\0'))
        continue;

      if (!build_registry_path (possible_location, sizeof (possible_location),
                                user_registry ? HKCU : HKLM, value_data, NULL))
        continue;

      location = g_win32_registry_key_new_w (possible_location, NULL);

      if (location == NULL)
        continue;

      p = wcsrchr (possible_location, L'\\');

      if (p)
        {
          *p = L'\0';
          g_ptr_array_add (capable_apps, g_wcsdup (possible_location, -1));
        }

      g_object_unref (location);
    }

  g_win32_registry_value_iter_clear (&iter);
  g_object_unref (registered_apps);
}

/* Looks up an app object identified by
 * @canonical_name_folded in the @app_hashmap.
 * If such object doesn't exist,
 * creates it and puts it into the @app_hashmap.
 * Returns the object.
 */
static GWin32AppInfoApplication *
get_app_object (GHashTable      *app_hashmap,
                const gunichar2 *canonical_name,
                const gchar     *canonical_name_u8,
                const gchar     *canonical_name_folded,
                gboolean         user_specific,
                gboolean         default_app)
{
  GWin32AppInfoApplication *app;

  app = g_hash_table_lookup (app_hashmap, canonical_name_folded);

  if (app != NULL)
    return app;

  app = g_object_new (G_TYPE_WIN32_APPINFO_APPLICATION, NULL);
  app->canonical_name = g_wcsdup (canonical_name, -1);
  app->canonical_name_u8 = g_strdup (canonical_name_u8);
  app->canonical_name_folded = g_strdup (canonical_name_folded);
  app->no_open_with = FALSE;
  app->user_specific = user_specific;
  app->default_app = default_app;
  g_hash_table_insert (app_hashmap,
                       g_strdup (canonical_name_folded),
                       app);

  return app;
}

/* Grabs an application that has Capabilities.
 * @app_key_path is the path to the application key
 * (which must have a "Capabilities" subkey).
 * @default_app is TRUE if the app has priority
 */
static void
read_capable_app (const gunichar2 *app_key_path,
                  gboolean         user_specific,
                  gboolean         default_app)
{
  GWin32AppInfoApplication *app;
  gchar *canonical_name_u8 = NULL;
  gchar *canonical_name_folded = NULL;
  gchar *app_key_path_u8 = NULL;
  gchar *app_key_path_u8_folded = NULL;
  GWin32RegistryKey *appkey = NULL;
  gunichar2 *fallback_friendly_name;
  GWin32RegistryValueType vtype;
  gboolean success;
  gunichar2 *friendly_name;
  gunichar2 *description;
  gunichar2 *narrow_application_name;
  gunichar2 *icon_source;
  GWin32RegistryKey *capabilities;
  GWin32RegistryKey *default_icon_key;
  GWin32RegistryKey *associations;
  const reg_verb *preferred_verb;
  GList *verbs = NULL;

  appkey = NULL;
  capabilities = NULL;

  if (!g_utf16_to_utf8_and_fold (app_key_path,
                                 -1,
                                 &canonical_name_u8,
                                 &canonical_name_folded) ||
      !g_utf16_to_utf8_and_fold (app_key_path,
                                 -1,
                                 &app_key_path_u8,
                                 &app_key_path_u8_folded) ||
      (appkey = g_win32_registry_key_new_w (app_key_path, NULL)) == NULL ||
      (capabilities = g_win32_registry_key_get_child_w (appkey, L"Capabilities", NULL)) == NULL ||
      !get_verbs (capabilities, &preferred_verb, &verbs, L"", L"Shell"))
    {
      g_clear_pointer (&canonical_name_u8, g_free);
      g_clear_pointer (&canonical_name_folded, g_free);
      g_clear_object (&appkey);
      g_clear_pointer (&app_key_path_u8, g_free);
      g_clear_pointer (&app_key_path_u8_folded, g_free);

      return;
    }

  app = get_app_object (apps_by_id,
                        app_key_path,
                        canonical_name_u8,
                        canonical_name_folded,
                        user_specific,
                        default_app);

  process_verbs_commands (verbs, /* consumes verbs */
                          preferred_verb,
                          L"", /* [ab]use the fact that two strings are simply concatenated */
                          g_win32_registry_key_get_path_w (capabilities),
                          FALSE,
                          app_add_verb,
                          app,
                          app);

  fallback_friendly_name = NULL;
  success = g_win32_registry_key_get_value_w (appkey,
                                              NULL,
                                              TRUE,
                                              L"",
                                              &vtype,
                                              (void **) &fallback_friendly_name,
                                              NULL,
                                              NULL);

  if (success && vtype != G_WIN32_REGISTRY_VALUE_STR)
    g_clear_pointer (&fallback_friendly_name, g_free);

  if (fallback_friendly_name &&
      app->pretty_name == NULL)
    {
      app->pretty_name = g_wcsdup (fallback_friendly_name, -1);
      g_clear_pointer (&app->pretty_name_u8, g_free);
      app->pretty_name_u8 = g_utf16_to_utf8 (fallback_friendly_name,
                                             -1,
                                             NULL,
                                             NULL,
                                             NULL);
    }

  friendly_name = NULL;
  success = g_win32_registry_key_get_value_w (capabilities,
                                              g_win32_registry_get_os_dirs_w (),
                                              TRUE,
                                              L"LocalizedString",
                                              &vtype,
                                              (void **) &friendly_name,
                                              NULL,
                                              NULL);

  if (success &&
      vtype != G_WIN32_REGISTRY_VALUE_STR)
    g_clear_pointer (&friendly_name, g_free);

  if (friendly_name &&
      app->localized_pretty_name == NULL)
    {
      app->localized_pretty_name = g_wcsdup (friendly_name, -1);
      g_clear_pointer (&app->localized_pretty_name_u8, g_free);
      app->localized_pretty_name_u8 = g_utf16_to_utf8 (friendly_name,
                                                       -1,
                                                       NULL,
                                                       NULL,
                                                       NULL);
    }

  description = NULL;
  success = g_win32_registry_key_get_value_w (capabilities,
                                              g_win32_registry_get_os_dirs_w (),
                                              TRUE,
                                              L"ApplicationDescription",
                                              &vtype,
                                              (void **) &description,
                                              NULL,
                                              NULL);

  if (success && vtype != G_WIN32_REGISTRY_VALUE_STR)
    g_clear_pointer (&description, g_free);

  if (description && app->description == NULL)
    {
      app->description = g_wcsdup (description, -1);
      g_clear_pointer (&app->description_u8, g_free);
      app->description_u8 = g_utf16_to_utf8 (description, -1, NULL, NULL, NULL);
    }

  default_icon_key = g_win32_registry_key_get_child_w (appkey,
                                                       L"DefaultIcon",
                                                       NULL);

  icon_source = NULL;

  if (default_icon_key != NULL)
    {
      success = g_win32_registry_key_get_value_w (default_icon_key,
                                                  NULL,
                                                  TRUE,
                                                  L"",
                                                  &vtype,
                                                  (void **) &icon_source,
                                                  NULL,
                                                  NULL);

      if (success &&
          vtype != G_WIN32_REGISTRY_VALUE_STR)
        g_clear_pointer (&icon_source, g_free);

      g_object_unref (default_icon_key);
    }

  if (icon_source == NULL)
    {
      success = g_win32_registry_key_get_value_w (capabilities,
                                                  NULL,
                                                  TRUE,
                                                  L"ApplicationIcon",
                                                  &vtype,
                                                  (void **) &icon_source,
                                                  NULL,
                                                  NULL);

      if (success &&
          vtype != G_WIN32_REGISTRY_VALUE_STR)
        g_clear_pointer (&icon_source, g_free);
    }

  if (icon_source &&
      app->icon == NULL)
    {
      gchar *name = g_utf16_to_utf8 (icon_source, -1, NULL, NULL, NULL);
      app->icon = g_themed_icon_new (name);
      g_free (name);
    }

  narrow_application_name = NULL;
  success = g_win32_registry_key_get_value_w (capabilities,
                                              g_win32_registry_get_os_dirs_w (),
                                              TRUE,
                                              L"ApplicationName",
                                              &vtype,
                                              (void **) &narrow_application_name,
                                              NULL,
                                              NULL);

  if (success && vtype != G_WIN32_REGISTRY_VALUE_STR)
    g_clear_pointer (&narrow_application_name, g_free);

  if (narrow_application_name &&
      app->localized_pretty_name == NULL)
    {
      app->localized_pretty_name = g_wcsdup (narrow_application_name, -1);
      g_clear_pointer (&app->localized_pretty_name_u8, g_free);
      app->localized_pretty_name_u8 = g_utf16_to_utf8 (narrow_application_name,
                                                       -1,
                                                       NULL,
                                                       NULL,
                                                       NULL);
    }

  associations = g_win32_registry_key_get_child_w (capabilities,
                                                   L"FileAssociations",
                                                   NULL);

  if (associations != NULL)
    {
      GWin32RegistryValueIter iter;

      if (g_win32_registry_value_iter_init (&iter, associations, NULL))
        {
          gunichar2 *file_extension;
          gunichar2 *extension_handler;
          gsize      file_extension_len;
          gsize      extension_handler_size;
          GWin32RegistryValueType value_type;

          while (g_win32_registry_value_iter_next (&iter, TRUE, NULL))
            {
              if ((!g_win32_registry_value_iter_get_value_type (&iter,
                                                                &value_type,
                                                                NULL)) ||
                  (value_type != G_WIN32_REGISTRY_VALUE_STR) ||
                  (!g_win32_registry_value_iter_get_name_w (&iter,
                                                            &file_extension,
                                                            &file_extension_len,
                                                            NULL)) ||
                  (file_extension_len <= 0) ||
                  (file_extension[0] != L'.') ||
                  (!g_win32_registry_value_iter_get_data_w (&iter, TRUE,
                                                            (void **) &extension_handler,
                                                            &extension_handler_size,
                                                            NULL)) ||
                  (extension_handler_size < sizeof (gunichar2)) ||
                  (extension_handler[0] == L'\0'))
                continue;

              get_file_ext (extension_handler, file_extension, app, FALSE);
            }

          g_win32_registry_value_iter_clear (&iter);
        }

      g_object_unref (associations);
    }

  associations = g_win32_registry_key_get_child_w (capabilities, L"URLAssociations", NULL);

  if (associations != NULL)
    {
      GWin32RegistryValueIter iter;

      if (g_win32_registry_value_iter_init (&iter, associations, NULL))
        {
          gunichar2 *url_schema;
          gunichar2 *schema_handler;
          gsize      url_schema_len;
          gsize      schema_handler_size;
          GWin32RegistryValueType value_type;

          while (g_win32_registry_value_iter_next (&iter, TRUE, NULL))
            {
              gchar *schema_u8;
              gchar *schema_u8_folded;

              if ((!g_win32_registry_value_iter_get_value_type (&iter,
                                                                &value_type,
                                                                NULL)) ||
                  ((value_type != G_WIN32_REGISTRY_VALUE_STR) &&
                   (value_type != G_WIN32_REGISTRY_VALUE_EXPAND_STR)) ||
                  (!g_win32_registry_value_iter_get_name_w (&iter,
                                                            &url_schema,
                                                            &url_schema_len,
                                                            NULL)) ||
                  (url_schema_len <= 0) ||
                  (url_schema[0] == L'\0') ||
                  (!g_win32_registry_value_iter_get_data_w (&iter, TRUE,
                                                            (void **) &schema_handler,
                                                            &schema_handler_size,
                                                            NULL)) ||
                  (schema_handler_size < sizeof (gunichar2)) ||
                  (schema_handler[0] == L'\0'))
                continue;



              if (g_utf16_to_utf8_and_fold (url_schema,
                                            url_schema_len,
                                            &schema_u8,
                                            &schema_u8_folded))
                get_url_association (schema_handler, url_schema, schema_u8, schema_u8_folded, app, FALSE);

              g_clear_pointer (&schema_u8, g_free);
              g_clear_pointer (&schema_u8_folded, g_free);
            }

          g_win32_registry_value_iter_clear (&iter);
        }

      g_object_unref (associations);
    }

  g_clear_pointer (&fallback_friendly_name, g_free);
  g_clear_pointer (&description, g_free);
  g_clear_pointer (&icon_source, g_free);
  g_clear_pointer (&narrow_application_name, g_free);

  g_object_unref (appkey);
  g_object_unref (capabilities);
  g_clear_pointer (&app_key_path_u8, g_free);
  g_clear_pointer (&app_key_path_u8_folded, g_free);
  g_clear_pointer (&canonical_name_u8, g_free);
  g_clear_pointer (&canonical_name_folded, g_free);
}

/* Iterates over subkeys in HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\
 * and calls get_url_association() for each one that has a user-chosen handler.
 */
static void
read_urls (GWin32RegistryKey *url_associations)
{
  GWin32RegistrySubkeyIter url_iter;

  if (url_associations == NULL)
    return;

  if (!g_win32_registry_subkey_iter_init (&url_iter, url_associations, NULL))
    return;

  while (g_win32_registry_subkey_iter_next (&url_iter, TRUE, NULL))
    {
      gchar *schema_u8 = NULL;
      gchar *schema_u8_folded = NULL;
      gunichar2 *url_schema = NULL;
      gunichar2 *program_id = NULL;
      GWin32RegistryKey *user_choice = NULL;
      gsize url_schema_len;
      GWin32RegistryValueType val_type;

      if (g_win32_registry_subkey_iter_get_name_w (&url_iter,
                                                   &url_schema,
                                                   &url_schema_len,
                                                   NULL) &&
          g_utf16_to_utf8_and_fold (url_schema,
                                    url_schema_len,
                                    &schema_u8,
                                    &schema_u8_folded) &&
          (user_choice = _g_win32_registry_key_build_and_new_w (NULL, URL_ASSOCIATIONS,
                                                                url_schema, USER_CHOICE,
                                                                NULL)) != NULL &&
          g_win32_registry_key_get_value_w (user_choice,
                                            NULL,
                                            TRUE,
                                            L"Progid",
                                            &val_type,
                                            (void **) &program_id,
                                            NULL,
                                            NULL) &&
          val_type == G_WIN32_REGISTRY_VALUE_STR)
        get_url_association (program_id, url_schema, schema_u8, schema_u8_folded, NULL, TRUE);

      g_clear_pointer (&program_id, g_free);
      g_clear_pointer (&user_choice, g_object_unref);
      g_clear_pointer (&schema_u8, g_free);
      g_clear_pointer (&schema_u8_folded, g_free);
    }

  g_win32_registry_subkey_iter_clear (&url_iter);
}

/* Reads an application that is only registered by the basename of its
 * executable (and doesn't have Capabilities subkey).
 * @incapable_app is the registry key for the app.
 * @app_exe_basename is the basename of its executable.
 */
static void
read_incapable_app (GWin32RegistryKey *incapable_app,
                    const gunichar2   *app_exe_basename,
                    gchar             *app_exe_basename_u8,
                    gchar             *app_exe_basename_u8_folded)
{
  GWin32RegistryValueIter sup_iter;
  GWin32AppInfoApplication *app;
  GList *verbs;
  const reg_verb *preferred_verb;
  gunichar2 *friendly_app_name;
  gboolean success;
  GWin32RegistryValueType vtype;
  gboolean no_open_with;
  GWin32RegistryKey *default_icon_key;
  gunichar2 *icon_source;
  GIcon *icon = NULL;
  GWin32RegistryKey *supported_key;

  if (!get_verbs (incapable_app, &preferred_verb, &verbs, L"", L"Shell"))
    return;

  app = get_app_object (apps_by_exe,
                        app_exe_basename,
                        app_exe_basename_u8,
                        app_exe_basename_u8_folded,
                        FALSE,
                        FALSE);

  process_verbs_commands (verbs, /* consumes verbs */
                          preferred_verb,
                          L"HKEY_CLASSES_ROOT\\Applications\\",
                          app_exe_basename,
                          TRUE,
                          app_add_verb,
                          app,
                          app);

  friendly_app_name = NULL;
  success = g_win32_registry_key_get_value_w (incapable_app,
                                              g_win32_registry_get_os_dirs_w (),
                                              TRUE,
                                              L"FriendlyAppName",
                                              &vtype,
                                              (void **) &friendly_app_name,
                                              NULL,
                                              NULL);

  if (success && vtype != G_WIN32_REGISTRY_VALUE_STR)
    g_clear_pointer (&friendly_app_name, g_free);

  no_open_with = g_win32_registry_key_get_value_w (incapable_app,
                                                   NULL,
                                                   TRUE,
                                                   L"NoOpenWith",
                                                   &vtype,
                                                   NULL,
                                                   NULL,
                                                   NULL);

  default_icon_key =
      g_win32_registry_key_get_child_w (incapable_app,
                                        L"DefaultIcon",
                                        NULL);

  icon_source = NULL;

  if (default_icon_key != NULL)
    {
      success =
          g_win32_registry_key_get_value_w (default_icon_key,
                                            NULL,
                                            TRUE,
                                            L"",
                                            &vtype,
                                            (void **) &icon_source,
                                            NULL,
                                            NULL);

      if (success && vtype != G_WIN32_REGISTRY_VALUE_STR)
        g_clear_pointer (&icon_source, g_free);

      g_object_unref (default_icon_key);
    }

  if (icon_source)
    {
      gchar *name = g_utf16_to_utf8 (icon_source, -1, NULL, NULL, NULL);
      icon = g_themed_icon_new (name);
      g_free (name);
    }

  app->no_open_with = no_open_with;

  if (friendly_app_name &&
      app->localized_pretty_name == NULL)
    {
      app->localized_pretty_name = g_wcsdup (friendly_app_name, -1);
      g_clear_pointer (&app->localized_pretty_name_u8, g_free);
      app->localized_pretty_name_u8 =
          g_utf16_to_utf8 (friendly_app_name, -1, NULL, NULL, NULL);
    }

  if (icon && app->icon == NULL)
    app->icon = g_object_ref (icon);

  supported_key =
      g_win32_registry_key_get_child_w (incapable_app,
                                        L"SupportedTypes",
                                        NULL);

  if (supported_key &&
      g_win32_registry_value_iter_init (&sup_iter, supported_key, NULL))
    {
      gunichar2 *ext_name;
      gsize      ext_name_len;

      while (g_win32_registry_value_iter_next (&sup_iter, TRUE, NULL))
        {
          if ((!g_win32_registry_value_iter_get_name_w (&sup_iter,
                                                        &ext_name,
                                                        &ext_name_len,
                                                        NULL)) ||
              (ext_name_len <= 0) ||
              (ext_name[0] != L'.'))
            continue;

          get_file_ext (ext_name, ext_name, app, FALSE);
        }

      g_win32_registry_value_iter_clear (&sup_iter);
    }

  g_clear_object (&supported_key);
  g_free (friendly_app_name);
  g_free (icon_source);

  g_clear_object (&icon);
}

/* Iterates over subkeys of HKEY_CLASSES_ROOT\\Applications
 * and calls read_incapable_app() for each one.
 */
static void
read_exeapps (void)
{
  GWin32RegistryKey *applications_key;
  GWin32RegistrySubkeyIter app_iter;

  applications_key =
      g_win32_registry_key_new_w (L"HKEY_CLASSES_ROOT\\Applications", NULL);

  if (applications_key == NULL)
    return;

  if (!g_win32_registry_subkey_iter_init (&app_iter, applications_key, NULL))
    {
      g_object_unref (applications_key);
      return;
    }

  while (g_win32_registry_subkey_iter_next (&app_iter, TRUE, NULL))
    {
      gunichar2 *app_exe_basename;
      gsize app_exe_basename_len;
      GWin32RegistryKey *incapable_app;
      gchar *app_exe_basename_u8;
      gchar *app_exe_basename_u8_folded;

      if (!g_win32_registry_subkey_iter_get_name_w (&app_iter,
                                                    &app_exe_basename,
                                                    &app_exe_basename_len,
                                                    NULL) ||
          !g_utf16_to_utf8_and_fold (app_exe_basename,
                                     app_exe_basename_len,
                                     &app_exe_basename_u8,
                                     &app_exe_basename_u8_folded))
        continue;

      incapable_app =
          g_win32_registry_key_get_child_w (applications_key,
                                            app_exe_basename,
                                            NULL);

      if (incapable_app != NULL)
        read_incapable_app (incapable_app,
                            app_exe_basename,
                            app_exe_basename_u8,
                            app_exe_basename_u8_folded);

      g_clear_object (&incapable_app);
      g_clear_pointer (&app_exe_basename_u8, g_free);
      g_clear_pointer (&app_exe_basename_u8_folded, g_free);
    }

  g_win32_registry_subkey_iter_clear (&app_iter);
  g_object_unref (applications_key);
}

/* Iterates over subkeys of HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\
 * and calls get_file_ext() for each associated handler
 * (starting with user-chosen handler, if any)
 */
static void
read_exts (GWin32RegistryKey *file_exts)
{
  GWin32RegistrySubkeyIter ext_iter;
  gunichar2 *file_extension;
  gsize file_extension_len;

  if (file_exts == NULL)
    return;

  if (!g_win32_registry_subkey_iter_init (&ext_iter, file_exts, NULL))
    return;

  while (g_win32_registry_subkey_iter_next (&ext_iter, TRUE, NULL))
    {
      GWin32RegistryKey *open_with_progids;
      gunichar2 *program_id;
      GWin32RegistryValueIter iter;
      gunichar2 *value_name;
      gsize      value_name_len;
      GWin32RegistryValueType value_type;
      GWin32RegistryKey *user_choice;

      if (!g_win32_registry_subkey_iter_get_name_w (&ext_iter,
                                                    &file_extension,
                                                    &file_extension_len,
                                                    NULL))
        continue;

      program_id = NULL;
      user_choice = _g_win32_registry_key_build_and_new_w (NULL, FILE_EXTS, file_extension,
                                                           USER_CHOICE, NULL);
      if (user_choice &&
          g_win32_registry_key_get_value_w (user_choice,
                                            NULL,
                                            TRUE,
                                            L"Progid",
                                            &value_type,
                                            (void **) &program_id,
                                            NULL,
                                            NULL) &&
          value_type == G_WIN32_REGISTRY_VALUE_STR)
        {
          /* Note: program_id could be "ProgramID" or "Applications\\program.exe".
           * The code still works, but handler_id might have a backslash
           * in it - that might trip us up later on.
           * Even though in that case this is logically an "application"
           * registry entry, we don't treat it in any special way.
           * We do scan that registry branch anyway, just not here.
           */
          get_file_ext (program_id, file_extension, NULL, TRUE);
        }

      g_clear_object (&user_choice);
      g_clear_pointer (&program_id, g_free);

      open_with_progids = _g_win32_registry_key_build_and_new_w (NULL, FILE_EXTS,
                                                                 file_extension,
                                                                 OPEN_WITH_PROGIDS,
                                                                 NULL);

      if (open_with_progids == NULL)
        continue;

      if (!g_win32_registry_value_iter_init (&iter, open_with_progids, NULL))
        {
          g_clear_object (&open_with_progids);
          continue;
        }

      while (g_win32_registry_value_iter_next (&iter, TRUE, NULL))
        {
          if (!g_win32_registry_value_iter_get_name_w (&iter, &value_name,
                                                       &value_name_len,
                                                       NULL) ||
              (value_name_len == 0))
            continue;

          get_file_ext (value_name, file_extension, NULL, FALSE);
        }

      g_win32_registry_value_iter_clear (&iter);
      g_clear_object (&open_with_progids);
    }

  g_win32_registry_subkey_iter_clear (&ext_iter);
}

/* Iterates over subkeys in HKCR, calls
 * get_file_ext() for any subkey that starts with ".",
 * or get_url_association() for any subkey that could
 * be a URL schema and has a "URL Protocol" value.
 */
static void
read_classes (GWin32RegistryKey *classes_root)
{
  GWin32RegistrySubkeyIter class_iter;
  gunichar2 *class_name;
  gsize class_name_len;

  if (classes_root == NULL)
    return;

  if (!g_win32_registry_subkey_iter_init (&class_iter, classes_root, NULL))
    return;

  while (g_win32_registry_subkey_iter_next (&class_iter, TRUE, NULL))
    {
      if ((!g_win32_registry_subkey_iter_get_name_w (&class_iter,
                                                     &class_name,
                                                     &class_name_len,
                                                     NULL)) ||
          (class_name_len <= 1))
        continue;

      if (class_name[0] == L'.')
        {
          GWin32RegistryKey *class_key;
          GWin32RegistryValueIter iter;
          GWin32RegistryKey *open_with_progids;
          gunichar2 *value_name;
          gsize      value_name_len;

          /* Read the data from the HKCR\\.ext (usually proxied
           * to another HKCR subkey)
           */
          get_file_ext (class_name, class_name, NULL, FALSE);

          class_key = g_win32_registry_key_get_child_w (classes_root, class_name, NULL);

          if (class_key == NULL)
            continue;

          open_with_progids = g_win32_registry_key_get_child_w (class_key, L"OpenWithProgids", NULL);
          g_clear_object (&class_key);

          if (open_with_progids == NULL)
            continue;

          if (!g_win32_registry_value_iter_init (&iter, open_with_progids, NULL))
            {
              g_clear_object (&open_with_progids);
              continue;
            }

          /* Read the data for other handlers for this extension */
          while (g_win32_registry_value_iter_next (&iter, TRUE, NULL))
            {
              if (!g_win32_registry_value_iter_get_name_w (&iter, &value_name,
                                                           &value_name_len,
                                                           NULL) ||
                  (value_name_len == 0))
                continue;

              get_file_ext (value_name, class_name, NULL, FALSE);
            }

          g_win32_registry_value_iter_clear (&iter);
          g_clear_object (&open_with_progids);
        }
      else
        {
          gsize i;
          GWin32RegistryKey *class_key;
          gboolean success;
          GWin32RegistryValueType vtype;
          gchar *schema_u8;
          gchar *schema_u8_folded;

          for (i = 0; i < class_name_len; i++)
            if (!iswalpha (class_name[i]))
              break;

          if (i != class_name_len)
            continue;

          class_key = g_win32_registry_key_get_child_w (classes_root, class_name, NULL);

          if (class_key == NULL)
            continue;

          success = g_win32_registry_key_get_value_w (class_key,
                                                      NULL,
                                                      TRUE,
                                                      L"URL Protocol",
                                                      &vtype,
                                                      NULL,
                                                      NULL,
                                                      NULL);
          g_clear_object (&class_key);

          if (!success ||
              vtype != G_WIN32_REGISTRY_VALUE_STR)
            continue;

          if (!g_utf16_to_utf8_and_fold (class_name, -1, &schema_u8, &schema_u8_folded))
            continue;

          get_url_association (class_name, class_name, schema_u8, schema_u8_folded, NULL, FALSE);

          g_clear_pointer (&schema_u8, g_free);
          g_clear_pointer (&schema_u8_folded, g_free);
        }
    }

  g_win32_registry_subkey_iter_clear (&class_iter);
}

/* Iterates over all handlers and over all apps,
 * and links handler verbs to apps if a handler
 * runs the same executable as one of the app verbs.
 */
static void
link_handlers_to_unregistered_apps (void)
{
  GHashTableIter iter;
  GHashTableIter app_iter;
  GWin32AppInfoHandler *handler;
  gchar *handler_id_fld;
  GWin32AppInfoApplication *app;
  gchar *canonical_name_fld;
  gchar *appexe_fld_basename;

  g_hash_table_iter_init (&iter, handlers);
  while (g_hash_table_iter_next (&iter,
                                 (gpointer *) &handler_id_fld,
                                 (gpointer *) &handler))
    {
      gsize vi;

      for (vi = 0; vi < handler->verbs->len; vi++)
        {
          GWin32AppInfoShellVerb *handler_verb;
          const gchar *handler_exe_basename;
          /* 0 - haven't tried to get info; 1 - got info; -1 - failed to get info */
          gint have_stat_handler = 0;
          GWin32PrivateStat handler_verb_exec_info;

          handler_verb = _verb_idx (handler->verbs, vi);

          if (handler_verb->app != NULL)
            continue;

          handler_exe_basename = g_utf8_find_basename (handler_verb->executable_folded, -1);
          g_hash_table_iter_init (&app_iter, apps_by_id);

          while (g_hash_table_iter_next (&app_iter,
                                         (gpointer *) &canonical_name_fld,
                                         (gpointer *) &app))
            {
              GWin32AppInfoShellVerb *app_verb;
              gsize ai;

              for (ai = 0; ai < app->verbs->len; ai++)
                {
                  GWin32PrivateStat app_verb_exec_info;
                  const gchar *app_exe_basename;
                  app_verb = _verb_idx (app->verbs, ai);

                  app_exe_basename = g_utf8_find_basename (app_verb->executable_folded, -1);

                  /* First check that the executable paths are identical */
                  if (g_strcmp0 (app_verb->executable_folded, handler_verb->executable_folded) != 0)
                    {
                      /* If not, check the basenames. If they are different, don't bother
                       * with further checks.
                       */
                      if (g_strcmp0 (app_exe_basename, handler_exe_basename) != 0)
                        continue;

                      /* Get filesystem IDs for both files.
                       * For the handler that is attempted only once.
                       */
                      if (have_stat_handler == 0)
                        {
                          if (GLIB_PRIVATE_CALL (g_win32_stat_utf8) (handler_verb->executable_folded,
                                                                     &handler_verb_exec_info) == 0)
                            have_stat_handler = 1;
                          else
                            have_stat_handler = -1;
                        }

                      if (have_stat_handler != 1 ||
                          (GLIB_PRIVATE_CALL (g_win32_stat_utf8) (app_verb->executable_folded,
                                                                  &app_verb_exec_info) != 0) ||
                          app_verb_exec_info.file_index != handler_verb_exec_info.file_index)
                        continue;
                    }

                  handler_verb->app = app;
                  break;
                }
            }

          if (handler_verb->app != NULL)
            continue;

          g_hash_table_iter_init (&app_iter, apps_by_exe);

          while (g_hash_table_iter_next (&app_iter,
                                         (gpointer *) &appexe_fld_basename,
                                         (gpointer *) &app))
            {
              /* Use basename because apps_by_exe only has basenames */
              if (g_strcmp0 (handler_exe_basename, appexe_fld_basename) != 0)
                continue;

              handler_verb->app = app;
              break;
            }
        }
    }
}

/* Finds all .ext and schema: handler verbs that have no app linked to them,
 * creates a "fake app" object and links these verbs to these
 * objects. Objects are identified by the full path to
 * the executable being run, thus multiple different invocations
 * get grouped in a more-or-less natural way.
 * The iteration goes separately over .ext and schema: handlers
 * (instead of the global handlers hashmap) to allow us to
 * put the handlers into supported_urls or supported_exts as
 * needed (handler objects themselves have no knowledge of extensions
 * and/or URLs they are associated with).
 */
static void
link_handlers_to_fake_apps (void)
{
  GHashTableIter iter;
  GHashTableIter handler_iter;
  gchar *extension_utf8_folded;
  GWin32AppInfoFileExtension *file_extn;
  gchar *handler_id_fld;
  GWin32AppInfoHandler *handler;
  gchar *url_utf8_folded;
  GWin32AppInfoURLSchema *schema;

  g_hash_table_iter_init (&iter, extensions);
  while (g_hash_table_iter_next (&iter,
                                 (gpointer *) &extension_utf8_folded,
                                 (gpointer *) &file_extn))
    {
      g_hash_table_iter_init (&handler_iter, file_extn->handlers);
      while (g_hash_table_iter_next (&handler_iter,
                                     (gpointer *) &handler_id_fld,
                                     (gpointer *) &handler))
        {
          gsize vi;

          for (vi = 0; vi < handler->verbs->len; vi++)
            {
              GWin32AppInfoShellVerb *handler_verb;
              GWin32AppInfoApplication *app;
              gunichar2 *exename_utf16;
              handler_verb = _verb_idx (handler->verbs, vi);

              if (handler_verb->app != NULL)
                continue;

              exename_utf16 = g_utf8_to_utf16 (handler_verb->executable, -1, NULL, NULL, NULL);
              if (exename_utf16 == NULL)
                continue;

              app = get_app_object (fake_apps,
                                    exename_utf16,
                                    handler_verb->executable,
                                    handler_verb->executable_folded,
                                    FALSE,
                                    FALSE);
              g_clear_pointer (&exename_utf16, g_free);
              handler_verb->app = g_object_ref (app);

              app_add_verb (app,
                            app,
                            handler_verb->verb_name,
                            handler_verb->command,
                            handler_verb->command_utf8,
                            handler_verb->verb_displayname,
                            TRUE,
                            TRUE);
              g_hash_table_insert (app->supported_exts,
                                   g_strdup (extension_utf8_folded),
                                   g_object_ref (handler));
            }
        }
    }

  g_hash_table_iter_init (&iter, urls);
  while (g_hash_table_iter_next (&iter,
                                 (gpointer *) &url_utf8_folded,
                                 (gpointer *) &schema))
    {
      g_hash_table_iter_init (&handler_iter, schema->handlers);
      while (g_hash_table_iter_next (&handler_iter,
                                     (gpointer *) &handler_id_fld,
                                     (gpointer *) &handler))
        {
          gsize vi;

          for (vi = 0; vi < handler->verbs->len; vi++)
            {
              GWin32AppInfoShellVerb *handler_verb;
              GWin32AppInfoApplication *app;
              gchar *command_utf8_folded;
              handler_verb = _verb_idx (handler->verbs, vi);

              if (handler_verb->app != NULL)
                continue;

              command_utf8_folded = g_utf8_casefold (handler_verb->command_utf8, -1);
              app = get_app_object (fake_apps,
                                    handler_verb->command,
                                    handler_verb->command_utf8,
                                    command_utf8_folded,
                                    FALSE,
                                    FALSE);
              g_clear_pointer (&command_utf8_folded, g_free);
              handler_verb->app = g_object_ref (app);

              app_add_verb (app,
                            app,
                            handler_verb->verb_name,
                            handler_verb->command,
                            handler_verb->command_utf8,
                            handler_verb->verb_displayname,
                            TRUE,
                            TRUE);
              g_hash_table_insert (app->supported_urls,
                                   g_strdup (url_utf8_folded),
                                   g_object_ref (handler));
            }
        }
    }
}


static void
update_registry_data (void)
{
  guint i;
  GPtrArray *capable_apps_keys;
  GPtrArray *user_capable_apps_keys;
  GPtrArray *priority_capable_apps_keys;
  GWin32RegistryKey *url_associations;
  GWin32RegistryKey *file_exts;
  GWin32RegistryKey *classes_root;
  DWORD collect_start, collect_end, alloc_end, capable_end, url_end, ext_end, exeapp_end, classes_end, postproc_end;

  url_associations =
      g_win32_registry_key_new_w (L"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations",
                                   NULL);
  file_exts =
      g_win32_registry_key_new_w (L"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts",
                                   NULL);
  classes_root = g_win32_registry_key_new_w (L"HKEY_CLASSES_ROOT", NULL);

  capable_apps_keys = g_ptr_array_new_with_free_func (g_free);
  user_capable_apps_keys = g_ptr_array_new_with_free_func (g_free);
  priority_capable_apps_keys = g_ptr_array_new_with_free_func (g_free);

  g_clear_pointer (&apps_by_id, g_hash_table_destroy);
  g_clear_pointer (&apps_by_exe, g_hash_table_destroy);
  g_clear_pointer (&fake_apps, g_hash_table_destroy);
  g_clear_pointer (&urls, g_hash_table_destroy);
  g_clear_pointer (&extensions, g_hash_table_destroy);
  g_clear_pointer (&handlers, g_hash_table_destroy);

  collect_start = GetTickCount ();
  collect_capable_apps_from_clients (capable_apps_keys,
                                     priority_capable_apps_keys,
                                     FALSE);
  collect_capable_apps_from_clients (user_capable_apps_keys,
                                     priority_capable_apps_keys,
                                     TRUE);
  collect_capable_apps_from_registered_apps (user_capable_apps_keys,
                                             TRUE);
  collect_capable_apps_from_registered_apps (capable_apps_keys,
                                             FALSE);
  collect_end = GetTickCount ();

  apps_by_id =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  apps_by_exe =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  fake_apps =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  urls =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  extensions =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  handlers =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  alloc_end = GetTickCount ();

  for (i = 0; i < priority_capable_apps_keys->len; i++)
    read_capable_app (g_ptr_array_index (priority_capable_apps_keys, i),
                      TRUE,
                      TRUE);
  for (i = 0; i < user_capable_apps_keys->len; i++)
    read_capable_app (g_ptr_array_index (user_capable_apps_keys, i),
                      TRUE,
                      FALSE);
  for (i = 0; i < capable_apps_keys->len; i++)
    read_capable_app (g_ptr_array_index (capable_apps_keys, i),
                      FALSE,
                      FALSE);
  capable_end = GetTickCount ();

  read_urls (url_associations);
  url_end = GetTickCount ();
  read_exts (file_exts);
  ext_end = GetTickCount ();
  read_exeapps ();
  exeapp_end = GetTickCount ();
  read_classes (classes_root);
  classes_end = GetTickCount ();
  link_handlers_to_unregistered_apps ();
  link_handlers_to_fake_apps ();
  postproc_end = GetTickCount ();

  g_debug ("Collecting capable appnames: %lums\n"
           "Allocating hashtables:...... %lums\n"
           "Reading capable apps:        %lums\n"
           "Reading URL associations:... %lums\n"
           "Reading extension assocs:    %lums\n"
           "Reading exe-only apps:...... %lums\n"
           "Reading classes:             %lums\n"
           "Postprocessing:..............%lums\n"
           "TOTAL:                       %lums",
           collect_end - collect_start,
           alloc_end - collect_end,
           capable_end - alloc_end,
           url_end - capable_end,
           ext_end - url_end,
           exeapp_end - ext_end,
           classes_end - exeapp_end,
           postproc_end - classes_end,
           postproc_end - collect_start);

  g_clear_object (&classes_root);
  g_clear_object (&url_associations);
  g_clear_object (&file_exts);
  g_ptr_array_free (capable_apps_keys, TRUE);
  g_ptr_array_free (user_capable_apps_keys, TRUE);
  g_ptr_array_free (priority_capable_apps_keys, TRUE);

  return;
}

static void
watch_keys (void)
{
  if (url_associations_key)
    g_win32_registry_key_watch (url_associations_key,
                                TRUE,
                                G_WIN32_REGISTRY_WATCH_NAME |
                                G_WIN32_REGISTRY_WATCH_ATTRIBUTES |
                                G_WIN32_REGISTRY_WATCH_VALUES,
                                NULL,
                                NULL,
                                NULL);

  if (file_exts_key)
    g_win32_registry_key_watch (file_exts_key,
                                TRUE,
                                G_WIN32_REGISTRY_WATCH_NAME |
                                G_WIN32_REGISTRY_WATCH_ATTRIBUTES |
                                G_WIN32_REGISTRY_WATCH_VALUES,
                                NULL,
                                NULL,
                                NULL);

  if (user_clients_key)
    g_win32_registry_key_watch (user_clients_key,
                                TRUE,
                                G_WIN32_REGISTRY_WATCH_NAME |
                                G_WIN32_REGISTRY_WATCH_ATTRIBUTES |
                                G_WIN32_REGISTRY_WATCH_VALUES,
                                NULL,
                                NULL,
                                NULL);

  if (system_clients_key)
    g_win32_registry_key_watch (system_clients_key,
                                TRUE,
                                G_WIN32_REGISTRY_WATCH_NAME |
                                G_WIN32_REGISTRY_WATCH_ATTRIBUTES |
                                G_WIN32_REGISTRY_WATCH_VALUES,
                                NULL,
                                NULL,
                                NULL);

  if (applications_key)
    g_win32_registry_key_watch (applications_key,
                                TRUE,
                                G_WIN32_REGISTRY_WATCH_NAME |
                                G_WIN32_REGISTRY_WATCH_ATTRIBUTES |
                                G_WIN32_REGISTRY_WATCH_VALUES,
                                NULL,
                                NULL,
                                NULL);

  if (user_registered_apps_key)
    g_win32_registry_key_watch (user_registered_apps_key,
                                TRUE,
                                G_WIN32_REGISTRY_WATCH_NAME |
                                G_WIN32_REGISTRY_WATCH_ATTRIBUTES |
                                G_WIN32_REGISTRY_WATCH_VALUES,
                                NULL,
                                NULL,
                                NULL);

  if (system_registered_apps_key)
    g_win32_registry_key_watch (system_registered_apps_key,
                                TRUE,
                                G_WIN32_REGISTRY_WATCH_NAME |
                                G_WIN32_REGISTRY_WATCH_ATTRIBUTES |
                                G_WIN32_REGISTRY_WATCH_VALUES,
                                NULL,
                                NULL,
                                NULL);

  if (classes_root_key)
    g_win32_registry_key_watch (classes_root_key,
                                FALSE,
                                G_WIN32_REGISTRY_WATCH_NAME |
                                G_WIN32_REGISTRY_WATCH_ATTRIBUTES |
                                G_WIN32_REGISTRY_WATCH_VALUES,
                                NULL,
                                NULL,
                                NULL);
}


static void
g_win32_appinfo_init (void)
{
  static gsize initialized;

  if (g_once_init_enter (&initialized))
    {
      url_associations_key =
          g_win32_registry_key_new_w (L"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations",
                                       NULL);
      file_exts_key =
          g_win32_registry_key_new_w (L"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts",
                                       NULL);
      user_clients_key =
          g_win32_registry_key_new_w (L"HKEY_CURRENT_USER\\Software\\Clients",
                                       NULL);
      system_clients_key =
          g_win32_registry_key_new_w (L"HKEY_LOCAL_MACHINE\\Software\\Clients",
                                       NULL);
      applications_key =
          g_win32_registry_key_new_w (L"HKEY_CLASSES_ROOT\\Applications",
                                       NULL);
      user_registered_apps_key =
          g_win32_registry_key_new_w (L"HKEY_CURRENT_USER\\Software\\RegisteredApplications",
                                       NULL);
      system_registered_apps_key =
          g_win32_registry_key_new_w (L"HKEY_LOCAL_MACHINE\\Software\\RegisteredApplications",
                                       NULL);
      classes_root_key =
          g_win32_registry_key_new_w (L"HKEY_CLASSES_ROOT",
                                       NULL);

      watch_keys ();

      update_registry_data ();

      g_once_init_leave (&initialized, TRUE);
    }

  if ((url_associations_key       && g_win32_registry_key_has_changed (url_associations_key))       ||
      (file_exts_key              && g_win32_registry_key_has_changed (file_exts_key))              ||
      (user_clients_key           && g_win32_registry_key_has_changed (user_clients_key))           ||
      (system_clients_key         && g_win32_registry_key_has_changed (system_clients_key))         ||
      (applications_key           && g_win32_registry_key_has_changed (applications_key))           ||
      (user_registered_apps_key   && g_win32_registry_key_has_changed (user_registered_apps_key))   ||
      (system_registered_apps_key && g_win32_registry_key_has_changed (system_registered_apps_key)) ||
      (classes_root_key           && g_win32_registry_key_has_changed (classes_root_key)))
    {
      G_LOCK (gio_win32_appinfo);
      update_registry_data ();
      watch_keys ();
      G_UNLOCK (gio_win32_appinfo);
    }
}


static void g_win32_app_info_iface_init (GAppInfoIface *iface);

struct _GWin32AppInfo
{
  GObject parent_instance;

  /*<private>*/
  gchar **supported_types;

  GWin32AppInfoApplication *app;

  GWin32AppInfoHandler *handler;

  guint startup_notify : 1;
};

G_DEFINE_TYPE_WITH_CODE (GWin32AppInfo, g_win32_app_info, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_APP_INFO,
                                                g_win32_app_info_iface_init))


static void
g_win32_app_info_finalize (GObject *object)
{
  GWin32AppInfo *info;

  info = G_WIN32_APP_INFO (object);

  g_clear_pointer (&info->supported_types, g_strfreev);
  g_clear_object (&info->app);
  g_clear_object (&info->handler);

  G_OBJECT_CLASS (g_win32_app_info_parent_class)->finalize (object);
}

static void
g_win32_app_info_class_init (GWin32AppInfoClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = g_win32_app_info_finalize;
}

static void
g_win32_app_info_init (GWin32AppInfo *local)
{
}

static GAppInfo *
g_win32_app_info_new_from_app (GWin32AppInfoApplication *app,
                               GWin32AppInfoHandler     *handler)
{
  GWin32AppInfo *new_info;
  GHashTableIter iter;
  gpointer ext;
  int i;

  new_info = g_object_new (G_TYPE_WIN32_APP_INFO, NULL);

  new_info->app = g_object_ref (app);

  g_win32_appinfo_init ();
  G_LOCK (gio_win32_appinfo);

  i = 0;
  g_hash_table_iter_init (&iter, new_info->app->supported_exts);

  while (g_hash_table_iter_next (&iter, &ext, NULL))
    {
      if (ext)
        i += 1;
    }

  new_info->supported_types = g_malloc (sizeof (gchar *) * (i + 1));

  i = 0;
  g_hash_table_iter_init (&iter, new_info->app->supported_exts);

  while (g_hash_table_iter_next (&iter, &ext, NULL))
    {
      if (!ext)
        continue;

      new_info->supported_types[i] = g_strdup ((gchar *) ext);
      i += 1;
    }

  G_UNLOCK (gio_win32_appinfo);

  new_info->supported_types[i] = NULL;

  new_info->handler = handler ? g_object_ref (handler) : NULL;

  return G_APP_INFO (new_info);
}


static GAppInfo *
g_win32_app_info_dup (GAppInfo *appinfo)
{
  GWin32AppInfo *info = G_WIN32_APP_INFO (appinfo);
  GWin32AppInfo *new_info;

  new_info = g_object_new (G_TYPE_WIN32_APP_INFO, NULL);

  if (info->app)
    new_info->app = g_object_ref (info->app);

  if (info->handler)
    new_info->handler = g_object_ref (info->handler);

  new_info->startup_notify = info->startup_notify;

  if (info->supported_types)
    {
      int i;

      for (i = 0; info->supported_types[i]; i++)
        break;

      new_info->supported_types = g_malloc (sizeof (gchar *) * (i + 1));

      for (i = 0; info->supported_types[i]; i++)
        new_info->supported_types[i] = g_strdup (info->supported_types[i]);

      new_info->supported_types[i] = NULL;
    }

  return G_APP_INFO (new_info);
}

static gboolean
g_win32_app_info_equal (GAppInfo *appinfo1,
                        GAppInfo *appinfo2)
{
  GWin32AppInfoShellVerb *shverb1 = NULL;
  GWin32AppInfoShellVerb *shverb2 = NULL;
  GWin32AppInfo *info1 = G_WIN32_APP_INFO (appinfo1);
  GWin32AppInfo *info2 = G_WIN32_APP_INFO (appinfo2);
  GWin32AppInfoApplication *app1 = info1->app;
  GWin32AppInfoApplication *app2 = info2->app;

  if (app1 == NULL ||
      app2 == NULL)
    return info1 == info2;

  if (app1->canonical_name_folded != NULL &&
      app2->canonical_name_folded != NULL)
    return (g_strcmp0 (app1->canonical_name_folded,
                       app2->canonical_name_folded)) == 0;

  if (app1->verbs->len > 0 &&
      app2->verbs->len > 0)
    {
      shverb1 = _verb_idx (app1->verbs, 0);
      shverb2 = _verb_idx (app2->verbs, 0);
      if (shverb1->executable_folded != NULL &&
          shverb2->executable_folded != NULL)
        return (g_strcmp0 (shverb1->executable_folded,
                           shverb2->executable_folded)) == 0;
    }

  return app1 == app2;
}

static const char *
g_win32_app_info_get_id (GAppInfo *appinfo)
{
  GWin32AppInfo *info = G_WIN32_APP_INFO (appinfo);
  GWin32AppInfoShellVerb *shverb;

  if (info->app == NULL)
    return NULL;

  if (info->app->canonical_name_u8)
    return info->app->canonical_name_u8;

  if (info->app->verbs->len > 0 &&
      (shverb = _verb_idx (info->app->verbs, 0))->executable_basename != NULL)
    return shverb->executable_basename;

  return NULL;
}

static const char *
g_win32_app_info_get_name (GAppInfo *appinfo)
{
  GWin32AppInfo *info = G_WIN32_APP_INFO (appinfo);

  if (info->app && info->app->canonical_name_u8)
    return info->app->canonical_name_u8;
  else
    return P_("Unnamed");
}

static const char *
g_win32_app_info_get_display_name (GAppInfo *appinfo)
{
  GWin32AppInfo *info = G_WIN32_APP_INFO (appinfo);

  if (info->app)
    {
      if (info->app->localized_pretty_name_u8)
        return info->app->localized_pretty_name_u8;
      else if (info->app->pretty_name_u8)
        return info->app->pretty_name_u8;
    }

  return g_win32_app_info_get_name (appinfo);
}

static const char *
g_win32_app_info_get_description (GAppInfo *appinfo)
{
  GWin32AppInfo *info = G_WIN32_APP_INFO (appinfo);

  if (info->app == NULL)
    return NULL;

  return info->app->description_u8;
}

static const char *
g_win32_app_info_get_executable (GAppInfo *appinfo)
{
  GWin32AppInfo *info = G_WIN32_APP_INFO (appinfo);

  if (info->app == NULL)
    return NULL;

  if (info->app->verbs->len > 0)
    return _verb_idx (info->app->verbs, 0)->executable;

  return NULL;
}

static const char *
g_win32_app_info_get_commandline (GAppInfo *appinfo)
{
  GWin32AppInfo *info = G_WIN32_APP_INFO (appinfo);

  if (info->app == NULL)
    return NULL;

  if (info->app->verbs->len > 0)
    return _verb_idx (info->app->verbs, 0)->command_utf8;

  return NULL;
}

static GIcon *
g_win32_app_info_get_icon (GAppInfo *appinfo)
{
  GWin32AppInfo *info = G_WIN32_APP_INFO (appinfo);

  if (info->app == NULL)
    return NULL;

  return info->app->icon;
}

typedef struct _file_or_uri {
  gchar *uri;
  gchar *file;
} file_or_uri;

static char *
expand_macro_single (char macro, file_or_uri *obj)
{
  char *result = NULL;

  switch (macro)
    {
    case '*':
    case '0':
    case '1':
    case 'l':
    case 'd':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      /* TODO: handle 'l' and 'd' differently (longname and desktop name) */
      if (obj->uri)
        result = g_strdup (obj->uri);
      else if (obj->file)
        result = g_strdup (obj->file);
      break;
    case 'u':
    case 'U':
      if (obj->uri)
        result = g_shell_quote (obj->uri);
      break;
    case 'f':
    case 'F':
      if (obj->file)
        result = g_shell_quote (obj->file);
      break;
    }

  return result;
}

static gboolean
expand_macro (char               macro,
              GString           *exec,
              GWin32AppInfo     *info,
              GList            **stat_obj_list,
              GList            **obj_list)
{
  GList *objs = *obj_list;
  char *expanded;
  gboolean result = FALSE;

  g_return_val_if_fail (exec != NULL, FALSE);

/*
Legend: (from http://msdn.microsoft.com/en-us/library/windows/desktop/cc144101%28v=vs.85%29.aspx)
%* - replace with all parameters
%~ - replace with all parameters starting with and following the second parameter
%0 or %1 the first file parameter. For example "C:\\Users\\Eric\\Destop\\New Text Document.txt". Generally this should be in quotes and the applications command line parsing should accept quotes to disambiguate files with spaces in the name and different command line parameters (this is a security best practice and I believe mentioned in MSDN).
%<n> (where N is 2 - 9), replace with the nth parameter
%s - show command
%h - hotkey value
%i - IDList stored in a shared memory handle is passed here.
%l - long file name form of the first parameter. Note win32 applications will be passed the long file name, win16 applications get the short file name. Specifying %L is preferred as it avoids the need to probe for the application type.
%d - desktop absolute parsing name of the first parameter (for items that don't have file system paths)
%v - for verbs that are none implies all, if there is no parameter passed this is the working directory
%w - the working directory
*/

  switch (macro)
    {
    case '*':
    case '~':
      if (*stat_obj_list)
        {
          gint i;
          GList *o;

          for (o = *stat_obj_list, i = 0;
               macro == '~' && o && i < 2;
               o = o->next, i++);

          for (; o; o = o->next)
            {
              expanded = expand_macro_single (macro, o->data);

              if (expanded)
                {
                  if (o != *stat_obj_list)
                    g_string_append (exec, " ");

                  g_string_append (exec, expanded);
                  g_free (expanded);
                }
            }

          objs = NULL;
          result = TRUE;
        }
      break;
    case '0':
    case '1':
    case 'l':
    case 'd':
      if (*stat_obj_list)
        {
          GList *o;

          o = *stat_obj_list;

          if (o)
            {
              expanded = expand_macro_single (macro, o->data);

              if (expanded)
                {
                  if (o != *stat_obj_list)
                    g_string_append (exec, " ");

                  g_string_append (exec, expanded);
                  g_free (expanded);
                }
            }

          if (objs)
            objs = objs->next;

          result = TRUE;
        }
      break;
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      if (*stat_obj_list)
        {
          gint i;
          GList *o;
          gint n;

          switch (macro)
            {
            case '2':
              n = 2;
              break;
            case '3':
              n = 3;
              break;
            case '4':
              n = 4;
              break;
            case '5':
              n = 5;
              break;
            case '6':
              n = 6;
              break;
            case '7':
              n = 7;
              break;
            case '8':
              n = 8;
              break;
            case '9':
              n = 9;
              break;
            }

          for (o = *stat_obj_list, i = 0; o && i < n; o = o->next, i++);

          if (o)
            {
              expanded = expand_macro_single (macro, o->data);

              if (expanded)
                {
                  if (o != *stat_obj_list)
                    g_string_append (exec, " ");

                  g_string_append (exec, expanded);
                  g_free (expanded);
                }
            }
          result = TRUE;

          if (objs)
            objs = NULL;
        }
      break;
    case 's':
      break;
    case 'h':
      break;
    case 'i':
      break;
    case 'v':
      break;
    case 'w':
      expanded = g_get_current_dir ();
      g_string_append (exec, expanded);
      g_free (expanded);
      break;
    case 'u':
    case 'f':
      if (objs)
        {
          expanded = expand_macro_single (macro, objs->data);

          if (expanded)
            {
              g_string_append (exec, expanded);
              g_free (expanded);
            }
          objs = objs->next;
          result = TRUE;
        }

      break;

    case 'U':
    case 'F':
      while (objs)
        {
          expanded = expand_macro_single (macro, objs->data);

          if (expanded)
            {
              g_string_append (exec, expanded);
              g_free (expanded);
            }

          objs = objs->next;
          result = TRUE;

          if (objs != NULL && expanded)
            g_string_append_c (exec, ' ');
        }

      break;

    case 'c':
      if (info->app && info->app->localized_pretty_name_u8)
        {
          expanded = g_shell_quote (info->app->localized_pretty_name_u8);
          g_string_append (exec, expanded);
          g_free (expanded);
        }
      break;

    case 'm': /* deprecated */
    case 'n': /* deprecated */
    case 'N': /* deprecated */
    /*case 'd': *//* deprecated */
    case 'D': /* deprecated */
      break;

    case '%':
      g_string_append_c (exec, '%');
      break;
    }

  *obj_list = objs;

  return result;
}

static gboolean
expand_application_parameters (GWin32AppInfo   *info,
                               const gchar     *exec_line,
                               GList          **objs,
                               int             *argc,
                               char          ***argv,
                               GError         **error)
{
  GList *obj_list = *objs;
  GList **stat_obj_list = objs;
  const char *p = exec_line;
  GString *expanded_exec;
  gboolean res;
  gchar *a_char;

  expanded_exec = g_string_new (NULL);
  res = FALSE;

  while (*p)
    {
      if (p[0] == '%' && p[1] != '\0')
        {
          if (expand_macro (p[1],
                            expanded_exec,
                            info, stat_obj_list,
                            objs))
            res = TRUE;

          p++;
        }
      else
        g_string_append_c (expanded_exec, *p);

      p++;
    }

  /* No file substitutions */
  if (obj_list == *objs && obj_list != NULL && !res)
    {
      /* If there is no macro default to %f. This is also what KDE does */
      g_string_append_c (expanded_exec, ' ');
      expand_macro ('f', expanded_exec, info, stat_obj_list, objs);
    }

  /* Replace '\\' with '/', because g_shell_parse_argv considers them
   * to be escape sequences.
   */
  for (a_char = expanded_exec->str;
       a_char <= &expanded_exec->str[expanded_exec->len];
       a_char++)
    {
      if (*a_char == '\\')
        *a_char = '/';
    }

  res = g_shell_parse_argv (expanded_exec->str, argc, argv, error);
  g_string_free (expanded_exec, TRUE);
  return res;
}


static gchar *
get_appath_for_exe (const gchar *exe_basename)
{
  GWin32RegistryKey *apppath_key = NULL;
  GWin32RegistryValueType val_type;
  gchar *appath = NULL;
  gboolean got_value;
  gchar *key_path = g_strdup_printf ("HKEY_LOCAL_MACHINE\\"
                                     "SOFTWARE\\"
                                     "Microsoft\\"
                                     "Windows\\"
                                     "CurrentVersion\\"
                                     "App Paths\\"
                                     "%s", exe_basename);

  apppath_key = g_win32_registry_key_new (key_path, NULL);
  g_clear_pointer (&key_path, g_free);

  if (apppath_key == NULL)
    return NULL;

  got_value = g_win32_registry_key_get_value (apppath_key,
                                              NULL,
                                              TRUE,
                                              "Path",
                                              &val_type,
                                              (void **) &appath,
                                              NULL,
                                              NULL);

  g_object_unref (apppath_key);

  if (got_value &&
      val_type == G_WIN32_REGISTRY_VALUE_STR)
    return appath;

  g_clear_pointer (&appath, g_free);

  return appath;
}


static gboolean
g_win32_app_info_launch_internal (GWin32AppInfo      *info,
                                  GList              *objs,
                                  GAppLaunchContext  *launch_context,
                                  GSpawnFlags         spawn_flags,
                                  GError            **error)
{
  gboolean completed = FALSE;
  char **argv, **envp;
  int argc;
  const gchar *command;
  gchar *apppath;
  GWin32AppInfoShellVerb *shverb;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (info->app != NULL, FALSE);

  argv = NULL;

  if (launch_context)
    envp = g_app_launch_context_get_environment (launch_context);
  else
    envp = g_get_environ ();

  shverb = NULL;

  if (info->handler != NULL &&
      info->handler->verbs->len > 0)
    shverb = _verb_idx (info->handler->verbs, 0);
  else if (info->app->verbs->len > 0)
    shverb = _verb_idx (info->app->verbs, 0);

  if (shverb == NULL)
    {
      if (info->handler == NULL)
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                     "The app %s in the application object has no verbs",
                     g_win32_appinfo_application_get_some_name (info->app));
      else if (info->handler->verbs->len == 0)
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                     "The app %s and the handler %s in the application object have no verbs",
                     g_win32_appinfo_application_get_some_name (info->app),
                     info->handler->handler_id_folded);

      return FALSE;
    }

  g_assert (shverb->command_utf8 != NULL);
  command = shverb->command_utf8;
  apppath = get_appath_for_exe (shverb->executable_basename);

  if (apppath)
    {
      gchar **p;
      gint p_index;

      for (p = envp, p_index = 0; p[0]; p++, p_index++)
        if ((p[0][0] == 'p' || p[0][0] == 'P') &&
            (p[0][1] == 'a' || p[0][1] == 'A') &&
            (p[0][2] == 't' || p[0][2] == 'T') &&
            (p[0][3] == 'h' || p[0][3] == 'H') &&
            (p[0][4] == '='))
          break;

      if (p[0] == NULL)
        {
          gchar **new_envp;
          new_envp = g_new (char *, g_strv_length (envp) + 2);
          new_envp[0] = g_strdup_printf ("PATH=%s", apppath);

          for (p_index = 0; p_index <= g_strv_length (envp); p_index++)
            new_envp[1 + p_index] = envp[p_index];

          g_free (envp);
          envp = new_envp;
        }
      else
        {
          gchar *p_path;

          p_path = &p[0][5];

          if (p_path[0] != '\0')
            envp[p_index] = g_strdup_printf ("PATH=%s%c%s",
                                             apppath,
                                             G_SEARCHPATH_SEPARATOR,
                                             p_path);
          else
            envp[p_index] = g_strdup_printf ("PATH=%s", apppath);

          g_free (&p_path[-5]);
        }
    }

  do
    {
      GPid pid;

      if (!expand_application_parameters (info,
                                          command,
                                          &objs,
                                          &argc,
                                          &argv,
                                          error))
        goto out;

      if (!g_spawn_async (NULL,
                          argv,
                          envp,
                          spawn_flags,
                          NULL,
                          NULL,
                          &pid,
                          error))
          goto out;

      if (launch_context != NULL)
        {
          GVariantBuilder builder;
          GVariant *platform_data;

          g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
          g_variant_builder_add (&builder, "{sv}", "pid", g_variant_new_int32 ((gint32) pid));

          platform_data = g_variant_ref_sink (g_variant_builder_end (&builder));
          g_signal_emit_by_name (launch_context, "launched", info, platform_data);
          g_variant_unref (platform_data);
        }

      g_strfreev (argv);
      argv = NULL;
    }
  while (objs != NULL);

  completed = TRUE;

 out:
  g_strfreev (argv);
  g_strfreev (envp);

  return completed;
}

static void
free_file_or_uri (gpointer ptr)
{
  file_or_uri *obj = ptr;
  g_free (obj->file);
  g_free (obj->uri);
  g_free (obj);
}


static gboolean
g_win32_app_supports_uris (GWin32AppInfoApplication *app)
{
  gssize num_of_uris_supported;

  if (app == NULL)
    return FALSE;

  num_of_uris_supported = (gssize) g_hash_table_size (app->supported_urls);

  if (g_hash_table_lookup (app->supported_urls, "file"))
    num_of_uris_supported -= 1;

  return num_of_uris_supported > 0;
}


static gboolean
g_win32_app_info_supports_uris (GAppInfo *appinfo)
{
  GWin32AppInfo *info = G_WIN32_APP_INFO (appinfo);

  if (info->app == NULL)
    return FALSE;

  return g_win32_app_supports_uris (info->app);
}


static gboolean
g_win32_app_info_supports_files (GAppInfo *appinfo)
{
  GWin32AppInfo *info = G_WIN32_APP_INFO (appinfo);

  if (info->app == NULL)
    return FALSE;

  return g_hash_table_size (info->app->supported_exts) > 0;
}


static gboolean
g_win32_app_info_launch_uris (GAppInfo           *appinfo,
                              GList              *uris,
                              GAppLaunchContext  *launch_context,
                              GError            **error)
{
  gboolean res = FALSE;
  gboolean do_files;
  GList *objs;

  do_files = g_win32_app_info_supports_files (appinfo);

  objs = NULL;
  while (uris)
    {
      file_or_uri *obj;
      obj = g_new0 (file_or_uri, 1);

      if (do_files)
        {
          GFile *file;
          gchar *path;

          file = g_file_new_for_uri (uris->data);
          path = g_file_get_path (file);
          obj->file = path;
          g_object_unref (file);
        }

      obj->uri = g_strdup (uris->data);

      objs = g_list_prepend (objs, obj);
      uris = uris->next;
    }

  objs = g_list_reverse (objs);

  res = g_win32_app_info_launch_internal (G_WIN32_APP_INFO (appinfo),
                                          objs,
                                          launch_context,
                                          G_SPAWN_SEARCH_PATH,
                                          error);

  g_list_free_full (objs, free_file_or_uri);

  return res;
}

static gboolean
g_win32_app_info_launch (GAppInfo           *appinfo,
                         GList              *files,
                         GAppLaunchContext  *launch_context,
                         GError            **error)
{
  gboolean res = FALSE;
  gboolean do_uris;
  GList *objs;

  do_uris = g_win32_app_info_supports_uris (appinfo);

  objs = NULL;
  while (files)
    {
      file_or_uri *obj;
      obj = g_new0 (file_or_uri, 1);
      obj->file = g_file_get_path (G_FILE (files->data));

      if (do_uris)
        obj->uri = g_file_get_uri (G_FILE (files->data));

      objs = g_list_prepend (objs, obj);
      files = files->next;
    }

  objs = g_list_reverse (objs);

  res = g_win32_app_info_launch_internal (G_WIN32_APP_INFO (appinfo),
                                          objs,
                                          launch_context,
                                          G_SPAWN_SEARCH_PATH,
                                          error);

  g_list_free_full (objs, free_file_or_uri);

  return res;
}

static const char **
g_win32_app_info_get_supported_types (GAppInfo *appinfo)
{
  GWin32AppInfo *info = G_WIN32_APP_INFO (appinfo);

  return (const char**) info->supported_types;
}

GAppInfo *
g_app_info_create_from_commandline (const char           *commandline,
                                    const char           *application_name,
                                    GAppInfoCreateFlags   flags,
                                    GError              **error)
{
  GWin32AppInfo *info;
  GWin32AppInfoApplication *app;
  gunichar2 *app_command;

  g_return_val_if_fail (commandline, NULL);

  app_command = g_utf8_to_utf16 (commandline, -1, NULL, NULL, NULL);

  if (app_command == NULL)
    return NULL;

  info = g_object_new (G_TYPE_WIN32_APP_INFO, NULL);
  app = g_object_new (G_TYPE_WIN32_APPINFO_APPLICATION, NULL);

  app->no_open_with = FALSE;
  app->user_specific = FALSE;
  app->default_app = FALSE;

  if (application_name)
    {
      app->canonical_name = g_utf8_to_utf16 (application_name,
                                             -1,
                                             NULL,
                                             NULL,
                                             NULL);
      app->canonical_name_u8 = g_strdup (application_name);
      app->canonical_name_folded = g_utf8_casefold (application_name, -1);
    }

  app_add_verb (app,
                app,
                L"open",
                app_command,
                commandline,
                "open",
                TRUE,
                FALSE);

  g_clear_pointer (&app_command, g_free);
  info->app = app;
  info->handler = NULL;

  return G_APP_INFO (info);
}

/* GAppInfo interface init */

static void
g_win32_app_info_iface_init (GAppInfoIface *iface)
{
  iface->dup = g_win32_app_info_dup;
  iface->equal = g_win32_app_info_equal;
  iface->get_id = g_win32_app_info_get_id;
  iface->get_name = g_win32_app_info_get_name;
  iface->get_description = g_win32_app_info_get_description;
  iface->get_executable = g_win32_app_info_get_executable;
  iface->get_icon = g_win32_app_info_get_icon;
  iface->launch = g_win32_app_info_launch;
  iface->supports_uris = g_win32_app_info_supports_uris;
  iface->supports_files = g_win32_app_info_supports_files;
  iface->launch_uris = g_win32_app_info_launch_uris;
/*  iface->should_show = g_win32_app_info_should_show;*/
/*  iface->set_as_default_for_type = g_win32_app_info_set_as_default_for_type;*/
/*  iface->set_as_default_for_extension = g_win32_app_info_set_as_default_for_extension;*/
/*  iface->add_supports_type = g_win32_app_info_add_supports_type;*/
/*  iface->can_remove_supports_type = g_win32_app_info_can_remove_supports_type;*/
/*  iface->remove_supports_type = g_win32_app_info_remove_supports_type;*/
/*  iface->can_delete = g_win32_app_info_can_delete;*/
/*  iface->do_delete = g_win32_app_info_delete;*/
  iface->get_commandline = g_win32_app_info_get_commandline;
  iface->get_display_name = g_win32_app_info_get_display_name;
/*  iface->set_as_last_used_for_type = g_win32_app_info_set_as_last_used_for_type;*/
  iface->get_supported_types = g_win32_app_info_get_supported_types;
}

GAppInfo *
g_app_info_get_default_for_uri_scheme (const char *uri_scheme)
{
  GWin32AppInfoURLSchema *scheme = NULL;
  char *scheme_down;
  GAppInfo *result;
  GWin32AppInfoShellVerb *shverb;

  scheme_down = g_utf8_casefold (uri_scheme, -1);

  if (!scheme_down)
    return NULL;

  if (strcmp (scheme_down, "file") == 0)
    {
      g_free (scheme_down);

      return NULL;
    }

  g_win32_appinfo_init ();
  G_LOCK (gio_win32_appinfo);

  g_set_object (&scheme, g_hash_table_lookup (urls, scheme_down));
  g_free (scheme_down);

  G_UNLOCK (gio_win32_appinfo);

  result = NULL;

  if (scheme != NULL &&
      scheme->chosen_handler != NULL &&
      scheme->chosen_handler->verbs->len > 0 &&
      (shverb = _verb_idx (scheme->chosen_handler->verbs, 0))->app != NULL)
    result = g_win32_app_info_new_from_app (shverb->app,
                                            scheme->chosen_handler);

  g_clear_object (&scheme);

  return result;
}

GAppInfo *
g_app_info_get_default_for_type (const char *content_type,
                                 gboolean    must_support_uris)
{
  GWin32AppInfoFileExtension *ext = NULL;
  char *ext_down;
  GAppInfo *result;
  GWin32AppInfoShellVerb *shverb;

  ext_down = g_utf8_casefold (content_type, -1);

  if (!ext_down)
    return NULL;

  g_win32_appinfo_init ();
  G_LOCK (gio_win32_appinfo);

  /* Assuming that "content_type" is a file extension, not a MIME type */
  g_set_object (&ext, g_hash_table_lookup (extensions, ext_down));
  g_free (ext_down);

  G_UNLOCK (gio_win32_appinfo);

  if (ext == NULL)
    return NULL;

  result = NULL;

  if (ext->chosen_handler != NULL &&
      ext->chosen_handler->verbs->len > 0 &&
      (shverb = _verb_idx (ext->chosen_handler->verbs, 0))->app != NULL &&
      (!must_support_uris ||
       g_win32_app_supports_uris (shverb->app)))
    result = g_win32_app_info_new_from_app (shverb->app,
                                            ext->chosen_handler);
  else
    {
      GHashTableIter iter;
      GWin32AppInfoHandler *handler;

      g_hash_table_iter_init (&iter, ext->handlers);

      while (result == NULL &&
             g_hash_table_iter_next (&iter, NULL, (gpointer *) &handler))
        {
          if (handler->verbs->len == 0)
            continue;

          shverb = _verb_idx (handler->verbs, 0);

          if (shverb->app &&
              (!must_support_uris ||
               g_win32_app_supports_uris (shverb->app)))
            result = g_win32_app_info_new_from_app (shverb->app, handler);
        }
    }

  g_clear_object (&ext);

  return result;
}

GList *
g_app_info_get_all (void)
{
  GHashTableIter iter;
  gpointer value;
  GList *infos;
  GList *apps;
  GList *apps_i;

  g_win32_appinfo_init ();
  G_LOCK (gio_win32_appinfo);

  apps = NULL;
  g_hash_table_iter_init (&iter, apps_by_id);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    apps = g_list_prepend (apps, g_object_ref (G_OBJECT (value)));

  G_UNLOCK (gio_win32_appinfo);

  infos = NULL;
  for (apps_i = apps; apps_i; apps_i = apps_i->next)
    infos = g_list_prepend (infos,
                            g_win32_app_info_new_from_app (apps_i->data, NULL));

  g_list_free_full (apps, g_object_unref);

  return infos;
}

GList *
g_app_info_get_all_for_type (const char *content_type)
{
  GWin32AppInfoFileExtension *ext = NULL;
  char *ext_down;
  GWin32AppInfoHandler *handler;
  GHashTableIter iter;
  /* TODO: consider using GHashTable instead of a GList */
  GList *apps = NULL;
  GList *result;
  GWin32AppInfoShellVerb *shverb;

  ext_down = g_utf8_casefold (content_type, -1);

  if (!ext_down)
    return NULL;

  g_win32_appinfo_init ();
  G_LOCK (gio_win32_appinfo);

  /* Assuming that "content_type" is a file extension, not a MIME type */
  g_set_object (&ext, g_hash_table_lookup (extensions, ext_down));
  g_free (ext_down);

  G_UNLOCK (gio_win32_appinfo);

  if (ext == NULL)
    return NULL;

  result = NULL;

  if (ext->chosen_handler != NULL &&
      ext->chosen_handler->verbs->len > 0 &&
      (shverb = _verb_idx (ext->chosen_handler->verbs, 0))->app != NULL)
    {
      apps = g_list_prepend (apps, shverb->app);
      result = g_list_prepend (result,
                               g_win32_app_info_new_from_app (shverb->app,
                                                              ext->chosen_handler));
    }

  g_hash_table_iter_init (&iter, ext->handlers);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &handler))
    {
      gsize vi;

      for (vi = 0; vi < handler->verbs->len; vi++)
        {
          shverb = _verb_idx (handler->verbs, vi);

          if (shverb->app == NULL ||
              g_list_find (apps, shverb->app) != NULL)
            continue;

          apps = g_list_prepend (apps, shverb->app);
          result = g_list_prepend (result,
                                   g_win32_app_info_new_from_app (shverb->app,
                                                                  handler));
        }
    }

  g_clear_object (&ext);
  result = g_list_reverse (result);

  return result;
}

GList *
g_app_info_get_fallback_for_type (const gchar *content_type)
{
  /* TODO: fix this once gcontenttype support is improved */
  return g_app_info_get_all_for_type (content_type);
}

GList *
g_app_info_get_recommended_for_type (const gchar *content_type)
{
  /* TODO: fix this once gcontenttype support is improved */
  return g_app_info_get_all_for_type (content_type);
}

void
g_app_info_reset_type_associations (const char *content_type)
{
  /* nothing to do */
}
