/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2012 Red Hat, Inc.
 * Copyright © 2012 Canonical Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 *
 * Authors: Colin Walters <walters@verbum.org>
 *          Ryan Lortie <desrt@desrt.ca>
 */

/**
 * SECTION:gsubprocess
 * @title: GSubprocess Launcher
 * @short_description: Environment options for launching a child process
 *
 * This class contains a set of options for launching child processes,
 * such as where its standard input and output will be directed, the
 * argument list, the environment, and more.
 *
 * While the #GSubprocess class has high level functions covering
 * popular cases, use of this class allows access to more advanced
 * options.  It can also be used to launch multiple subprocesses with
 * a similar configuration.
 *
 * Since: 2.36
 */

#define ALL_STDIN_FLAGS         (G_SUBPROCESS_FLAGS_STDIN_PIPE |        \
                                 G_SUBPROCESS_FLAGS_STDIN_INHERIT)
#define ALL_STDOUT_FLAGS        (G_SUBPROCESS_FLAGS_STDOUT_PIPE |       \
                                 G_SUBPROCESS_FLAGS_STDOUT_SILENCE)
#define ALL_STDERR_FLAGS        (G_SUBPROCESS_FLAGS_STDERR_PIPE |       \
                                 G_SUBPROCESS_FLAGS_STDERR_SILENCE |    \
                                 G_SUBPROCESS_FLAGS_STDERR_MERGE)

#include "config.h"

#include "gsubprocesslauncher-private.h"
#include "gioenumtypes.h"
#include "gsubprocess.h"
#include "ginitable.h"

typedef GObjectClass GSubprocessLauncherClass;

G_DEFINE_TYPE (GSubprocessLauncher, g_subprocess_launcher, G_TYPE_OBJECT);

static void
g_subprocess_launcher_finalize (GObject *object)
{
  GSubprocessLauncher *self = G_SUBPROCESS_LAUNCHER (object);

  g_strfreev (self->envp);
  g_free (self->cwd);

  g_free (self->stdin_path);
  g_free (self->stdout_path);
  g_free (self->stderr_path);

  if (self->child_setup_destroy_notify)
    (* self->child_setup_destroy_notify) (self->child_setup_user_data);

  if (G_OBJECT_CLASS (g_subprocess_launcher_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (g_subprocess_launcher_parent_class)->finalize (object);
}

static void
g_subprocess_launcher_init (GSubprocessLauncher  *self)
{
  self->envp = g_listenv ();

  self->stdin_fd = -1;
  self->stdout_fd = -1;
  self->stderr_fd = -1;
}

static void
g_subprocess_launcher_class_init (GSubprocessLauncherClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->finalize = g_subprocess_launcher_finalize;
}

/**
 * g_subprocess_launcher_new:
 *
 * Creates a new #GSubprocessLauncher.
 *
 * The launcher is created with the default options.  A copy of the
 * environment of the calling process is made at the time of this call
 * and will be used as the environment that the process is launched in.
 *
 * Since: 2.36
 **/
GSubprocessLauncher *
g_subprocess_launcher_new (void)
{
  return g_object_new (G_TYPE_SUBPROCESS_LAUNCHER, NULL);
}


/**
 * g_subprocess_launcher_set_environ:
 * @self: a #GSubprocess
 * @environ: the replacement environment
 *
 * Replace the entire environment of processes launched from this
 * launcher with the given 'environ' variable.
 *
 * Typically you will build this variable by using g_listenv() to copy
 * the process 'environ' and using the functions g_environ_setenv(),
 * g_environ_unsetenv(), etc.
 *
 * As an alternative, you can use g_subprocess_launcher_setenv(),
 * g_subprocess_launcher_unsetenv(), etc.
 *
 * All strings in this array are expected to be in the GLib file name
 * encoding.  On UNIX, this means that they can be arbitrary byte
 * strings.  On Windows, they should be in UTF-8.
 *
 * Since: 2.36
 **/
void
g_subprocess_launcher_set_environ (GSubprocessLauncher  *self,
                                   gchar               **environ)
{
  g_strfreev (self->envp);
  self->envp = g_strdupv (environ);
}

/**
 * g_subprocess_launcher_setenv:
 * @self: a #GSubprocess
 * @variable: the environment variable to set, must not contain '='
 * @value: the new value for the variable
 * @overwrite: whether to change the variable if it already exists
 *
 * Sets the environment variable @variable in the environment of
 * processes launched from this launcher.
 *
 * Both the variable's name and value should be in the GLib file name
 * encoding. On UNIX, this means that they can be arbitrary byte
 * strings. On Windows, they should be in UTF-8.
 *
 *
 * Since: 2.36
 **/
void
g_subprocess_launcher_setenv (GSubprocessLauncher *self,
                              const gchar         *variable,
                              const gchar         *value,
                              gboolean             overwrite)
{
  self->envp = g_environ_setenv (self->envp, variable, value, overwrite);
}

/**
 * g_subprocess_launcher_unsetsenv:
 * @self: a #GSubprocess
 * @variable: the environment variable to unset, must not contain '='
 *
 * Removes the environment variable @variable from the environment of
 * processes launched from this launcher.
 *
 * The variable name should be in the GLib file name encoding.  On UNIX,
 * this means that they can be arbitrary byte strings.  On Windows, they
 * should be in UTF-8.
 *
 * Since: 2.36
 **/
void
g_subprocess_launcher_unsetenv (GSubprocessLauncher *self,
                                const gchar         *variable)
{
  self->envp = g_environ_unsetenv (self->envp, variable);
}

/**
 * g_subprocess_launcher_getenv:
 * @self: a #GSubprocess
 * @variable: the environment variable to get
 *
 * Returns the value of the environment variable @variable in the
 * environment of processes launched from this launcher.
 *
 * The returned string is in the GLib file name encoding.  On UNIX, this
 * means that it can be an arbitrary byte string.  On Windows, it will
 * be UTF-8.
 *
 * Returns: the value of the environment variable, %NULL if unset
 *
 * Since: 2.36
 **/
const gchar *
g_subprocess_launcher_getenv (GSubprocessLauncher *self,
                              const gchar         *variable)
{
  return g_environ_getenv (self->envp, variable);
}

/**
 * g_subprocess_launcher_set_cwd:
 * @self: a #GSubprocess
 * @cwd: the cwd for launched processes
 *
 * Sets the current working directory that processes will be launched
 * with.
 *
 * By default processes are launched with the current working directory
 * of the launching process at the time of launch.
 *
 * Since: 2.36
 **/
void
g_subprocess_launcher_set_cwd (GSubprocessLauncher *self,
                               const gchar         *cwd)
{
  g_free (self->cwd);
  self->cwd = g_strdup (cwd);
}

static gboolean
verify_disposition (const gchar      *stream_name,
                    GSubprocessFlags  filtered_flags,
                    gint              fd,
                    const gchar      *filename)
{
  guint n_bits;

  if (!filtered_flags)
    n_bits = 0;
  else if (((filtered_flags - 1) & filtered_flags) == 0)
    n_bits = 1;
  else
    n_bits = 2; /* ...or more */

  if (n_bits + (fd >= 0) + (filename != NULL) > 1)
    {
      GString *err;

      err = g_string_new (NULL);
      if (n_bits)
        {
          GFlagsClass *class;
          GFlagsValue *value;

          class = g_type_class_peek (G_TYPE_SUBPROCESS_FLAGS);
          while ((value = g_flags_get_first_value (class, filtered_flags)))
            {
              g_string_append_printf (err, " %s", value->value_name);
              filtered_flags &= value->value;
            }

          g_type_class_unref (class);
        }

      if (fd >= 0)
        g_string_append_printf (err, " g_subprocess_launcher_set_%s_fd()", stream_name);

      if (filename)
        g_string_append_printf (err, " g_subprocess_launcher_set_%s_file_path()", stream_name);

      g_critical ("You may specify at most one disposition for the %s stream, but you specified:%s.",
                  stream_name, err->str);
      g_string_free (err, TRUE);

      return FALSE;
    }

  return TRUE;
}

/**
 * g_subprocess_launcher_set_flags:
 * @self: a #GSubprocessLauncher
 * @flags: #GSubprocessFlags
 *
 * Sets the flags on the launcher.
 *
 * The default flags are %G_SUBPROCESS_FLAGS_NONE.
 *
 * You may not set flags that specify conflicting options for how to
 * handle a particular stdio stream (eg: specifying both
 * %G_SUBPROCESS_FLAGS_STDIN_PIPE and
 * %G_SUBPROCESS_FLAGS_STDIN_INHERIT).
 *
 * You may also not set a flag that conflicts with a previous call to a
 * function like g_subprocess_launcher_set_stdin_file_path() or
 * g_subprocess_launcher_set_stdout_fd().
 *
 * Since: 2.36
 **/
void
g_subprocess_launcher_set_flags (GSubprocessLauncher *self,
                                 GSubprocessFlags     flags)
{
  if (verify_disposition ("stdin", flags & ALL_STDIN_FLAGS, self->stdin_fd, self->stdin_path) &&
      verify_disposition ("stdout", flags & ALL_STDOUT_FLAGS, self->stdout_fd, self->stdout_path) &&
      verify_disposition ("stderr", flags & ALL_STDERR_FLAGS, self->stderr_fd, self->stderr_path))
    self->flags = flags;
}

#ifdef G_OS_UNIX
/**
 * g_subprocess_launcher_set_stdin_file_path:
 * @self: a #GSubprocessLauncher
 * @path: a filename or %NULL
 *
 * Sets the file path to use as the stdin for spawned processes.
 *
 * If @path is %NULL then any previously given path is unset.
 *
 * The file must exist or spawning the process will fail.
 *
 * You may not set a stdin file path if a stdin fd is already set or if
 * the launcher flags contain any flags directing stdin elsewhere.
 *
 * This feature is only available on UNIX.
 *
 * Since: 2.36
 **/
void
g_subprocess_launcher_set_stdin_file_path (GSubprocessLauncher *self,
                                           const gchar         *path)
{
  if (verify_disposition ("stdin", self->flags & ALL_STDIN_FLAGS, self->stdin_fd, path))
    {
      g_free (self->stdin_path);
      self->stdin_path = g_strdup (path);
    }
}

/**
 * g_subprocess_launcher_set_stdin_fd:
 * @self: a #GSubprocessLauncher
 * @fd: a file descriptor, or -1
 *
 * Sets the file descriptor to use as the stdin for spawned processes.
 *
 * If @fd is -1 then any previously given fd is unset.
 *
 * Note that if your intention is to have the stdin of the calling
 * process inherited by the child then %G_SUBPROCESS_FLAGS_STDIN_INHERIT
 * is a better way to go about doing that.
 *
 * The passed @fd is noted but will not be touched in the current
 * process.  It is therefore necessary that it be kept open by the
 * caller until the subprocess is spawned.  The file descriptor will
 * also not be explicitly closed on the child side, so it must be marked
 * O_CLOEXEC if that's what you want.
 *
 * You may not set a stdin fd if a stdin file path is already set or if
 * the launcher flags contain any flags directing stdin elsewhere.
 *
 * This feature is only available on UNIX.
 *
 * Since: 2.36
 **/
void
g_subprocess_launcher_set_stdin_fd (GSubprocessLauncher *self,
                                    gint                 fd)
{
  if (verify_disposition ("stdin", self->flags & ALL_STDIN_FLAGS, fd, self->stdin_path))
    self->stdin_fd = fd;
}

/**
 * g_subprocess_launcher_set_stdout_file_path:
 * @self: a #GSubprocessLauncher
 * @path: a filename or %NULL
 *
 * Sets the file path to use as the stdout for spawned processes.
 *
 * If @path is %NULL then any previously given path is unset.
 *
 * The file will be created or truncated when the process is spawned, as
 * would be the case if using '>' at the shell.
 *
 * You may not set a stdout file path if a stdout fd is already set or
 * if the launcher flags contain any flags directing stdout elsewhere.
 *
 * This feature is only available on UNIX.
 *
 * Since: 2.36
 **/
void
g_subprocess_launcher_set_stdout_file_path (GSubprocessLauncher *self,
                                            const gchar         *path)
{
  if (verify_disposition ("stdout", self->flags & ALL_STDIN_FLAGS, self->stdout_fd, path))
    {
      g_free (self->stdout_path);
      self->stdout_path = g_strdup (path);
    }
}

/**
 * g_subprocess_launcher_set_stdout_fd:
 * @self: a #GSubprocessLauncher
 * @fd: a file descriptor, or -1
 *
 * Sets the file descriptor to use as the stdout for spawned processes.
 *
 * If @fd is -1 then any previously given fd is unset.
 *
 * Note that the default behaviour is to pass stdout through to the
 * stdout of the parent process.
 *
 * The passed @fd is noted but will not be touched in the current
 * process.  It is therefore necessary that it be kept open by the
 * caller until the subprocess is spawned.  The file descriptor will
 * also not be explicitly closed on the child side, so it must be marked
 * O_CLOEXEC if that's what you want.
 *
 * You may not set a stdout fd if a stdout file path is already set or
 * if the launcher flags contain any flags directing stdout elsewhere.
 *
 * This feature is only available on UNIX.
 *
 * Since: 2.36
 **/
void
g_subprocess_launcher_set_stdout_fd (GSubprocessLauncher *self,
                                     gint                 fd)
{
  if (verify_disposition ("stdin", self->flags & ALL_STDIN_FLAGS, fd, self->stdin_path))
    self->stdout_fd = fd;
}

/**
 * g_subprocess_launcher_set_stderr_file_path:
 * @self: a #GSubprocessLauncher
 * @path: a filename or %NULL
 *
 * Sets the file path to use as the stderr for spawned processes.
 *
 * If @path is %NULL then any previously given path is unset.
 *
 * The file will be created or truncated when the process is spawned, as
 * would be the case if using '2>' at the shell.
 *
 * If you want to send both stdout and stderr to the same file then use
 * %G_SUBPROCESS_FLAGS_STDERR_MERGE.
 *
 * You may not set a stderr file path if a stderr fd is already set or
 * if the launcher flags contain any flags directing stderr elsewhere.
 *
 * This feature is only available on UNIX.
 *
 * Since: 2.36
 **/
void
g_subprocess_launcher_set_stderr_file_path (GSubprocessLauncher *self,
                                            const gchar         *path)
{
  if (verify_disposition ("stderr", self->flags & ALL_STDIN_FLAGS, self->stderr_fd, path))
    {
      g_free (self->stderr_path);
      self->stderr_path = g_strdup (path);
    }
}

/**
 * g_subprocess_launcher_set_stderr_fd:
 * @self: a #GSubprocessLauncher
 * @fd: a file descriptor, or -1
 *
 * Sets the file descriptor to use as the stderr for spawned processes.
 *
 * If @fd is -1 then any previously given fd is unset.
 *
 * Note that the default behaviour is to pass stderr through to the
 * stderr of the parent process.
 *
 * The passed @fd is noted but will not be touched in the current
 * process.  It is therefore necessary that it be kept open by the
 * caller until the subprocess is spawned.  The file descriptor will
 * also not be explicitly closed on the child side, so it must be marked
 * O_CLOEXEC if that's what you want.
 *
 * You may not set a stderr fd if a stderr file path is already set or
 * if the launcher flags contain any flags directing stderr elsewhere.
 *
 * This feature is only available on UNIX.
 *
 * Since: 2.36
 **/
void
g_subprocess_launcher_set_stderr_fd (GSubprocessLauncher *self,
                                     gint                 fd)
{
  if (verify_disposition ("stdin", self->flags & ALL_STDIN_FLAGS, fd, self->stdin_path))
    self->stderr_fd = fd;
}

/**
 * g_subprocess_launcher_set_child_setup:
 * @self: a #GSubprocessLauncher
 * @child_setup: a #GSpawnChildSetupFunc to use as the child setup function
 * @user_data: user data for @child_setup
 * @destroy_notify: a #GDestroyNotify for @user_data
 *
 * Sets up a child setup function.
 *
 * The child setup function will be called after fork() but before
 * exec() on the child's side.
 *
 * @destroy_notify will not be automatically called on the child's side
 * of the fork().  It will only be called when the last reference on the
 * #GSubprocessLauncher is dropped or when a new child setup function is
 * given.
 *
 * %NULL can be given as @child_setup to disable the functionality.
 *
 * Child setup functions are only available on UNIX.
 *
 * Since: 2.36
 **/
void
g_subprocess_launcher_set_child_setup (GSubprocessLauncher  *self,
                                       GSpawnChildSetupFunc  child_setup,
                                       gpointer              user_data,
                                       GDestroyNotify        destroy_notify)
{
  if (self->child_setup_destroy_notify)
    (* self->child_setup_destroy_notify) (self->child_setup_user_data);

  self->child_setup_func = child_setup;
  self->child_setup_user_data = user_data;
  self->child_setup_destroy_notify = destroy_notify;
}
#endif

GSubprocess *
g_subprocess_launcher_spawn (GSubprocessLauncher  *launcher,
                             GError              **error,
                             const gchar          *argv0,
                             ...)
{
  GSubprocess *result;
  GPtrArray *args;
  const gchar *arg;
  va_list ap;

  g_return_val_if_fail (argv0 != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  args = g_ptr_array_new ();

  va_start (ap, argv0);
  g_ptr_array_add (args, (gchar *) argv0);
  while ((arg = va_arg (ap, const gchar *)))
    g_ptr_array_add (args, (gchar *) arg);

  result = g_subprocess_launcher_spawnv (launcher, (const gchar * const *) args->pdata, error);

  g_ptr_array_free (args, TRUE);

  return result;

}

GSubprocess *
g_subprocess_launcher_spawnv (GSubprocessLauncher  *launcher,
                              const gchar * const  *argv,
                              GError              **error)
{
  GSubprocess *subprocess;

  subprocess = g_object_new (G_TYPE_SUBPROCESS,
                             "argv", argv,
                             "flags", launcher->flags,
                             NULL);
  g_subprocess_set_launcher (subprocess, launcher);

  if (!g_initable_init (G_INITABLE (subprocess), NULL, error))
    {
      g_object_unref (subprocess);
      return NULL;
    }

  return subprocess;
}
