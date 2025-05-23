#include "glib-private.h"
#include <gio/gio.h>
#include <string.h>

#ifdef G_OS_UNIX
#include <sys/wait.h>
#include <glib-unix.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <gio/gfiledescriptorbased.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#ifdef __linux__
#include <sys/ptrace.h>
#endif

/* We write 2^1 + 2^2 ... + 2^10 or 2047 copies of "Hello World!\n"
 * ultimately
 */
#define TOTAL_HELLOS 2047
#define HELLO_WORLD "hello world!\n"

#ifdef G_OS_WIN32
#define LINEEND "\r\n"
#define EXEEXT ".exe"
#define SPLICELEN (TOTAL_HELLOS * (strlen (HELLO_WORLD) + 1)) /* because \r */
#else
#define LINEEND "\n"
#define EXEEXT
#define SPLICELEN (TOTAL_HELLOS * strlen (HELLO_WORLD))
#endif



#ifdef G_OS_WIN32
#define TESTPROG "gsubprocess-testprog.exe"
#else
#define TESTPROG "gsubprocess-testprog"
#endif

static GPtrArray *
get_test_subprocess_args (const char *mode,
                          ...) G_GNUC_NULL_TERMINATED;

static GPtrArray *
get_test_subprocess_args (const char *mode,
                          ...)
{
  GPtrArray *ret;
  char *path;
  va_list args;
  gpointer arg;

  ret = g_ptr_array_new_with_free_func (g_free);

  path = g_test_build_filename (G_TEST_BUILT, TESTPROG, NULL);
  g_ptr_array_add (ret, path);
  g_ptr_array_add (ret, g_strdup (mode));

  va_start (args, mode);
  while ((arg = va_arg (args, gpointer)) != NULL)
    g_ptr_array_add (ret, g_strdup (arg));
  va_end (args);

  g_ptr_array_add (ret, NULL);
  return ret;
}

static void
test_noop (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GPtrArray *args;
  GSubprocess *proc;

  args = get_test_subprocess_args ("noop", NULL);
  proc = g_subprocess_newv ((const gchar * const *) args->pdata, G_SUBPROCESS_FLAGS_NONE, error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  g_subprocess_wait_check (proc, NULL, error);
  g_assert_no_error (local_error);
  g_assert_true (g_subprocess_get_successful (proc));

  g_object_unref (proc);
}

static void
check_ready (GObject      *source,
             GAsyncResult *res,
             gpointer      user_data)
{
  gboolean ret;
  GError *error = NULL;

  ret = g_subprocess_wait_check_finish (G_SUBPROCESS (source),
                                        res,
                                        &error);
  g_assert_true (ret);
  g_assert_no_error (error);

  g_object_unref (source);
}

static void
test_noop_all_to_null (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GPtrArray *args;
  GSubprocess *proc;

  args = get_test_subprocess_args ("noop", NULL);
  proc = g_subprocess_newv ((const gchar * const *) args->pdata,
                            G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_SILENCE,
                            error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  g_subprocess_wait_check_async (proc, NULL, check_ready, NULL);
}

static void
test_noop_no_wait (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GPtrArray *args;
  GSubprocess *proc;

  args = get_test_subprocess_args ("noop", NULL);
  proc = g_subprocess_newv ((const gchar * const *) args->pdata, G_SUBPROCESS_FLAGS_NONE, error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  g_object_unref (proc);
}

static void
test_noop_stdin_inherit (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GPtrArray *args;
  GSubprocess *proc;

  args = get_test_subprocess_args ("noop", NULL);
  proc = g_subprocess_newv ((const gchar * const *) args->pdata, G_SUBPROCESS_FLAGS_STDIN_INHERIT, error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  g_subprocess_wait_check (proc, NULL, error);
  g_assert_no_error (local_error);

  g_object_unref (proc);
}

#ifdef G_OS_UNIX
static void
test_search_path (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GSubprocess *proc;

  proc = g_subprocess_new (G_SUBPROCESS_FLAGS_NONE, error, "true", NULL);
  g_assert_no_error (local_error);

  g_subprocess_wait_check (proc, NULL, error);
  g_assert_no_error (local_error);

  g_object_unref (proc);
}

static void
test_search_path_from_envp (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GSubprocessLauncher *launcher;
  GSubprocess *proc;
  const char *path;

  path = g_test_get_dir (G_TEST_BUILT);

  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_SEARCH_PATH_FROM_ENVP);
  g_subprocess_launcher_setenv (launcher, "PATH", path, TRUE);

  proc = g_subprocess_launcher_spawn (launcher, error, TESTPROG, "exit1", NULL);
  g_assert_no_error (local_error);
  g_object_unref (launcher);

  g_subprocess_wait_check (proc, NULL, error);
  g_assert_error (local_error, G_SPAWN_EXIT_ERROR, 1);
  g_clear_error (error);

  g_object_unref (proc);
}
#endif

static void
test_exit1 (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GPtrArray *args;
  GSubprocess *proc;

  args = get_test_subprocess_args ("exit1", NULL);
  proc = g_subprocess_newv ((const gchar * const *) args->pdata, G_SUBPROCESS_FLAGS_NONE, error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  g_subprocess_wait_check (proc, NULL, error);
  g_assert_error (local_error, G_SPAWN_EXIT_ERROR, 1);
  g_clear_error (error);

  g_object_unref (proc);
}

typedef struct {
  GMainLoop    *loop;
  GCancellable *cancellable;
  gboolean      cb_called;
} TestExit1CancelData;

static gboolean
test_exit1_cancel_idle_quit_cb (gpointer user_data)
{
  GMainLoop *loop = user_data;
  g_main_loop_quit (loop);
  return G_SOURCE_REMOVE;
}

static void
test_exit1_cancel_wait_check_cb (GObject      *source,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  GSubprocess *subprocess = G_SUBPROCESS (source);
  TestExit1CancelData *data = user_data;
  gboolean ret;
  GError *error = NULL;

  g_assert_false (data->cb_called);
  data->cb_called = TRUE;

  ret = g_subprocess_wait_check_finish (subprocess, result, &error);
  g_assert_false (ret);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
  g_clear_error (&error);

  g_idle_add (test_exit1_cancel_idle_quit_cb, data->loop);
}

static void
test_exit1_cancel (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GPtrArray *args;
  GSubprocess *proc;
  TestExit1CancelData data = { 0 };

  g_test_bug ("https://bugzilla.gnome.org/show_bug.cgi?id=786456");

  args = get_test_subprocess_args ("exit1", NULL);
  proc = g_subprocess_newv ((const gchar * const *) args->pdata, G_SUBPROCESS_FLAGS_NONE, error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  data.loop = g_main_loop_new (NULL, FALSE);
  data.cancellable = g_cancellable_new ();
  g_subprocess_wait_check_async (proc, data.cancellable, test_exit1_cancel_wait_check_cb, &data);

  g_subprocess_wait_check (proc, NULL, error);
  g_assert_error (local_error, G_SPAWN_EXIT_ERROR, 1);
  g_clear_error (error);

  g_cancellable_cancel (data.cancellable);
  g_main_loop_run (data.loop);

  g_object_unref (proc);
  g_main_loop_unref (data.loop);
  g_clear_object (&data.cancellable);
}

static void
test_exit1_cancel_in_cb_wait_check_cb (GObject      *source,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  GSubprocess *subprocess = G_SUBPROCESS (source);
  TestExit1CancelData *data = user_data;
  gboolean ret;
  GError *error = NULL;

  g_assert_false (data->cb_called);
  data->cb_called = TRUE;

  ret = g_subprocess_wait_check_finish (subprocess, result, &error);
  g_assert_false (ret);
  g_assert_error (error, G_SPAWN_EXIT_ERROR, 1);
  g_clear_error (&error);

  g_cancellable_cancel (data->cancellable);

  g_idle_add (test_exit1_cancel_idle_quit_cb, data->loop);
}

static void
test_exit1_cancel_in_cb (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GPtrArray *args;
  GSubprocess *proc;
  TestExit1CancelData data = { 0 };

  g_test_bug ("https://bugzilla.gnome.org/show_bug.cgi?id=786456");

  args = get_test_subprocess_args ("exit1", NULL);
  proc = g_subprocess_newv ((const gchar * const *) args->pdata, G_SUBPROCESS_FLAGS_NONE, error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  data.loop = g_main_loop_new (NULL, FALSE);
  data.cancellable = g_cancellable_new ();
  g_subprocess_wait_check_async (proc, data.cancellable, test_exit1_cancel_in_cb_wait_check_cb, &data);

  g_subprocess_wait_check (proc, NULL, error);
  g_assert_error (local_error, G_SPAWN_EXIT_ERROR, 1);
  g_clear_error (error);

  g_main_loop_run (data.loop);

  g_object_unref (proc);
  g_main_loop_unref (data.loop);
  g_clear_object (&data.cancellable);
}

static gchar *
splice_to_string (GInputStream   *stream,
                  GError        **error)
{
  GMemoryOutputStream *buffer = NULL;
  char *ret = NULL;

  buffer = (GMemoryOutputStream*)g_memory_output_stream_new (NULL, 0, g_realloc, g_free);
  if (g_output_stream_splice ((GOutputStream*)buffer, stream, 0, NULL, error) < 0)
    goto out;

  if (!g_output_stream_write ((GOutputStream*)buffer, "\0", 1, NULL, error))
    goto out;

  if (!g_output_stream_close ((GOutputStream*)buffer, NULL, error))
    goto out;

  ret = g_memory_output_stream_steal_data (buffer);
 out:
  g_clear_object (&buffer);
  return ret;
}

static void
test_echo1 (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GSubprocess *proc;
  GPtrArray *args;
  GInputStream *stdout_stream;
  gchar *result;

  args = get_test_subprocess_args ("echo", "hello", "world!", NULL);
  proc = g_subprocess_newv ((const gchar * const *) args->pdata, G_SUBPROCESS_FLAGS_STDOUT_PIPE, error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  stdout_stream = g_subprocess_get_stdout_pipe (proc);

  result = splice_to_string (stdout_stream, error);
  g_assert_no_error (local_error);

  g_assert_cmpstr (result, ==, "hello" LINEEND "world!" LINEEND);

  g_free (result);
  g_object_unref (proc);
}

#ifdef G_OS_UNIX
static void
test_echo_merged (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GSubprocess *proc;
  GPtrArray *args;
  GInputStream *stdout_stream;
  gchar *result;

  args = get_test_subprocess_args ("echo-stdout-and-stderr", "merge", "this", NULL);
  proc = g_subprocess_newv ((const gchar * const *) args->pdata,
                            G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_MERGE,
                            error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  stdout_stream = g_subprocess_get_stdout_pipe (proc);
  result = splice_to_string (stdout_stream, error);
  g_assert_no_error (local_error);

  g_assert_cmpstr (result, ==, "merge\nmerge\nthis\nthis\n");

  g_free (result);
  g_object_unref (proc);
}
#endif

typedef struct {
  guint events_pending;
  GMainLoop *loop;
} TestCatData;

static void
test_cat_on_input_splice_complete (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  TestCatData *data = user_data;
  GError *error = NULL;

  (void)g_output_stream_splice_finish ((GOutputStream*)object, result, &error);
  g_assert_no_error (error);

  data->events_pending--;
  if (data->events_pending == 0)
    g_main_loop_quit (data->loop);
}

static void
test_cat_utf8 (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GSubprocess *proc;
  GPtrArray *args;
  GBytes *input_buf;
  GBytes *output_buf;
  GInputStream *input_buf_stream = NULL;
  GOutputStream *output_buf_stream = NULL;
  GOutputStream *stdin_stream = NULL;
  GInputStream *stdout_stream = NULL;
  TestCatData data;

  memset (&data, 0, sizeof (data));
  data.loop = g_main_loop_new (NULL, TRUE);

  args = get_test_subprocess_args ("cat", NULL);
  proc = g_subprocess_newv ((const gchar * const *) args->pdata,
                            G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE,
                            error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  stdin_stream = g_subprocess_get_stdin_pipe (proc);
  stdout_stream = g_subprocess_get_stdout_pipe (proc);

  input_buf = g_bytes_new_static ("hello, world!", strlen ("hello, world!"));
  input_buf_stream = g_memory_input_stream_new_from_bytes (input_buf);
  g_bytes_unref (input_buf);

  output_buf_stream = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);

  g_output_stream_splice_async (stdin_stream, input_buf_stream, G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                G_PRIORITY_DEFAULT, NULL, test_cat_on_input_splice_complete,
                                &data);
  data.events_pending++;
  g_output_stream_splice_async (output_buf_stream, stdout_stream, G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                G_PRIORITY_DEFAULT, NULL, test_cat_on_input_splice_complete,
                                &data);
  data.events_pending++;

  g_main_loop_run (data.loop);

  g_subprocess_wait_check (proc, NULL, error);
  g_assert_no_error (local_error);

  output_buf = g_memory_output_stream_steal_as_bytes ((GMemoryOutputStream*)output_buf_stream);

  g_assert_cmpmem (g_bytes_get_data (output_buf, NULL),
                   g_bytes_get_size (output_buf),
                   "hello, world!", 13);

  g_bytes_unref (output_buf);
  g_main_loop_unref (data.loop);
  g_object_unref (input_buf_stream);
  g_object_unref (output_buf_stream);
  g_object_unref (proc);
}

static gpointer
cancel_soon (gpointer user_data)
{
  GCancellable *cancellable = user_data;

  g_usleep (G_TIME_SPAN_SECOND);
  g_cancellable_cancel (cancellable);
  g_object_unref (cancellable);

  return NULL;
}

static void
test_cat_eof (void)
{
  GCancellable *cancellable;
  GError *error = NULL;
  GSubprocess *cat;
  gboolean result;
  gchar buffer;
  gssize s;

#ifdef G_OS_WIN32
  g_test_skip ("This test has not been ported to Win32");
  return;
#endif

  /* Spawn 'cat' */
  cat = g_subprocess_new (G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE, &error, "cat", NULL);
  g_assert_no_error (error);
  g_assert_nonnull (cat);

  /* Make sure that reading stdout blocks (until we cancel) */
  cancellable = g_cancellable_new ();
  g_thread_unref (g_thread_new ("cancel thread", cancel_soon, g_object_ref (cancellable)));
  s = g_input_stream_read (g_subprocess_get_stdout_pipe (cat), &buffer, sizeof buffer, cancellable, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
  g_assert_cmpint (s, ==, -1);
  g_object_unref (cancellable);
  g_clear_error (&error);

  /* Close the stream (EOF on cat's stdin) */
  result = g_output_stream_close (g_subprocess_get_stdin_pipe (cat), NULL, &error);
  g_assert_no_error (error);
  g_assert_true (result);

  /* Now check that reading cat's stdout gets us an EOF (since it quit) */
  s = g_input_stream_read (g_subprocess_get_stdout_pipe (cat), &buffer, sizeof buffer, NULL, &error);
  g_assert_no_error (error);
  g_assert_false (s);

  /* Check that the process has exited as a result of the EOF */
  result = g_subprocess_wait (cat, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (g_subprocess_get_if_exited (cat));
  g_assert_cmpint (g_subprocess_get_exit_status (cat), ==, 0);
  g_assert_true (result);

  g_object_unref (cat);
}

typedef struct {
  guint events_pending;
  gboolean caught_error;
  GError *error;
  GMainLoop *loop;

  gint counter;
  GOutputStream *first_stdin;
} TestMultiSpliceData;

static void
on_one_multi_splice_done (GObject       *obj,
                          GAsyncResult  *res,
                          gpointer       user_data)
{
  TestMultiSpliceData *data = user_data;

  if (!data->caught_error)
    {
      if (g_output_stream_splice_finish ((GOutputStream*)obj, res, &data->error) < 0)
        data->caught_error = TRUE;
    }

  data->events_pending--;
  if (data->events_pending == 0)
    g_main_loop_quit (data->loop);
}

static gboolean
on_idle_multisplice (gpointer     user_data)
{
  TestMultiSpliceData *data = user_data;

  if (data->counter >= TOTAL_HELLOS || data->caught_error)
    {
      if (!g_output_stream_close (data->first_stdin, NULL, &data->error))
        data->caught_error = TRUE;
      data->events_pending--;
      if (data->events_pending == 0)
        {
          g_main_loop_quit (data->loop);
        }
      return FALSE;
    }
  else
    {
      int i;
      for (i = 0; i < data->counter; i++)
        {
          gsize bytes_written;
          if (!g_output_stream_write_all (data->first_stdin, HELLO_WORLD,
                                          strlen (HELLO_WORLD), &bytes_written,
                                          NULL, &data->error))
            {
              data->caught_error = TRUE;
              return FALSE;
            }
        }
      data->counter *= 2;
      return TRUE;
    }
}

static void
on_subprocess_exited (GObject         *object,
                      GAsyncResult    *result,
                      gpointer         user_data)
{
  GSubprocess *subprocess = G_SUBPROCESS (object);
  TestMultiSpliceData *data = user_data;
  GError *error = NULL;

  if (!g_subprocess_wait_finish (subprocess, result, &error))
    {
      if (!data->caught_error)
        {
          data->caught_error = TRUE;
          g_propagate_error (&data->error, error);
        }
    }
  g_spawn_check_wait_status (g_subprocess_get_status (subprocess), &error);
  g_assert_no_error (error);
  data->events_pending--;
  if (data->events_pending == 0)
    g_main_loop_quit (data->loop);
}

static void
test_multi_1 (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GPtrArray *args;
  GSubprocessLauncher *launcher;
  GSubprocess *first;
  GSubprocess *second;
  GSubprocess *third;
  GOutputStream *first_stdin;
  GInputStream *first_stdout;
  GOutputStream *second_stdin;
  GInputStream *second_stdout;
  GOutputStream *third_stdin;
  GInputStream *third_stdout;
  GOutputStream *membuf;
  TestMultiSpliceData data;
  int splice_flags = G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET;

  args = get_test_subprocess_args ("cat", NULL);
  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  first = g_subprocess_launcher_spawnv (launcher, (const gchar * const *) args->pdata, error);
  g_assert_no_error (local_error);
  second = g_subprocess_launcher_spawnv (launcher, (const gchar * const *) args->pdata, error);
  g_assert_no_error (local_error);
  third = g_subprocess_launcher_spawnv (launcher, (const gchar * const *) args->pdata, error);
  g_assert_no_error (local_error);

  g_ptr_array_free (args, TRUE);

  membuf = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);

  first_stdin = g_subprocess_get_stdin_pipe (first);
  first_stdout = g_subprocess_get_stdout_pipe (first);
  second_stdin = g_subprocess_get_stdin_pipe (second);
  second_stdout = g_subprocess_get_stdout_pipe (second);
  third_stdin = g_subprocess_get_stdin_pipe (third);
  third_stdout = g_subprocess_get_stdout_pipe (third);

  memset (&data, 0, sizeof (data));
  data.loop = g_main_loop_new (NULL, TRUE);
  data.counter = 1;
  data.first_stdin = first_stdin;

  data.events_pending++;
  g_output_stream_splice_async (second_stdin, first_stdout, splice_flags, G_PRIORITY_DEFAULT,
                                NULL, on_one_multi_splice_done, &data);
  data.events_pending++;
  g_output_stream_splice_async (third_stdin, second_stdout, splice_flags, G_PRIORITY_DEFAULT,
                                NULL, on_one_multi_splice_done, &data);
  data.events_pending++;
  g_output_stream_splice_async (membuf, third_stdout, splice_flags, G_PRIORITY_DEFAULT,
                                NULL, on_one_multi_splice_done, &data);

  data.events_pending++;
  g_timeout_add (250, on_idle_multisplice, &data);

  data.events_pending++;
  g_subprocess_wait_async (first, NULL, on_subprocess_exited, &data);
  data.events_pending++;
  g_subprocess_wait_async (second, NULL, on_subprocess_exited, &data);
  data.events_pending++;
  g_subprocess_wait_async (third, NULL, on_subprocess_exited, &data);

  g_main_loop_run (data.loop);

  g_assert_false (data.caught_error);
  g_assert_no_error (data.error);

  g_assert_cmpint (g_memory_output_stream_get_data_size ((GMemoryOutputStream*)membuf), ==, SPLICELEN);

  g_main_loop_unref (data.loop);
  g_object_unref (membuf);
  g_object_unref (launcher);
  g_object_unref (first);
  g_object_unref (second);
  g_object_unref (third);
}

typedef struct {
  GSubprocessFlags flags;
  gboolean is_utf8;
  gboolean running;
  GError *error;
} TestAsyncCommunicateData;

static void
on_communicate_complete (GObject               *proc,
                         GAsyncResult          *result,
                         gpointer               user_data)
{
  TestAsyncCommunicateData *data = user_data;
  GBytes *stdout_bytes = NULL, *stderr_bytes = NULL;
  char *stdout_str = NULL, *stderr_str = NULL;
  const guint8 *stdout_data;
  gsize stdout_len;

  data->running = FALSE;
  if (data->is_utf8)
    (void) g_subprocess_communicate_utf8_finish ((GSubprocess*)proc, result,
                                                 &stdout_str, &stderr_str, &data->error);
  else
    (void) g_subprocess_communicate_finish ((GSubprocess*)proc, result,
                                            &stdout_bytes, &stderr_bytes, &data->error);
  if (data->error)
      return;

  if (data->flags & G_SUBPROCESS_FLAGS_STDOUT_PIPE)
    {
      if (data->is_utf8)
        {
          g_assert_nonnull (stdout_str);
          stdout_data = (guint8*)stdout_str;
          stdout_len = strlen (stdout_str);
        }
      else
        {
          g_assert_nonnull (stdout_bytes);
          stdout_data = g_bytes_get_data (stdout_bytes, &stdout_len);
        }

      g_assert_cmpmem (stdout_data, stdout_len, "# hello world" LINEEND, 13 + strlen (LINEEND));
    }
  else
    {
      g_assert_null (stdout_str);
      g_assert_null (stdout_bytes);
    }

  if (data->flags & G_SUBPROCESS_FLAGS_STDERR_PIPE)
    {
      if (data->is_utf8)
        g_assert_nonnull (stderr_str);
      else
        g_assert_nonnull (stderr_bytes);
    }
  else
    {
      g_assert_null (stderr_str);
      g_assert_null (stderr_bytes);
    }

  g_clear_pointer (&stdout_bytes, g_bytes_unref);
  g_clear_pointer (&stderr_bytes, g_bytes_unref);
  g_free (stdout_str);
  g_free (stderr_str);
}

/* Test g_subprocess_communicate_async() works correctly with a variety of flags,
 * as passed in via @test_data. */
static void
test_communicate_async (gconstpointer test_data)
{
  GSubprocessFlags flags = GPOINTER_TO_INT (test_data);
  GError *error = NULL;
  GPtrArray *args;
  TestAsyncCommunicateData data = { flags, 0, 0, NULL };
  GSubprocess *proc;
  GCancellable *cancellable = NULL;
  GBytes *input;
  const char *hellostring;

  args = get_test_subprocess_args ("cat", NULL);
  proc = g_subprocess_newv ((const gchar* const*)args->pdata,
                            G_SUBPROCESS_FLAGS_STDIN_PIPE | flags,
                            &error);
  g_assert_no_error (error);
  g_ptr_array_free (args, TRUE);

  /* Include a leading hash and trailing newline so that if this gets onto the
   * test’s stdout, it doesn’t mess up TAP output. */
  hellostring = "# hello world\n";
  input = g_bytes_new_static (hellostring, strlen (hellostring));

  g_subprocess_communicate_async (proc, input,
                                  cancellable,
                                  on_communicate_complete, 
                                  &data);
  
  data.running = TRUE;
  while (data.running)
    g_main_context_iteration (NULL, TRUE);

  g_assert_no_error (data.error);

  g_bytes_unref (input);
  g_object_unref (proc);
}

/* Test g_subprocess_communicate() works correctly with a variety of flags,
 * as passed in via @test_data. */
static void
test_communicate (gconstpointer test_data)
{
  GSubprocessFlags flags = GPOINTER_TO_INT (test_data);
  GError *error = NULL;
  GPtrArray *args;
  GSubprocess *proc;
  GCancellable *cancellable = NULL;
  GBytes *input;
  const gchar *hellostring;
  GBytes *stdout_bytes, *stderr_bytes;
  const gchar *stdout_data;
  gsize stdout_len;

  args = get_test_subprocess_args ("cat", NULL);
  proc = g_subprocess_newv ((const gchar* const*)args->pdata,
                            G_SUBPROCESS_FLAGS_STDIN_PIPE | flags,
                            &error);
  g_assert_no_error (error);
  g_ptr_array_free (args, TRUE);

  /* Include a leading hash and trailing newline so that if this gets onto the
   * test’s stdout, it doesn’t mess up TAP output. */
  hellostring = "# hello world\n";
  input = g_bytes_new_static (hellostring, strlen (hellostring));

  g_subprocess_communicate (proc, input, cancellable, &stdout_bytes, &stderr_bytes, &error);
  g_assert_no_error (error);

  if (flags & G_SUBPROCESS_FLAGS_STDOUT_PIPE)
    {
      stdout_data = g_bytes_get_data (stdout_bytes, &stdout_len);
      g_assert_cmpmem (stdout_data, stdout_len, "# hello world" LINEEND, 13 + strlen (LINEEND));
    }
  else
    g_assert_null (stdout_bytes);
  if (flags & G_SUBPROCESS_FLAGS_STDERR_PIPE)
    g_assert_nonnull (stderr_bytes);
  else
    g_assert_null (stderr_bytes);

  g_bytes_unref (input);
  g_clear_pointer (&stdout_bytes, g_bytes_unref);
  g_clear_pointer (&stderr_bytes, g_bytes_unref);
  g_object_unref (proc);
}

typedef struct {
  GSubprocess  *proc;
  GCancellable *cancellable;
  gboolean is_utf8;
  gboolean running;
  GError *error;
} TestCancelledCommunicateData;

static gboolean
on_test_communicate_cancelled_idle (gpointer user_data)
{
  TestCancelledCommunicateData *data = user_data;
  GBytes *input;
  const gchar *hellostring;
  GBytes *stdout_bytes = NULL, *stderr_bytes = NULL;
  gchar *stdout_buf = NULL, *stderr_buf = NULL;

  /* Include a leading hash and trailing newline so that if this gets onto the
   * test’s stdout, it doesn’t mess up TAP output. */
  hellostring = "# hello world\n";
  input = g_bytes_new_static (hellostring, strlen (hellostring));

  if (data->is_utf8)
    g_subprocess_communicate_utf8 (data->proc, hellostring, data->cancellable,
                                   &stdout_buf, &stderr_buf, &data->error);
  else
    g_subprocess_communicate (data->proc, input, data->cancellable, &stdout_bytes,
                              &stderr_bytes, &data->error);

  data->running = FALSE;

  if (data->is_utf8)
    {
      g_assert_null (stdout_buf);
      g_assert_null (stderr_buf);
    }
  else
    {
      g_assert_null (stdout_bytes);
      g_assert_null (stderr_bytes);
    }

  g_bytes_unref (input);

  return G_SOURCE_REMOVE;
}

/* Test g_subprocess_communicate() can be cancelled correctly */
static void
test_communicate_cancelled (gconstpointer test_data)
{
  GSubprocessFlags flags = GPOINTER_TO_INT (test_data);
  GPtrArray *args;
  GSubprocess *proc;
  GCancellable *cancellable = NULL;
  GError *error = NULL;
  TestCancelledCommunicateData data = { 0 };

  args = get_test_subprocess_args ("cat", NULL);
  proc = g_subprocess_newv ((const gchar* const*)args->pdata,
                            G_SUBPROCESS_FLAGS_STDIN_PIPE | flags,
                            &error);
  g_assert_no_error (error);
  g_ptr_array_free (args, TRUE);

  cancellable = g_cancellable_new ();

  data.proc = proc;
  data.cancellable = cancellable;
  data.error = error;

  g_cancellable_cancel (cancellable);
  g_idle_add (on_test_communicate_cancelled_idle, &data);

  data.running = TRUE;
  while (data.running)
    g_main_context_iteration (NULL, TRUE);

  g_assert_error (data.error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
  g_clear_error (&data.error);

  g_object_unref (cancellable);
  g_object_unref (proc);
}

static void
on_communicate_cancelled_complete (GObject               *proc,
                                   GAsyncResult          *result,
                                   gpointer               user_data)
{
  TestAsyncCommunicateData *data = user_data;
  GBytes *stdout_bytes = NULL, *stderr_bytes = NULL;
  char *stdout_str = NULL, *stderr_str = NULL;

  data->running = FALSE;
  if (data->is_utf8)
    (void) g_subprocess_communicate_utf8_finish ((GSubprocess*)proc, result,
                                                 &stdout_str, &stderr_str, &data->error);
  else
    (void) g_subprocess_communicate_finish ((GSubprocess*)proc, result,
                                            &stdout_bytes, &stderr_bytes, &data->error);

  if (data->is_utf8)
    {
      g_assert_null (stdout_str);
      g_assert_null (stderr_str);
    }
  else
    {
      g_assert_null (stdout_bytes);
      g_assert_null (stderr_bytes);
    }
}

/* Test g_subprocess_communicate_async() can be cancelled correctly,
 * as passed in via @test_data. */
static void
test_communicate_cancelled_async (gconstpointer test_data)
{
  GSubprocessFlags flags = GPOINTER_TO_INT (test_data);
  GError *error = NULL;
  GPtrArray *args;
  TestAsyncCommunicateData data = { 0 };
  GSubprocess *proc;
  GCancellable *cancellable = NULL;
  GBytes *input;
  const char *hellostring;

  args = get_test_subprocess_args ("cat", NULL);
  proc = g_subprocess_newv ((const gchar* const*)args->pdata,
                            G_SUBPROCESS_FLAGS_STDIN_PIPE | flags,
                            &error);
  g_assert_no_error (error);
  g_ptr_array_free (args, TRUE);

  /* Include a leading hash and trailing newline so that if this gets onto the
   * test’s stdout, it doesn’t mess up TAP output. */
  hellostring = "# hello world\n";
  input = g_bytes_new_static (hellostring, strlen (hellostring));

  cancellable = g_cancellable_new ();

  g_subprocess_communicate_async (proc, input,
                                  cancellable,
                                  on_communicate_cancelled_complete,
                                  &data);

  g_cancellable_cancel (cancellable);

  data.running = TRUE;
  while (data.running)
    g_main_context_iteration (NULL, TRUE);

  g_assert_error (data.error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
  g_clear_error (&data.error);

  g_bytes_unref (input);
  g_object_unref (cancellable);
  g_object_unref (proc);
}

/* Test g_subprocess_communicate_utf8_async() works correctly with a variety of
 * flags, as passed in via @test_data. */
static void
test_communicate_utf8_async (gconstpointer test_data)
{
  GSubprocessFlags flags = GPOINTER_TO_INT (test_data);
  GError *error = NULL;
  GPtrArray *args;
  TestAsyncCommunicateData data = { flags, 0, 0, NULL };
  GSubprocess *proc;
  GCancellable *cancellable = NULL;

  args = get_test_subprocess_args ("cat", NULL);
  proc = g_subprocess_newv ((const gchar* const*)args->pdata,
                            G_SUBPROCESS_FLAGS_STDIN_PIPE | flags,
                            &error);
  g_assert_no_error (error);
  g_ptr_array_free (args, TRUE);

  data.is_utf8 = TRUE;
  g_subprocess_communicate_utf8_async (proc, "# hello world\n",
                                       cancellable,
                                       on_communicate_complete, 
                                       &data);
  
  data.running = TRUE;
  while (data.running)
    g_main_context_iteration (NULL, TRUE);

  g_assert_no_error (data.error);

  g_object_unref (proc);
}

/* Test g_subprocess_communicate_utf8_async() can be cancelled correctly. */
static void
test_communicate_utf8_cancelled_async (gconstpointer test_data)
{
  GSubprocessFlags flags = GPOINTER_TO_INT (test_data);
  GError *error = NULL;
  GPtrArray *args;
  TestAsyncCommunicateData data = { flags, 0, 0, NULL };
  GSubprocess *proc;
  GCancellable *cancellable = NULL;

  args = get_test_subprocess_args ("cat", NULL);
  proc = g_subprocess_newv ((const gchar* const*)args->pdata,
                            G_SUBPROCESS_FLAGS_STDIN_PIPE | flags,
                            &error);
  g_assert_no_error (error);
  g_ptr_array_free (args, TRUE);

  cancellable = g_cancellable_new ();
  data.is_utf8 = TRUE;
  g_subprocess_communicate_utf8_async (proc, "# hello world\n",
                                       cancellable,
                                       on_communicate_cancelled_complete,
                                       &data);

  g_cancellable_cancel (cancellable);

  data.running = TRUE;
  while (data.running)
    g_main_context_iteration (NULL, TRUE);

  g_assert_error (data.error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
  g_clear_error (&data.error);

  g_object_unref (cancellable);
  g_object_unref (proc);
}

/* Test g_subprocess_communicate_utf8() works correctly with a variety of flags,
 * as passed in via @test_data. */
static void
test_communicate_utf8 (gconstpointer test_data)
{
  GSubprocessFlags flags = GPOINTER_TO_INT (test_data);
  GError *error = NULL;
  GPtrArray *args;
  GSubprocess *proc;
  GCancellable *cancellable = NULL;
  const gchar *stdin_buf;
  gchar *stdout_buf, *stderr_buf;

  args = get_test_subprocess_args ("cat", NULL);
  proc = g_subprocess_newv ((const gchar* const*)args->pdata,
                            G_SUBPROCESS_FLAGS_STDIN_PIPE | flags,
                            &error);
  g_assert_no_error (error);
  g_ptr_array_free (args, TRUE);

  /* Include a leading hash and trailing newline so that if this gets onto the
   * test’s stdout, it doesn’t mess up TAP output. */
  stdin_buf = "# hello world\n";

  g_subprocess_communicate_utf8 (proc, stdin_buf, cancellable, &stdout_buf, &stderr_buf, &error);
  g_assert_no_error (error);

  if (flags & G_SUBPROCESS_FLAGS_STDOUT_PIPE)
    g_assert_cmpstr (stdout_buf, ==, "# hello world" LINEEND);
  else
    g_assert_null (stdout_buf);
  if (flags & G_SUBPROCESS_FLAGS_STDERR_PIPE)
    g_assert_nonnull (stderr_buf);
  else     g_assert_null (stderr_buf);

  g_free (stdout_buf);
  g_free (stderr_buf);
  g_object_unref (proc);
}

/* Test g_subprocess_communicate_utf8() can be cancelled correctly */
static void
test_communicate_utf8_cancelled (gconstpointer test_data)
{
  GSubprocessFlags flags = GPOINTER_TO_INT (test_data);
  GPtrArray *args;
  GSubprocess *proc;
  GCancellable *cancellable = NULL;
  GError *error = NULL;
  TestCancelledCommunicateData data = { 0 };

  args = get_test_subprocess_args ("cat", NULL);
  proc = g_subprocess_newv ((const gchar* const*)args->pdata,
                            G_SUBPROCESS_FLAGS_STDIN_PIPE | flags,
                            &error);
  g_assert_no_error (error);
  g_ptr_array_free (args, TRUE);

  cancellable = g_cancellable_new ();

  data.proc = proc;
  data.cancellable = cancellable;
  data.error = error;

  g_cancellable_cancel (cancellable);
  g_idle_add (on_test_communicate_cancelled_idle, &data);

  data.is_utf8 = TRUE;
  data.running = TRUE;
  while (data.running)
    g_main_context_iteration (NULL, TRUE);

  g_assert_error (data.error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
  g_clear_error (&data.error);

  g_object_unref (cancellable);
  g_object_unref (proc);
}

static void
test_communicate_nothing (void)
{
  GError *error = NULL;
  GPtrArray *args;
  GSubprocess *proc;
  GCancellable *cancellable = NULL;
  gchar *stdout_buf;

  args = get_test_subprocess_args ("cat", NULL);
  proc = g_subprocess_newv ((const gchar* const*)args->pdata,
                            G_SUBPROCESS_FLAGS_STDIN_PIPE
                            | G_SUBPROCESS_FLAGS_STDOUT_PIPE
                            | G_SUBPROCESS_FLAGS_STDERR_MERGE,
                            &error);
  g_assert_no_error (error);
  g_ptr_array_free (args, TRUE);

  g_subprocess_communicate_utf8 (proc, "", cancellable, &stdout_buf, NULL, &error);
  g_assert_no_error (error);

  g_assert_cmpstr (stdout_buf, ==, "");

  g_free (stdout_buf);

  g_object_unref (proc);
}

static void
test_communicate_utf8_async_invalid (void)
{
  GSubprocessFlags flags = G_SUBPROCESS_FLAGS_STDOUT_PIPE;
  GError *error = NULL;
  GPtrArray *args;
  TestAsyncCommunicateData data = { flags, 0, 0, NULL };
  GSubprocess *proc;
  GCancellable *cancellable = NULL;

  args = get_test_subprocess_args ("cat", NULL);
  proc = g_subprocess_newv ((const gchar* const*)args->pdata,
                            G_SUBPROCESS_FLAGS_STDIN_PIPE | flags,
                            &error);
  g_assert_no_error (error);
  g_ptr_array_free (args, TRUE);

  data.is_utf8 = TRUE;
  g_subprocess_communicate_utf8_async (proc, "\xFF\xFF",
                                       cancellable,
                                       on_communicate_complete, 
                                       &data);
  
  data.running = TRUE;
  while (data.running)
    g_main_context_iteration (NULL, TRUE);

  g_assert_error (data.error, G_IO_ERROR, G_IO_ERROR_FAILED);
  g_error_free (data.error);

  g_object_unref (proc);
}

/* Test that invalid UTF-8 received using g_subprocess_communicate_utf8()
 * results in an error. */
static void
test_communicate_utf8_invalid (void)
{
  GSubprocessFlags flags = G_SUBPROCESS_FLAGS_STDOUT_PIPE;
  GError *local_error = NULL;
  gboolean ret;
  GPtrArray *args;
  gchar *stdout_str = NULL, *stderr_str = NULL;
  GSubprocess *proc;

  args = get_test_subprocess_args ("cat", NULL);
  proc = g_subprocess_newv ((const gchar* const*)args->pdata,
                            G_SUBPROCESS_FLAGS_STDIN_PIPE | flags,
                            &local_error);
  g_assert_no_error (local_error);
  g_ptr_array_free (args, TRUE);

  ret = g_subprocess_communicate_utf8 (proc, "\xFF\xFF", NULL,
                                       &stdout_str, &stderr_str, &local_error);
  g_assert_error (local_error, G_IO_ERROR, G_IO_ERROR_FAILED);
  g_error_free (local_error);
  g_assert_false (ret);

  g_assert_null (stdout_str);
  g_assert_null (stderr_str);

  g_object_unref (proc);
}

static void
send_terminate (gpointer   user_data)
{
  GSubprocess *proc = user_data;

  g_subprocess_force_exit (proc);
}

static void
on_request_quit_exited (GObject        *object,
                        GAsyncResult   *result,
                        gpointer        user_data)
{
  GSubprocess *subprocess = G_SUBPROCESS (object);
  GError *error = NULL;

  g_subprocess_wait_finish (subprocess, result, &error);
  g_assert_no_error (error);
#ifdef G_OS_UNIX
  g_assert_true (g_subprocess_get_if_signaled (subprocess));
  g_assert_cmpint (g_subprocess_get_term_sig (subprocess), ==, 9);
#endif
  g_spawn_check_wait_status (g_subprocess_get_status (subprocess), &error);
  g_assert_nonnull (error);
  g_clear_error (&error);

  g_main_loop_quit ((GMainLoop*)user_data);
}

static void
test_terminate (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GSubprocess *proc;
  GPtrArray *args;
  GMainLoop *loop;
  const gchar *id;

  args = get_test_subprocess_args ("sleep-forever", NULL);
  proc = g_subprocess_newv ((const gchar * const *) args->pdata, G_SUBPROCESS_FLAGS_NONE, error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  id = g_subprocess_get_identifier (proc);
  g_assert_nonnull (id);

  loop = g_main_loop_new (NULL, TRUE);

  g_subprocess_wait_async (proc, NULL, on_request_quit_exited, loop);

  g_timeout_add_seconds_once (3, send_terminate, proc);

  g_main_loop_run (loop);

  g_main_loop_unref (loop);
  g_object_unref (proc);
}

static void
test_fail_initialization (void)
{
  GError *local_error = NULL;
  GSubprocess *proc;

  proc = g_subprocess_new (G_SUBPROCESS_FLAGS_NONE,
                           &local_error,
                           "thisprogramshouldnotexistprettyplease",
                           NULL);

  g_assert_error (local_error, G_SPAWN_ERROR, G_SPAWN_ERROR_NOENT);
  g_assert_null (proc);
  g_clear_error (&local_error);
}

#ifdef G_OS_UNIX
static void
send_signal (gpointer user_data)
{
  GSubprocess *proc = user_data;

  g_subprocess_send_signal (proc, SIGKILL);
}

static void
test_signal (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GSubprocess *proc;
  GPtrArray *args;
  GMainLoop *loop;

  args = get_test_subprocess_args ("sleep-forever", NULL);
  proc = g_subprocess_newv ((const gchar * const *) args->pdata, G_SUBPROCESS_FLAGS_NONE, error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  loop = g_main_loop_new (NULL, TRUE);

  g_subprocess_wait_async (proc, NULL, on_request_quit_exited, loop);

  g_timeout_add_seconds_once (3, send_signal, proc);

  g_main_loop_run (loop);

  g_main_loop_unref (loop);
  g_object_unref (proc);
}
#endif

static void
test_env (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GSubprocessLauncher *launcher;
  GSubprocess *proc;
  GPtrArray *args;
  GInputStream *stdout_stream;
  gchar *result;
  gchar *envp[] = { NULL, "ONE=1", "TWO=1", "THREE=3", "FOUR=1", NULL };
  gchar **split;

  envp[0] = g_strdup_printf ("PATH=%s", g_getenv ("PATH"));
  args = get_test_subprocess_args ("env", NULL);
  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_NONE);
  g_subprocess_launcher_set_flags (launcher, G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  g_subprocess_launcher_set_environ (launcher, envp);
  g_subprocess_launcher_setenv (launcher, "TWO", "2", TRUE);
  g_subprocess_launcher_setenv (launcher, "THREE", "1", FALSE);
  g_subprocess_launcher_unsetenv (launcher, "FOUR");

  g_assert_null (g_subprocess_launcher_getenv (launcher, "FOUR"));
   
  proc = g_subprocess_launcher_spawn (launcher, error, args->pdata[0], "env", NULL);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);
  g_free (envp[0]);

  stdout_stream = g_subprocess_get_stdout_pipe (proc);

  result = splice_to_string (stdout_stream, error);
  split = g_strsplit (result, LINEEND, -1);
  g_assert_cmpstr (g_environ_getenv (split, "ONE"), ==, "1");
  g_assert_cmpstr (g_environ_getenv (split, "TWO"), ==, "2");
  g_assert_cmpstr (g_environ_getenv (split, "THREE"), ==, "3");
  g_assert_null (g_environ_getenv (split, "FOUR"));

  g_strfreev (split);
  g_free (result);
  g_object_unref (proc);
  g_object_unref (launcher);
}

/* Test that explicitly inheriting and modifying the parent process’
 * environment works. */
static void
test_env_inherit (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GSubprocessLauncher *launcher;
  GSubprocess *proc;
  GPtrArray *args;
  GInputStream *stdout_stream;
  gchar *result;
  gchar **split;

  g_setenv ("TEST_ENV_INHERIT1", "1", TRUE);
  g_setenv ("TEST_ENV_INHERIT2", "2", TRUE);

  args = get_test_subprocess_args ("env", NULL);
  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_NONE);
  g_subprocess_launcher_set_flags (launcher, G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  g_subprocess_launcher_set_environ (launcher, NULL);
  g_subprocess_launcher_setenv (launcher, "TWO", "2", TRUE);
  g_subprocess_launcher_unsetenv (launcher, "TEST_ENV_INHERIT1");

  g_assert_null (g_subprocess_launcher_getenv (launcher, "TEST_ENV_INHERIT1"));
  g_assert_cmpstr (g_subprocess_launcher_getenv (launcher, "TEST_ENV_INHERIT2"), ==, "2");
  g_assert_cmpstr (g_subprocess_launcher_getenv (launcher, "TWO"), ==, "2");

  proc = g_subprocess_launcher_spawn (launcher, error, args->pdata[0], "env", NULL);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  stdout_stream = g_subprocess_get_stdout_pipe (proc);

  result = splice_to_string (stdout_stream, error);
  split = g_strsplit (result, LINEEND, -1);
  g_assert_null (g_environ_getenv (split, "TEST_ENV_INHERIT1"));
  g_assert_cmpstr (g_environ_getenv (split, "TEST_ENV_INHERIT2"), ==, "2");
  g_assert_cmpstr (g_environ_getenv (split, "TWO"), ==, "2");

  g_strfreev (split);
  g_free (result);
  g_object_unref (proc);
  g_object_unref (launcher);
}

static void
test_cwd (void)
{
  GError *local_error = NULL;
  GSubprocessLauncher *launcher;
  GSubprocess *proc;
  GPtrArray *args;
  GInputStream *stdout_stream;
  gchar *result;
  gsize result_len;
  const gchar *tmpdir = g_get_tmp_dir ();
  gchar *tmpdir_basename = NULL, *result_basename = NULL;

  args = get_test_subprocess_args ("cwd", NULL);
  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  g_subprocess_launcher_set_flags (launcher, G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  g_subprocess_launcher_set_cwd (launcher, tmpdir);

  proc = g_subprocess_launcher_spawnv (launcher, (const char * const *)args->pdata, &local_error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  stdout_stream = g_subprocess_get_stdout_pipe (proc);

  result = splice_to_string (stdout_stream, &local_error);
  g_assert_no_error (local_error);
  result_len = strlen (result);

  /* The result should end with a line ending */
  g_assert_cmpstr (result + result_len - strlen (LINEEND), ==, LINEEND);

  /* Not sure if the testprog guarantees to return an absolute path for the cwd,
   * so only compare the basenames. */
  tmpdir_basename = g_path_get_basename (tmpdir);
  result_basename = g_path_get_basename (g_strstrip (result));
  g_assert_cmpstr (tmpdir_basename, ==, result_basename);
  g_free (tmpdir_basename);
  g_free (result_basename);

  g_free (result);
  g_object_unref (proc);
  g_object_unref (launcher);
}
#ifdef G_OS_UNIX

static void
test_subprocess_launcher_close (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GSubprocessLauncher *launcher;
  GSubprocess *proc;
  GPtrArray *args;
  int fd, fd2;
  gboolean is_open;

  /* Open two arbitrary FDs. One of them, @fd, will be transferred to the
   * launcher, and the other’s FD integer will be used as its target FD, giving
   * the mapping `fd → fd2` if a child process were to be spawned.
   *
   * The launcher will then be closed, which should close @fd but *not* @fd2,
   * as the value of @fd2 is only valid as an FD in a child process. (A child
   * process is not actually spawned in this test.)
   */
  fd = dup (0);
  fd2 = dup (0);
  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_NONE);
  g_subprocess_launcher_take_fd (launcher, fd, fd2);

  is_open = fcntl (fd, F_GETFD) != -1;
  g_assert_true (is_open);
  is_open = fcntl (fd2, F_GETFD) != -1;
  g_assert_true (is_open);

  g_subprocess_launcher_close (launcher);

  is_open = fcntl (fd, F_GETFD) != -1;
  g_assert_false (is_open);
  is_open = fcntl (fd2, F_GETFD) != -1;
  g_assert_true (is_open);

  /* Now test that actually trying to spawn the child gives %G_IO_ERROR_CLOSED,
   * as g_subprocess_launcher_close() has been called. */
  args = get_test_subprocess_args ("cat", NULL);
  proc = g_subprocess_launcher_spawnv (launcher, (const gchar * const *) args->pdata, error);
  g_ptr_array_free (args, TRUE);
  g_assert_null (proc);
  g_assert_error (local_error, G_IO_ERROR, G_IO_ERROR_CLOSED);
  g_clear_error (error);

  close (fd2);
  g_object_unref (launcher);
}

static void
test_stdout_file (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GSubprocessLauncher *launcher;
  GSubprocess *proc;
  GPtrArray *args;
  GFile *tmpfile;
  GFileIOStream *iostream;
  GOutputStream *stdin_stream;
  const char *test_data = "this is some test data\n";
  char *tmp_contents;
  char *tmp_file_path;

  tmpfile = g_file_new_tmp ("gsubprocessXXXXXX", &iostream, error);
  g_assert_no_error (local_error);
  g_clear_object (&iostream);

  tmp_file_path = g_file_get_path (tmpfile);

  args = get_test_subprocess_args ("cat", NULL);
  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDIN_PIPE);
  g_subprocess_launcher_set_stdout_file_path (launcher, tmp_file_path);
  proc = g_subprocess_launcher_spawnv (launcher, (const gchar * const *) args->pdata, error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  stdin_stream = g_subprocess_get_stdin_pipe (proc);

  g_output_stream_write_all (stdin_stream, test_data, strlen (test_data), NULL, NULL, error);
  g_assert_no_error (local_error);

  g_output_stream_close (stdin_stream, NULL, error);
  g_assert_no_error (local_error);

  g_subprocess_wait_check (proc, NULL, error);

  g_object_unref (launcher);
  g_object_unref (proc);

  g_file_load_contents (tmpfile, NULL, &tmp_contents, NULL, NULL, error);
  g_assert_no_error (local_error);

  g_assert_cmpstr (test_data, ==, tmp_contents);
  g_free (tmp_contents);

  (void) g_file_delete (tmpfile, NULL, NULL);
  g_object_unref (tmpfile);
  g_free (tmp_file_path);
}

static void
test_stdout_fd (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GSubprocessLauncher *launcher;
  GSubprocess *proc;
  GPtrArray *args;
  GFile *tmpfile;
  GFileIOStream *iostream;
  GFileDescriptorBased *descriptor_stream;
  GOutputStream *stdin_stream;
  const char *test_data = "this is some test data\n";
  char *tmp_contents;

  tmpfile = g_file_new_tmp ("gsubprocessXXXXXX", &iostream, error);
  g_assert_no_error (local_error);

  args = get_test_subprocess_args ("cat", NULL);
  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDIN_PIPE);
  descriptor_stream = G_FILE_DESCRIPTOR_BASED (g_io_stream_get_output_stream (G_IO_STREAM (iostream)));
  g_subprocess_launcher_take_stdout_fd (launcher, dup (g_file_descriptor_based_get_fd (descriptor_stream)));
  proc = g_subprocess_launcher_spawnv (launcher, (const gchar * const *) args->pdata, error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  g_clear_object (&iostream);

  stdin_stream = g_subprocess_get_stdin_pipe (proc);

  g_output_stream_write_all (stdin_stream, test_data, strlen (test_data), NULL, NULL, error);
  g_assert_no_error (local_error);

  g_output_stream_close (stdin_stream, NULL, error);
  g_assert_no_error (local_error);

  g_subprocess_wait_check (proc, NULL, error);

  g_object_unref (launcher);
  g_object_unref (proc);

  g_file_load_contents (tmpfile, NULL, &tmp_contents, NULL, NULL, error);
  g_assert_no_error (local_error);

  g_assert_cmpstr (test_data, ==, tmp_contents);
  g_free (tmp_contents);

  (void) g_file_delete (tmpfile, NULL, NULL);
  g_object_unref (tmpfile);
}

static void
child_setup (gpointer user_data)
{
  dup2 (GPOINTER_TO_INT (user_data), 1);
}

static void
test_child_setup (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GSubprocessLauncher *launcher;
  GSubprocess *proc;
  GPtrArray *args;
  GFile *tmpfile;
  GFileIOStream *iostream;
  GOutputStream *stdin_stream;
  const char *test_data = "this is some test data\n";
  char *tmp_contents;
  int fd;

  tmpfile = g_file_new_tmp ("gsubprocessXXXXXX", &iostream, error);
  g_assert_no_error (local_error);

  fd = g_file_descriptor_based_get_fd (G_FILE_DESCRIPTOR_BASED (g_io_stream_get_output_stream (G_IO_STREAM (iostream))));

  args = get_test_subprocess_args ("cat", NULL);
  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDIN_PIPE);
  g_subprocess_launcher_set_child_setup (launcher, child_setup, GINT_TO_POINTER (fd), NULL);
  proc = g_subprocess_launcher_spawnv (launcher, (const gchar * const *) args->pdata, error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  g_clear_object (&iostream);

  stdin_stream = g_subprocess_get_stdin_pipe (proc);

  g_output_stream_write_all (stdin_stream, test_data, strlen (test_data), NULL, NULL, error);
  g_assert_no_error (local_error);

  g_output_stream_close (stdin_stream, NULL, error);
  g_assert_no_error (local_error);

  g_subprocess_wait_check (proc, NULL, error);

  g_object_unref (launcher);
  g_object_unref (proc);

  g_file_load_contents (tmpfile, NULL, &tmp_contents, NULL, NULL, error);
  g_assert_no_error (local_error);

  g_assert_cmpstr (test_data, ==, tmp_contents);
  g_free (tmp_contents);

  (void) g_file_delete (tmpfile, NULL, NULL);
  g_object_unref (tmpfile);
}

static void
do_test_pass_fd (GSubprocessFlags     flags,
                 GSpawnChildSetupFunc child_setup)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GInputStream *child_input;
  GDataInputStream *child_datainput;
  GSubprocessLauncher *launcher;
  GSubprocess *proc;
  GPtrArray *args;
  int basic_pipefds[2];
  int needdup_pipefds[2];
  char *buf;
  gsize len;
  char *basic_fd_str;
  char *needdup_fd_str;

  g_unix_open_pipe (basic_pipefds, O_CLOEXEC, error);
  g_assert_no_error (local_error);
  g_unix_open_pipe (needdup_pipefds, O_CLOEXEC, error);
  g_assert_no_error (local_error);

  basic_fd_str = g_strdup_printf ("%d", basic_pipefds[1]);
  needdup_fd_str = g_strdup_printf ("%d", needdup_pipefds[1] + 1);

  args = get_test_subprocess_args ("write-to-fds", basic_fd_str, needdup_fd_str, NULL);
  launcher = g_subprocess_launcher_new (flags);
  g_subprocess_launcher_take_fd (launcher, basic_pipefds[1], basic_pipefds[1]);
  g_subprocess_launcher_take_fd (launcher, needdup_pipefds[1], needdup_pipefds[1] + 1);
  if (child_setup != NULL)
    g_subprocess_launcher_set_child_setup (launcher, child_setup, NULL, NULL);
  proc = g_subprocess_launcher_spawnv (launcher, (const gchar * const *) args->pdata, error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  g_free (basic_fd_str);
  g_free (needdup_fd_str);

  child_input = g_unix_input_stream_new (basic_pipefds[0], TRUE);
  child_datainput = g_data_input_stream_new (child_input);
  buf = g_data_input_stream_read_line_utf8 (child_datainput, &len, NULL, error);
  g_assert_no_error (local_error);
  g_assert_cmpstr (buf, ==, "hello world");
  g_object_unref (child_datainput);
  g_object_unref (child_input);
  g_free (buf);

  child_input = g_unix_input_stream_new (needdup_pipefds[0], TRUE);
  child_datainput = g_data_input_stream_new (child_input);
  buf = g_data_input_stream_read_line_utf8 (child_datainput, &len, NULL, error);
  g_assert_no_error (local_error);
  g_assert_cmpstr (buf, ==, "hello world");
  g_free (buf);
  g_object_unref (child_datainput);
  g_object_unref (child_input);

  g_object_unref (launcher);
  g_object_unref (proc);
}

static void
test_pass_fd (void)
{
  do_test_pass_fd (G_SUBPROCESS_FLAGS_NONE, NULL);
}

static void
empty_child_setup (gpointer user_data)
{
}

static void
test_pass_fd_empty_child_setup (void)
{
  /* Using a child setup function forces gspawn to use fork/exec
   * rather than posix_spawn.
   */
  do_test_pass_fd (G_SUBPROCESS_FLAGS_NONE, empty_child_setup);
}

static void
test_pass_fd_inherit_fds (void)
{
  /* Try to test the optimized posix_spawn codepath instead of
   * fork/exec. Currently this requires using INHERIT_FDS since gspawn's
   * posix_spawn codepath does not currently handle closing
   * non-inherited fds. Note that using INHERIT_FDS means our testing of
   * g_subprocess_launcher_take_fd() is less-comprehensive than when
   * using G_SUBPROCESS_FLAGS_NONE.
   */
  do_test_pass_fd (G_SUBPROCESS_FLAGS_INHERIT_FDS, NULL);
}

static void
do_test_fd_conflation (GSubprocessFlags     flags,
                       GSpawnChildSetupFunc child_setup,
                       gboolean             test_child_err_report_fd)
{
  char success_message[] = "Yay success!";
  GError *error = NULL;
  GOutputStream *output_stream;
  GSubprocessLauncher *launcher;
  GSubprocess *proc;
  GPtrArray *args;
  int unused_pipefds[2];
  int pipefds[2];
  int fd_to_pass_to_child;
  gsize bytes_written;
  gboolean success;
  char *fd_str;

  /* This test must run in a new process because it is extremely sensitive to
   * order of opened fds.
   */
  if (!g_test_subprocess ())
    {
      g_test_trap_subprocess (NULL, 0, G_TEST_SUBPROCESS_INHERIT_STDOUT | G_TEST_SUBPROCESS_INHERIT_STDERR);
      g_test_trap_assert_passed ();
      return;
    }

  g_unix_open_pipe (unused_pipefds, O_CLOEXEC, &error);
  g_assert_no_error (error);

  g_unix_open_pipe (pipefds, O_CLOEXEC, &error);
  g_assert_no_error (error);

  /* The fds should be sequential since we are in a new process. */
  g_assert_cmpint (unused_pipefds[0] /* 3 */, ==, unused_pipefds[1] - 1);
  g_assert_cmpint (unused_pipefds[1] /* 4 */, ==, pipefds[0] - 1);
  g_assert_cmpint (pipefds[0] /* 5 */, ==, pipefds[1] /* 6 */ - 1);

  /* Because GSubprocess allows arbitrary remapping of fds, it has to be careful
   * to avoid fd conflation issues, e.g. it should properly handle 5 -> 4 and
   * 4 -> 5 at the same time. GIO previously attempted to handle this by naively
   * dup'ing the source fds, but this was not good enough because it was
   * possible that the dup'ed result could still conflict with one of the target
   * fds. For example:
   *
   * source_fd 5 -> target_fd 9, source_fd 3 -> target_fd 7
   *
   * dup(5) -> dup returns 8
   * dup(3) -> dup returns 9
   *
   * After dup'ing, we wind up with: 8 -> 9, 9 -> 7. That means that after we
   * dup2(8, 9), we have clobbered fd 9 before we dup2(9, 7). The end result is
   * we have remapped 5 -> 9 as expected, but then remapped 5 -> 7 instead of
   * 3 -> 7 as the application intended.
   *
   * This issue has been fixed in the simplest way possible, by passing a
   * minimum fd value when using F_DUPFD_CLOEXEC that is higher than any of the
   * target fds, to guarantee all source fds are different than all target fds,
   * eliminating any possibility of conflation.
   *
   * Anyway, that is why we have the unused_pipefds here. We need to open fds in
   * a certain order in order to trick older GSubprocess into conflating the
   * fds. The primary goal of this test is to ensure this particular conflation
   * issue is not reintroduced. See glib#2503.
   *
   * This test also has an alternate mode of operation where it instead tests
   * for conflation with gspawn's child_err_report_fd, glib#2506.
   *
   * Be aware this test is necessarily extremely fragile. To reproduce these
   * bugs, it relies on internals of gspawn and gmain that will likely change
   * in the future, eventually causing this test to no longer test the bugs
   * it was originally designed to test. That is OK! If the test fails, at
   * least you know *something* is wrong.
   */
  if (test_child_err_report_fd)
    fd_to_pass_to_child = pipefds[1] + 2 /* 8 */;
  else
    fd_to_pass_to_child = pipefds[1] + 3 /* 9 */;

  launcher = g_subprocess_launcher_new (flags);
  g_subprocess_launcher_take_fd (launcher, pipefds[0] /* 5 */, fd_to_pass_to_child);
  g_subprocess_launcher_take_fd (launcher, unused_pipefds[0] /* 3 */, pipefds[1] + 1 /* 7 */);
  if (child_setup != NULL)
    g_subprocess_launcher_set_child_setup (launcher, child_setup, NULL, NULL);
  fd_str = g_strdup_printf ("%d", fd_to_pass_to_child);
  args = get_test_subprocess_args ("read-from-fd", fd_str, NULL);
  proc = g_subprocess_launcher_spawnv (launcher, (const gchar * const *) args->pdata, &error);
  g_assert_no_error (error);
  g_assert_nonnull (proc);
  g_ptr_array_free (args, TRUE);
  g_object_unref (launcher);
  g_free (fd_str);

  /* Close the read ends of the pipes. */
  close (unused_pipefds[0]);
  close (pipefds[0]);

  /* Also close the write end of the unused pipe. */
  close (unused_pipefds[1]);

  /* If doing our normal test:
   *
   * So now pipefds[0] should be inherited into the subprocess as
   * pipefds[1] + 2, and unused_pipefds[0] should be inherited as
   * pipefds[1] + 1. We will write to pipefds[1] and the subprocess will verify
   * that it reads the expected data. But older broken GIO will accidentally
   * clobber pipefds[1] + 2 with pipefds[1] + 1! This will cause the subprocess
   * to hang trying to read from the wrong pipe.
   *
   * If testing conflation with child_err_report_fd:
   *
   * We are actually already done. The real test succeeded if we made it this
   * far without hanging while spawning the child. But let's continue with our
   * write and read anyway, to ensure things are good.
   */
  output_stream = g_unix_output_stream_new (pipefds[1], TRUE);
  success = g_output_stream_write_all (output_stream,
                                       success_message, sizeof (success_message),
                                       &bytes_written,
                                       NULL,
                                       &error);
  g_assert_no_error (error);
  g_assert_cmpint (bytes_written, ==, sizeof (success_message));
  g_assert_true (success);
  g_object_unref (output_stream);

  success = g_subprocess_wait_check (proc, NULL, &error);
  g_assert_no_error (error);
  g_object_unref (proc);
}

static void
test_fd_conflation (void)
{
  do_test_fd_conflation (G_SUBPROCESS_FLAGS_NONE, NULL, FALSE);
}

static void
test_fd_conflation_empty_child_setup (void)
{
  /* Using a child setup function forces gspawn to use fork/exec
   * rather than posix_spawn.
   */
  do_test_fd_conflation (G_SUBPROCESS_FLAGS_NONE, empty_child_setup, FALSE);
}

static void
test_fd_conflation_inherit_fds (void)
{
  /* Try to test the optimized posix_spawn codepath instead of
   * fork/exec. Currently this requires using INHERIT_FDS since gspawn's
   * posix_spawn codepath does not currently handle closing
   * non-inherited fds.
   */
  do_test_fd_conflation (G_SUBPROCESS_FLAGS_INHERIT_FDS, NULL, FALSE);
}

static void
test_fd_conflation_child_err_report_fd (void)
{
  /* Using a child setup function forces gspawn to use fork/exec
   * rather than posix_spawn.
   */
  do_test_fd_conflation (G_SUBPROCESS_FLAGS_NONE, empty_child_setup, TRUE);
}

#ifdef __linux__

/* Handle ptrace events on @main_child, and assert that when it exits, it does
 * so with status %EXIT_SUCCESS, rather than signalling. Other than that, this
 * just calls %PTRACE_CONT for all trace events. */
static void
trace_children (pid_t main_child)
{
  int wstatus;

  g_assert_no_errno (waitpid (main_child, &wstatus, 0));
  g_assert_no_errno (ptrace (PTRACE_SETOPTIONS, main_child, NULL,
                             (PTRACE_O_TRACEFORK |
#ifdef HAVE_PTRACE_O_EXITKILL
                              PTRACE_O_EXITKILL |
#endif
                              PTRACE_O_TRACEVFORK |
                              PTRACE_O_TRACECLONE |
                              PTRACE_O_TRACEEXEC)));
  g_assert_no_errno (ptrace (PTRACE_CONT, main_child, NULL, 0));

  while (TRUE)
    {
      pid_t pid;
      int wstatus;
      int stop_signum;
      int ptrace_event;

      pid = waitpid (-1, &wstatus, 0);
      if (pid == -1 && errno == ECHILD)
        break;

      g_assert_cmpint (errno, ==, 0);
      g_assert_cmpint (pid, >=, 0);

      if (WIFSTOPPED (wstatus))
        stop_signum = WSTOPSIG (wstatus);
      else
        stop_signum = 0;

      switch (stop_signum)
        {
        case SIGTRAP:
          ptrace_event = (wstatus >> 16) & 0xffff;
          switch (ptrace_event)
            {
            case 0:
              g_assert_no_errno (ptrace (PTRACE_CONT, pid, NULL, stop_signum));
              break;
            default:
              g_assert_no_errno (ptrace (PTRACE_CONT, pid, NULL, 0));
              break;
            }
          break;
        case SIGSTOP:
          g_assert_no_errno (ptrace (PTRACE_CONT, pid, NULL, 0));
          break;
        default:
          if (!WIFEXITED (wstatus) && !WIFSIGNALED (wstatus))
            g_assert_no_errno (ptrace (PTRACE_CONT, pid, NULL, stop_signum));
          break;
        }

      if (pid == main_child)
        {
          g_assert_false (WIFSIGNALED (wstatus));
          if (WIFEXITED (wstatus))
            {
              g_assert_cmpint (WEXITSTATUS (wstatus), ==, EXIT_SUCCESS);
              break;
            }
        }
    }
}

#endif  /* __linux__ */

static void
test_exit_status_trapped (void)
{
#ifndef _GLIB_ADDRESS_SANITIZER

#ifdef __linux__
  GPtrArray *args = NULL;
  pid_t test_child;
#endif

  g_test_summary ("Test that exit status is reported correctly for ptrace()d child processes");
  g_test_bug ("https://gitlab.gnome.org/GNOME/glib/-/merge_requests/3433");

#ifdef __linux__
  /* Call fork() directly here, rather than using #GSubprocess, so that we can
   * safely call waitpid() on it ourselves without interfering with the internals
   * of #GSubprocess.
   * See https://gitlab.gnome.org/GNOME/glib/-/merge_requests/3433#note_1749055 */
  args = get_test_subprocess_args ("sleep-and-kill", NULL);
  test_child = fork ();
  if (test_child == 0)
    {
      /* Between fork() and exec() we can only call async-signal-safe functions. */
      if (ptrace (PTRACE_TRACEME, 0, NULL, NULL) < 0)
        abort ();

      g_assert_no_errno (execvp (args->pdata[0], (char * const *) args->pdata));
    }

  trace_children (test_child);

  g_clear_pointer (&args, g_ptr_array_unref);
#else
  g_test_skip ("ptrace() support for this test is only tested on Linux");
#endif

#else /* if defined (_GLIB_ADDRESS_SANITIZER) */

g_test_skip ("LeakSanitizer does not work under ptrace");
(void) trace_children;

#endif /* _GLIB_ADDRESS_SANITIZER */
}

#endif  /* G_OS_UNIX */

static void
test_launcher_environment (void)
{
  GSubprocessLauncher *launcher;
  GError *error = NULL;
  GSubprocess *proc;
  GPtrArray *args;
  gchar *out;

  g_setenv ("A", "B", TRUE);
  g_setenv ("C", "D", TRUE);

  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);

  /* unset a variable */
  g_subprocess_launcher_unsetenv (launcher, "A");

  /* and set a different one */
  g_subprocess_launcher_setenv (launcher, "E", "F", TRUE);

  args = get_test_subprocess_args ("printenv", "A", "C", "E", NULL);
  proc = g_subprocess_launcher_spawnv (launcher, (const gchar **) args->pdata, &error);
  g_assert_no_error (error);
  g_assert_nonnull (proc);

  g_subprocess_communicate_utf8 (proc, NULL, NULL, &out, NULL, &error);
  g_assert_no_error (error);

  g_assert_cmpstr (out, ==, "C=D" LINEEND "E=F" LINEEND);
  g_free (out);

  g_object_unref (proc);
  g_object_unref (launcher);
  g_ptr_array_unref (args);
}

int
main (int argc, char **argv)
{
  const struct
    {
      const gchar *subtest;
      GSubprocessFlags flags;
    }
  flags_vectors[] =
    {
      { "", G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_MERGE },
      { "/no-pipes", G_SUBPROCESS_FLAGS_NONE },
      { "/separate-stderr", G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE },
      { "/stdout-only", G_SUBPROCESS_FLAGS_STDOUT_PIPE },
      { "/stderr-only", G_SUBPROCESS_FLAGS_STDERR_PIPE },
      { "/stdout-silence", G_SUBPROCESS_FLAGS_STDOUT_SILENCE },
    };
  gsize i;

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gsubprocess/noop", test_noop);
  g_test_add_func ("/gsubprocess/noop-all-to-null", test_noop_all_to_null);
  g_test_add_func ("/gsubprocess/noop-no-wait", test_noop_no_wait);
  g_test_add_func ("/gsubprocess/noop-stdin-inherit", test_noop_stdin_inherit);
#ifdef G_OS_UNIX
  g_test_add_func ("/gsubprocess/search-path", test_search_path);
  g_test_add_func ("/gsubprocess/search-path-from-envp", test_search_path_from_envp);
  g_test_add_func ("/gsubprocess/signal", test_signal);
#endif
  g_test_add_func ("/gsubprocess/exit1", test_exit1);
  g_test_add_func ("/gsubprocess/exit1/cancel", test_exit1_cancel);
  g_test_add_func ("/gsubprocess/exit1/cancel_in_cb", test_exit1_cancel_in_cb);
  g_test_add_func ("/gsubprocess/echo1", test_echo1);
#ifdef G_OS_UNIX
  g_test_add_func ("/gsubprocess/echo-merged", test_echo_merged);
#endif
  g_test_add_func ("/gsubprocess/cat-utf8", test_cat_utf8);
  g_test_add_func ("/gsubprocess/cat-eof", test_cat_eof);
  g_test_add_func ("/gsubprocess/multi1", test_multi_1);

  /* Add various tests for g_subprocess_communicate() with different flags. */
  for (i = 0; i < G_N_ELEMENTS (flags_vectors); i++)
    {
      gchar *test_path = NULL;

      test_path = g_strdup_printf ("/gsubprocess/communicate%s", flags_vectors[i].subtest);
      g_test_add_data_func (test_path, GINT_TO_POINTER (flags_vectors[i].flags),
                            test_communicate);
      g_free (test_path);

      test_path = g_strdup_printf ("/gsubprocess/communicate/cancelled%s", flags_vectors[i].subtest);
      g_test_add_data_func (test_path, GINT_TO_POINTER (flags_vectors[i].flags),
                            test_communicate_cancelled);
      g_free (test_path);

      test_path = g_strdup_printf ("/gsubprocess/communicate/async%s", flags_vectors[i].subtest);
      g_test_add_data_func (test_path, GINT_TO_POINTER (flags_vectors[i].flags),
                            test_communicate_async);
      g_free (test_path);

      test_path = g_strdup_printf ("/gsubprocess/communicate/async/cancelled%s", flags_vectors[i].subtest);
      g_test_add_data_func (test_path, GINT_TO_POINTER (flags_vectors[i].flags),
                            test_communicate_cancelled_async);
      g_free (test_path);

      test_path = g_strdup_printf ("/gsubprocess/communicate/utf8%s", flags_vectors[i].subtest);
      g_test_add_data_func (test_path, GINT_TO_POINTER (flags_vectors[i].flags),
                            test_communicate_utf8);
      g_free (test_path);

      test_path = g_strdup_printf ("/gsubprocess/communicate/utf8/cancelled%s", flags_vectors[i].subtest);
      g_test_add_data_func (test_path, GINT_TO_POINTER (flags_vectors[i].flags),
                            test_communicate_utf8_cancelled);
      g_free (test_path);

      test_path = g_strdup_printf ("/gsubprocess/communicate/utf8/async%s", flags_vectors[i].subtest);
      g_test_add_data_func (test_path, GINT_TO_POINTER (flags_vectors[i].flags),
                            test_communicate_utf8_async);
      g_free (test_path);

      test_path = g_strdup_printf ("/gsubprocess/communicate/utf8/async/cancelled%s", flags_vectors[i].subtest);
      g_test_add_data_func (test_path, GINT_TO_POINTER (flags_vectors[i].flags),
                            test_communicate_utf8_cancelled_async);
      g_free (test_path);
    }

  g_test_add_func ("/gsubprocess/communicate/utf8/async/invalid", test_communicate_utf8_async_invalid);
  g_test_add_func ("/gsubprocess/communicate/utf8/invalid", test_communicate_utf8_invalid);
  g_test_add_func ("/gsubprocess/communicate/nothing", test_communicate_nothing);
  g_test_add_func ("/gsubprocess/terminate", test_terminate);
  g_test_add_func ("/gsubprocess/fail-initialization", test_fail_initialization);
  g_test_add_func ("/gsubprocess/env", test_env);
  g_test_add_func ("/gsubprocess/env/inherit", test_env_inherit);
  g_test_add_func ("/gsubprocess/cwd", test_cwd);
#ifdef G_OS_UNIX
  g_test_add_func ("/gsubprocess/launcher-close", test_subprocess_launcher_close);
  g_test_add_func ("/gsubprocess/stdout-file", test_stdout_file);
  g_test_add_func ("/gsubprocess/stdout-fd", test_stdout_fd);
  g_test_add_func ("/gsubprocess/child-setup", test_child_setup);
  g_test_add_func ("/gsubprocess/pass-fd/basic", test_pass_fd);
  g_test_add_func ("/gsubprocess/pass-fd/empty-child-setup", test_pass_fd_empty_child_setup);
  g_test_add_func ("/gsubprocess/pass-fd/inherit-fds", test_pass_fd_inherit_fds);
  g_test_add_func ("/gsubprocess/fd-conflation/basic", test_fd_conflation);
  g_test_add_func ("/gsubprocess/fd-conflation/empty-child-setup", test_fd_conflation_empty_child_setup);
  g_test_add_func ("/gsubprocess/fd-conflation/inherit-fds", test_fd_conflation_inherit_fds);
  g_test_add_func ("/gsubprocess/fd-conflation/child-err-report-fd", test_fd_conflation_child_err_report_fd);
  g_test_add_func ("/gsubprocess/exit-status/trapped", test_exit_status_trapped);
#endif
  g_test_add_func ("/gsubprocess/launcher-environment", test_launcher_environment);

  return g_test_run ();
}
