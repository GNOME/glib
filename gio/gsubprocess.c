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
 * @title: GSubprocess
 * @short_description: Create child processes and monitor their status
 *
 * This class wraps the lower-level g_spawn_async_with_pipes() API,
 * providing a more modern GIO-style API, such as returning
 * #GInputStream objects for child output pipes.
 *
 * One major advantage that GIO brings over the core GLib library is
 * comprehensive API for asynchronous I/O, such
 * g_output_stream_splice_async().  This makes GSubprocess
 * significantly more powerful and flexible than equivalent APIs in
 * some other languages such as the <literal>subprocess.py</literal>
 * included with Python.  For example, using #GSubprocess one could
 * create two child processes, reading standard output from the first,
 * processing it, and writing to the input stream of the second, all
 * without blocking the main loop.
 *
 * Since: 2.36
 */

#include "config.h"
#include "gsubprocess.h"
#include "gsubprocesslauncher-private.h"
#include "gasyncresult.h"
#include "giostream.h"
#include "gmemoryinputstream.h"
#include "glibintl.h"
#include "glib-private.h"

#include <string.h>
#ifdef G_OS_UNIX
#include <gio/gunixoutputstream.h>
#include <gio/gfiledescriptorbased.h>
#include <gio/gunixinputstream.h>
#include <gstdio.h>
#include <glib-unix.h>
#include <fcntl.h>
#endif
#ifdef G_OS_WIN32
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include "giowin32-priv.h"
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

/* A GSubprocess can have two possible states: running and not.
 *
 * These two states are reflected by the value of 'pid'.  If it is
 * non-zero then the process is running, with that pid.
 *
 * When a GSubprocess is first created with g_object_new() it is not
 * running.  When it is finalized, it is also not running.
 *
 * During initable_init(), if the g_spawn() is successful then we
 * immediately register a child watch and take an extra ref on the
 * subprocess.  That reference doesn't drop until the child has quit,
 * which is why finalize can only happen in the non-running state.  In
 * the event that the g_spawn() failed we will still be finalizing a
 * non-running GSubprocess (before returning from g_subprocess_new())
 * with NULL.
 *
 * We make extensive use of the glib worker thread to guarentee
 * race-free operation.  As with all child watches, glib calls waitpid()
 * in the worker thread.  It reports the child exiting to us via the
 * worker thread (which means that we can do synchronous waits without
 * running a separate loop).  We also send signals to the child process
 * via the worker thread so that we don't race with waitpid() and
 * accidentally send a signal to an already-reaped child.
 */
static void initable_iface_init (GInitableIface         *initable_iface);

typedef GObjectClass GSubprocessClass;

struct _GSubprocess
{
  GObject parent;

  /* only used during construction */
  GSubprocessLauncher *launcher;
  GSubprocessFlags flags;
  gchar **argv;

  /* state tracking variables */
  int exit_status;
  GPid pid;

  /* list of GTask */
  GMutex pending_waits_lock;
  GSList *pending_waits;

  /* These are the streams created if a pipe is requested via flags. */
  GOutputStream *stdin_pipe;
  GInputStream  *stdout_pipe;
  GInputStream  *stderr_pipe;
};

G_DEFINE_TYPE_WITH_CODE (GSubprocess, g_subprocess, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init));

enum
{
  PROP_0,
  PROP_FLAGS,
  PROP_ARGV,
  N_PROPS
};

static GParamSpec *g_subprocess_pspecs[N_PROPS];


typedef struct
{
  gint                 fds[3];
  GSpawnChildSetupFunc child_setup_func;
  gpointer             child_setup_data;
} ChildData;

static void
child_setup (gpointer user_data)
{
  ChildData *child_data = user_data;
  gint i;

  /* We're on the child side now.  "Rename" the file descriptors in
   * child_data.fds[] to stdin/stdout/stderr.
   *
   * We don't close the originals.  It's possible that the originals
   * should not be closed and if they should be closed then they should
   * have been created O_CLOEXEC.
   */
  for (i = 0; i < 3; i++)
    if (child_data->fds[i] != -1 && child_data->fds[i] != i)
      {
        gint result;

        do
          result = dup2 (child_data->fds[i], i);
        while (result == -1 && errno == EINTR);
      }

  if (child_data->child_setup_func)
    child_data->child_setup_func (child_data->child_setup_data);
}

static GInputStream *
platform_input_stream_from_spawn_fd (gint fd)
{
  if (fd < 0)
    return NULL;

#ifdef G_OS_UNIX
  return g_unix_input_stream_new (fd, TRUE);
#else
  return g_win32_input_stream_new_from_fd (fd, TRUE);
#endif
}

static GOutputStream *
platform_output_stream_from_spawn_fd (gint fd)
{
  if (fd < 0)
    return NULL;

#ifdef G_OS_UNIX
  return g_unix_output_stream_new (fd, TRUE);
#else
  return g_win32_output_stream_new_from_fd (fd, TRUE);
#endif
}

#ifdef G_OS_UNIX
static gint
unix_open_file (const char  *filename,
                gint         mode,
                GError     **error)
{
  gint my_fd;

  my_fd = g_open (filename, mode | O_BINARY | O_CLOEXEC, 0666);

  /* If we return -1 we should also set the error */
  if (my_fd < 0)
    {
      gint saved_errno = errno;
      char *display_name;

      display_name = g_filename_display_name (filename);
      g_set_error (error, G_IO_ERROR, g_io_error_from_errno (saved_errno),
                   _("Error opening file '%s': %s"), display_name,
                   g_strerror (saved_errno));
      g_free (display_name);
      /* fall through... */
    }

  return my_fd;
}
#endif


static void
g_subprocess_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GSubprocess *self = G_SUBPROCESS (object);

  switch (prop_id)
    {
    case PROP_FLAGS:
      self->flags = g_value_get_flags (value);
      break;

    case PROP_ARGV:
      self->argv = g_value_dup_boxed (value);
      break;

    default:
      g_assert_not_reached ();
    }
}

static gboolean
g_subprocess_exited (GPid     pid,
                     gint     exit_status,
                     gpointer user_data)
{
  GSubprocess *self = user_data;
  GSList *tasks;

  g_assert (self->pid == pid);

  g_mutex_lock (&self->pending_waits_lock);
  self->exit_status = exit_status;
  tasks = self->pending_waits;
  self->pending_waits = NULL;
  self->pid = 0;
  g_mutex_unlock (&self->pending_waits_lock);

  while (tasks)
    {
      g_task_return_boolean (tasks->data, TRUE);
      tasks = g_slist_delete_link (tasks, tasks);
    }

  return FALSE;
}

static gboolean
initable_init (GInitable     *initable,
               GCancellable  *cancellable,
               GError       **error)
{
  GSubprocess *self = G_SUBPROCESS (initable);
  ChildData child_data = { { -1, -1, -1 } };
  gint *pipe_ptrs[3] = { NULL, NULL, NULL };
  gint pipe_fds[3] = { -1, -1, -1 };
  gint close_fds[3] = { -1, -1, -1 };
  GSpawnFlags spawn_flags = 0;
  gboolean success = FALSE;
  gint i;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;

  /* We must setup the three fds that will end up in the child as stdin,
   * stdout and stderr.
   *
   * First, stdin.
   */
  if (self->flags & G_SUBPROCESS_FLAGS_STDIN_INHERIT)
    spawn_flags |= G_SPAWN_CHILD_INHERITS_STDIN;
  else if (self->flags & G_SUBPROCESS_FLAGS_STDIN_PIPE)
    pipe_ptrs[0] = &pipe_fds[0];
#ifdef G_OS_UNIX
  else if (self->launcher)
    {
      if (self->launcher->stdin_fd != -1)
        child_data.fds[0] = self->launcher->stdin_fd;
      else if (self->launcher->stdin_path != NULL)
        {
          child_data.fds[0] = close_fds[0] = unix_open_file (self->launcher->stdin_path, O_RDONLY, error);
          if (child_data.fds[0] == -1)
            goto out;
        }
    }
#endif

  /* Next, stdout. */
  if (self->flags & G_SUBPROCESS_FLAGS_STDOUT_SILENCE)
    spawn_flags |= G_SPAWN_STDOUT_TO_DEV_NULL;
  else if (self->flags & G_SUBPROCESS_FLAGS_STDOUT_PIPE)
    pipe_ptrs[1] = &pipe_fds[1];
#ifdef G_OS_UNIX
  else if (self->launcher)
    {
      if (self->launcher->stdout_fd != -1)
        child_data.fds[1] = self->launcher->stdout_fd;
      else if (self->launcher->stdout_path != NULL)
        {
          child_data.fds[1] = close_fds[1] = unix_open_file (self->launcher->stdout_path, O_CREAT | O_WRONLY, error);
          if (child_data.fds[1] == -1)
            goto out;
        }
    }
#endif

  /* Finally, stderr. */
  if (self->flags & G_SUBPROCESS_FLAGS_STDERR_SILENCE)
    spawn_flags |= G_SPAWN_STDERR_TO_DEV_NULL;
  else if (self->flags & G_SUBPROCESS_FLAGS_STDERR_PIPE)
    pipe_ptrs[2] = &pipe_fds[2];
#ifdef G_OS_UNIX
  if (self->launcher)
    {
      if (self->launcher->stderr_fd != -1)
        child_data.fds[2] = self->launcher->stderr_fd;
      else if (self->launcher->stderr_path != NULL)
        {
          child_data.fds[2] = close_fds[2] = unix_open_file (self->launcher->stderr_path, O_CREAT | O_WRONLY, error);
          if (child_data.fds[2] == -1)
            goto out;
        }
    }
#endif

  if (self->flags & G_SUBPROCESS_FLAGS_SEARCH_PATH)
    spawn_flags |= G_SPAWN_SEARCH_PATH;

  else if (self->flags & G_SUBPROCESS_FLAGS_SEARCH_PATH_FROM_ENVP)
    spawn_flags |= G_SPAWN_SEARCH_PATH_FROM_ENVP;

  spawn_flags |= G_SPAWN_LEAVE_DESCRIPTORS_OPEN;
  spawn_flags |= G_SPAWN_DO_NOT_REAP_CHILD;
  spawn_flags |= G_SPAWN_CLOEXEC_PIPES;

  child_data.child_setup_func = self->launcher ? self->launcher->child_setup_func : NULL;
  child_data.child_setup_data = self->launcher ? self->launcher->child_setup_user_data : NULL;
  success = g_spawn_async_with_pipes (self->launcher ? self->launcher->cwd : NULL,
                                      self->argv,
                                      self->launcher ? self->launcher->envp : NULL,
                                      spawn_flags,
                                      child_setup, &child_data,
                                      &self->pid,
                                      pipe_ptrs[0], pipe_ptrs[1], pipe_ptrs[2],
                                      error);
  g_assert (success == (self->pid != 0));

  if (success)
    {
      GMainContext *worker_context;
      GSource *source;

      worker_context = GLIB_PRIVATE_CALL (g_get_worker_context) ();
      source = g_child_watch_source_new (self->pid);
      g_source_set_callback (source, (GSourceFunc) g_subprocess_exited, g_object_ref (self), g_object_unref);
      g_source_attach (source, worker_context);
      g_source_unref (source);
    }

out:
  /* we don't need this past init... */
  self->launcher = NULL;

  for (i = 0; i < 3; i++)
    if (close_fds[i] != -1)
      close (close_fds[i]);

  self->stdin_pipe = platform_output_stream_from_spawn_fd (pipe_fds[0]);
  self->stdout_pipe = platform_input_stream_from_spawn_fd (pipe_fds[1]);
  self->stderr_pipe = platform_input_stream_from_spawn_fd (pipe_fds[2]);

  return success;
}

static void
g_subprocess_finalize (GObject *object)
{
  GSubprocess *self = G_SUBPROCESS (object);

  g_assert (self->pid == 0);

  g_clear_object (&self->stdin_pipe);
  g_clear_object (&self->stdout_pipe);
  g_clear_object (&self->stderr_pipe);
  g_free (self->argv);

  if (G_OBJECT_CLASS (g_subprocess_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (g_subprocess_parent_class)->finalize (object);
}

static void
g_subprocess_init (GSubprocess  *self)
{
}

static void
initable_iface_init (GInitableIface *initable_iface)
{
  initable_iface->init = initable_init;
}

static void
g_subprocess_class_init (GSubprocessClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->finalize = g_subprocess_finalize;
  gobject_class->set_property = g_subprocess_set_property;

  g_subprocess_pspecs[PROP_FLAGS] = g_param_spec_flags ("flags", P_("Flags"), P_("Subprocess flags"),
                                                        G_TYPE_SUBPROCESS_FLAGS, 0, G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_subprocess_pspecs[PROP_ARGV] = g_param_spec_boxed ("argv", P_("Arguments"), P_("Argument vector"),
                                                       G_TYPE_STRV, G_PARAM_WRITABLE |
                                                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, g_subprocess_pspecs);
}

/**
 * g_subprocess_new: (skip)
 *
 * Create a new process with the given flags and varargs argument list.
 *
 * The argument list must be terminated with %NULL.
 *
 * Returns: A newly created #GSubprocess, or %NULL on error (and @error
 *   will be set)
 *
 * Since: 2.36
 */
GSubprocess *
g_subprocess_new (GSubprocessFlags   flags,
                  GError           **error,
                  const gchar       *argv0,
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

  result = g_subprocess_newv ((const gchar * const *) args->pdata, flags, error);

  g_ptr_array_free (args, TRUE);

  return result;
}

/**
 * g_subprocess_newv:
 *
 * Create a new process with the given flags and argument list.
 *
 * The argument list is expected to be %NULL-terminated.
 *
 * Returns: A newly created #GSubprocess, or %NULL on error (and @error
 *   will be set)
 *
 * Since: 2.36
 * Rename to: g_subprocess_new
 */
GSubprocess *
g_subprocess_newv (const gchar * const  *argv,
                   GSubprocessFlags      flags,
                   GError              **error)
{
  return g_initable_new (G_TYPE_SUBPROCESS, NULL, error,
                         "argv", argv,
                         "flags", flags,
                         NULL);
}

GOutputStream *
g_subprocess_get_stdin_pipe (GSubprocess       *self)
{
  g_return_val_if_fail (G_IS_SUBPROCESS (self), NULL);
  g_return_val_if_fail (self->stdin_pipe, NULL);

  return self->stdin_pipe;
}

GInputStream *
g_subprocess_get_stdout_pipe (GSubprocess      *self)
{
  g_return_val_if_fail (G_IS_SUBPROCESS (self), NULL);
  g_return_val_if_fail (self->stdout_pipe, NULL);

  return self->stdout_pipe;
}

GInputStream *
g_subprocess_get_stderr_pipe (GSubprocess      *self)
{
  g_return_val_if_fail (G_IS_SUBPROCESS (self), NULL);
  g_return_val_if_fail (self->stderr_pipe, NULL);

  return self->stderr_pipe;
}

static void
g_subprocess_wait_cancelled (GCancellable *cancellable,
                             gpointer      user_data)
{
  GTask *task = user_data;
  GSubprocess *self;

  self = g_task_get_source_object (task);

  g_mutex_lock (&self->pending_waits_lock);
  self->pending_waits = g_slist_remove (self->pending_waits, task);
  g_mutex_unlock (&self->pending_waits_lock);

  g_task_return_boolean (task, FALSE);
  g_object_unref (task);
}

void
g_subprocess_wait_async (GSubprocess         *self,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  GTask *task;

  task = g_task_new (self, cancellable, callback, user_data);

  g_mutex_lock (&self->pending_waits_lock);
  if (self->pid)
    {
      /* Only bother with cancellable if we're putting it in the list.
       * If not, it's going to dispatch immediately anyway and we will
       * see the cancellation in the _finish().
       */
      if (cancellable)
        g_signal_connect_object (cancellable, "cancelled", G_CALLBACK (g_subprocess_wait_cancelled), task, 0);

      self->pending_waits = g_slist_prepend (self->pending_waits, task);
      task = NULL;
    }
  g_mutex_unlock (&self->pending_waits_lock);

  /* If we still have task then it's because did_exit is already TRUE */
  if (task != NULL)
    {
      g_task_return_boolean (task, TRUE);
      g_object_unref (task);
    }
}

gboolean
g_subprocess_wait_finish (GSubprocess   *self,
                          GAsyncResult  *result,
                          GError       **error)
{
  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
g_subprocess_on_sync_wait_complete (GObject       *object,
                                    GAsyncResult  *result,
                                    gpointer       user_data)
{
  GAsyncResult **result_ptr = user_data;

  *result_ptr = g_object_ref (result);
}

/**
 * g_subprocess_wait:
 * @self: a #GSubprocess
 * @out_exit_status: (out): Platform-specific exit code
 * @cancellable: a #GCancellable
 * @error: a #GError
 *
 * Synchronously wait for the subprocess to terminate, returning the
 * status code in @out_exit_status.  See the documentation of
 * g_spawn_check_exit_status() for how to interpret it.  Note that if
 * @error is set, then @out_exit_status will be left uninitialized.
 *
 * Returns: %TRUE on success, %FALSE if @cancellable was cancelled
 *
 * Since: 2.36
 */
gboolean
g_subprocess_wait (GSubprocess   *self,
                   GCancellable  *cancellable,
                   GError       **error)
{
  GAsyncResult *result = NULL;
  GMainContext *context;
  gboolean success;

  g_return_val_if_fail (G_IS_SUBPROCESS (self), FALSE);

  /* Synchronous waits are actually the 'more difficult' case because we
   * need to deal with the possibility of cancellation.  That more or
   * less implies that we need a main context (to dispatch either of the
   * possible reasons for the operation ending).
   *
   * So we make one and then do this async...
   */

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;

  /* We can shortcut in the case that the process already quit (but only
   * after we checked the cancellable).
   */
  if (self->pid == 0)
    return TRUE;

  /* Otherwise, we need to do this the long way... */
  context = g_main_context_new ();
  g_main_context_push_thread_default (context);

  g_subprocess_wait_async (self, cancellable, g_subprocess_on_sync_wait_complete, &result);

  while (!result)
    g_main_context_iteration (context, TRUE);

  g_main_context_pop_thread_default (context);
  g_main_context_unref (context);

  success = g_subprocess_wait_finish (self, result, error);
  g_object_unref (result);

  return success;
}

/**
 * g_subprocess_wait_sync_check:
 * @self: a #GSubprocess
 * @cancellable: a #GCancellable
 * @error: a #GError
 *
 * Combines g_subprocess_wait_sync() with g_spawn_check_exit_status().
 *
 * Returns: %TRUE on success, %FALSE if process exited abnormally, or @cancellable was cancelled
 *
 * Since: 2.36
 */
gboolean
g_subprocess_wait_check (GSubprocess   *self,
                         GCancellable  *cancellable,
                         GError       **error)
{
  return g_subprocess_wait (self, cancellable, error) &&
         g_spawn_check_exit_status (self->exit_status, error);
}

void
g_subprocess_wait_check_async (GSubprocess         *self,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_subprocess_wait_async (self, cancellable, callback, user_data);
}

gboolean
g_subprocess_wait_check_finish (GSubprocess   *self,
                                GAsyncResult  *result,
                                GError       **error)
{
  return g_subprocess_wait_finish (self, result, error) &&
         g_spawn_check_exit_status (self->exit_status, error);
}

#ifdef G_OS_UNIX
typedef struct
{
  GSubprocess *subprocess;
  gint signalnum;
} SignalRecord;

static gboolean
g_subprocess_actually_send_signal (gpointer user_data)
{
  SignalRecord *signal_record = user_data;

  /* The pid is set to zero from the worker thread as well, so we don't
   * need to take a lock in order to prevent it from changing under us.
   */
  if (signal_record->subprocess->pid)
    kill (signal_record->subprocess->pid, signal_record->signalnum);

  g_object_unref (signal_record->subprocess);

  g_slice_free (SignalRecord, signal_record);

  return FALSE;
}

static void
g_subprocess_dispatch_signal (GSubprocess *self,
                              gint         signalnum)
{
  SignalRecord signal_record = { g_object_ref (self), signalnum };

  g_return_if_fail (G_IS_SUBPROCESS (self));

  /* This MUST be a lower priority than the priority that the child
   * watch source uses in initable_init().
   *
   * Reaping processes, reporting the results back to GSubprocess and
   * sending signals is all done in the glib worker thread.  We cannot
   * have a kill() done after the reap and before the report without
   * risking killing a process that's no longer there so the kill()
   * needs to have the lower priority.
   *
   * G_PRIORITY_HIGH_IDLE is lower priority than G_PRIORITY_DEFAULT.
   */
  g_main_context_invoke_full (GLIB_PRIVATE_CALL (g_get_worker_context) (),
                              G_PRIORITY_HIGH_IDLE,
                              g_subprocess_actually_send_signal,
                              g_slice_dup (SignalRecord, &signal_record),
                              NULL);
}

/**
 * g_subprocess_send_signal:
 * @self: a #GSubprocess
 * @signal_num: the signal number to send
 *
 * Sends the UNIX signal @signal_num to the subprocess, if it is still
 * running.
 *
 * This API is race-free.  If the subprocess has terminated, it will not
 * be signalled.
 *
 * This API is not available on Windows.
 *
 * Since: 2.36
 **/
void
g_subprocess_send_signal (GSubprocess *self,
                          gint         signal_num)
{
  g_return_if_fail (G_IS_SUBPROCESS (self));

  g_subprocess_dispatch_signal (self, signal_num);
}
#endif

/**
 * g_subprocess_force_exit:
 * @self: a #GSubprocess
 *
 * Use an operating-system specific method to attempt an immediate,
 * forceful termination of the process.  There is no mechanism to
 * determine whether or not the request itself was successful;
 * however, you can use g_subprocess_wait() to monitor the status of
 * the process after calling this function.
 *
 * On Unix, this function sends %SIGKILL.
 *
 * Since: 2.36
 **/
void
g_subprocess_force_exit (GSubprocess *self)
{
  g_return_if_fail (G_IS_SUBPROCESS (self));

#ifdef G_OS_UNIX
  g_subprocess_dispatch_signal (self, SIGKILL);
#else
  TerminateProcess (self->pid, 1);
#endif
}

/*< private >*/
void
g_subprocess_set_launcher (GSubprocess         *subprocess,
                           GSubprocessLauncher *launcher)
{
  subprocess->launcher = launcher;
}

