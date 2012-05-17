#include <gio/gio.h>
#include <string.h>

#ifdef G_OS_UNIX
#include <sys/wait.h>
#include <gio/gfiledescriptorbased.h>
#endif

static GPtrArray *
get_test_subprocess_args (const char *mode,
			  ...) G_GNUC_NULL_TERMINATED;

static GPtrArray *
get_test_subprocess_args (const char *mode,
			  ...)
{
  GPtrArray *ret;
  char *cwd;
  char *cwd_path;
  const char *binname;
  va_list args;
  gpointer arg;

  ret = g_ptr_array_new_with_free_func (g_free);

  cwd = g_get_current_dir ();

#ifdef G_OS_WIN32
  binname = "gsubprocess-testprog.exe";
#else
  binname = "gsubprocess-testprog";
#endif

  cwd_path = g_build_filename (cwd, binname, NULL);
  g_free (cwd);
  g_ptr_array_add (ret, cwd_path);
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
  proc = g_subprocess_new_simple_argv ((gchar**) args->pdata,
                                       G_SUBPROCESS_STREAM_DISPOSITION_INHERIT,
                                       G_SUBPROCESS_STREAM_DISPOSITION_INHERIT,
                                       error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  (void)g_subprocess_wait_sync_check (proc, NULL, error);
  g_assert_no_error (local_error);
  
  g_object_unref (proc);
}

static void
test_noop_all_to_null (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GPtrArray *args;
  GSubprocess *proc;

  args = get_test_subprocess_args ("noop", NULL);
  proc = g_subprocess_new_simple_argv ((gchar**) args->pdata,
                                       G_SUBPROCESS_STREAM_DISPOSITION_NULL,
                                       G_SUBPROCESS_STREAM_DISPOSITION_NULL,
                                       error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  (void)g_subprocess_wait_sync_check (proc, NULL, error);
  g_assert_no_error (local_error);
  
  g_object_unref (proc);
}

static void
test_noop_no_wait (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GPtrArray *args;
  GSubprocess *proc;

  args = get_test_subprocess_args ("noop", NULL);
  proc = g_subprocess_new_simple_argv ((gchar **) args->pdata,
                                       G_SUBPROCESS_STREAM_DISPOSITION_INHERIT,
                                       G_SUBPROCESS_STREAM_DISPOSITION_INHERIT,
                                       error);
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
  GSubprocessContext *context;

  args = get_test_subprocess_args ("noop", NULL);
  context = g_subprocess_context_new ((gchar**) args->pdata);
  g_subprocess_context_set_stdin_disposition (context, G_SUBPROCESS_STREAM_DISPOSITION_INHERIT);
  proc = g_subprocess_new (context, error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  (void)g_subprocess_wait_sync_check (proc, NULL, error);
  g_assert_no_error (local_error);
  
  g_object_unref (proc);
  g_object_unref (context);
}


#ifdef G_OS_UNIX
static void
test_search_path (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GSubprocess *proc;

  proc = g_subprocess_new_simple_argl (G_SUBPROCESS_STREAM_DISPOSITION_INHERIT,
                                       G_SUBPROCESS_STREAM_DISPOSITION_INHERIT,
                                       error,
                                       "true", NULL);
  g_assert_no_error (local_error);

  (void)g_subprocess_wait_sync_check (proc, NULL, error);
  g_assert_no_error (local_error);
  
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
  proc = g_subprocess_new_simple_argv ((gchar **) args->pdata,
                                       G_SUBPROCESS_STREAM_DISPOSITION_INHERIT,
                                       G_SUBPROCESS_STREAM_DISPOSITION_INHERIT,
                                       error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  (void)g_subprocess_wait_sync_check (proc, NULL, error);
  g_assert_error (local_error, G_SPAWN_EXIT_ERROR, 1);
  g_clear_error (error);

  g_object_unref (proc);
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
  GInputStream *stdout;
  gchar *result;

  args = get_test_subprocess_args ("echo", "hello", "world!", NULL);
  proc = g_subprocess_new_simple_argv ((gchar **) args->pdata,
                                       G_SUBPROCESS_STREAM_DISPOSITION_PIPE,
                                       G_SUBPROCESS_STREAM_DISPOSITION_INHERIT,
                                       error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  stdout = g_subprocess_get_stdout_pipe (proc);

  result = splice_to_string (stdout, error);
  g_assert_no_error (local_error);

  g_assert_cmpstr (result, ==, "hello\nworld!\n");

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
  GInputStream *stdout;
  gchar *result;

  args = get_test_subprocess_args ("echo-stdout-and-stderr", "merge", "this", NULL);
  proc = g_subprocess_new_simple_argv ((gchar **) args->pdata,
                                       G_SUBPROCESS_STREAM_DISPOSITION_PIPE,
                                       G_SUBPROCESS_STREAM_DISPOSITION_STDERR_MERGE,
                                       error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  stdout = g_subprocess_get_stdout_pipe (proc);
  result = splice_to_string (stdout, error);
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
  GSubprocessContext *context;
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
  context = g_subprocess_context_new ((gchar**)args->pdata);
  g_subprocess_context_set_stdin_disposition (context, G_SUBPROCESS_STREAM_DISPOSITION_PIPE);
  g_subprocess_context_set_stdout_disposition (context, G_SUBPROCESS_STREAM_DISPOSITION_PIPE);
  proc = g_subprocess_new (context, error);
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

  g_subprocess_wait_sync_check (proc, NULL, error);
  g_assert_no_error (local_error);

  output_buf = g_memory_output_stream_steal_as_bytes ((GMemoryOutputStream*)output_buf_stream);
  
  g_assert_cmpint (g_bytes_get_size (output_buf), ==, 13);
  g_assert_cmpint (memcmp (g_bytes_get_data (output_buf, NULL), "hello, world!", 13), ==, 0);

  g_bytes_unref (output_buf);
  g_main_loop_unref (data.loop);
  g_object_unref (input_buf_stream);
  g_object_unref (output_buf_stream);
  g_object_unref (proc);
  g_object_unref (context);
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
  const gchar *args[] = { "cat", NULL };
  GCancellable *cancellable;
  GError *error = NULL;
  GSubprocessContext *context;
  GSubprocess *cat;
  gint exit_status;
  gboolean result;
  gchar buffer;
  gssize s;

  /* Spawn 'cat' */
  context = g_subprocess_context_new ((gchar**)args);
  g_subprocess_context_set_stdin_disposition (context, G_SUBPROCESS_STREAM_DISPOSITION_PIPE);
  g_subprocess_context_set_stdout_disposition (context, G_SUBPROCESS_STREAM_DISPOSITION_PIPE);
  cat = g_subprocess_new (context, &error);
  g_assert_no_error (error);
  g_assert (cat);

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
  g_assert (result);

  /* Now check that reading cat's stdout gets us an EOF (since it quit) */
  s = g_input_stream_read (g_subprocess_get_stdout_pipe (cat), &buffer, sizeof buffer, NULL, &error);
  g_assert_no_error (error);
  g_assert (!s);

  /* Check that the process has exited as a result of the EOF */
  result = g_subprocess_wait_sync (cat, &exit_status, NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (exit_status, ==, 0);
  g_assert (result);

  g_object_unref (cat);
  g_object_unref (context);
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

  /* We write 2^1 + 2^2 ... + 2^10 or 2047 copies of "Hello World!\n"
   * ultimately
   */
  if (data->counter >= 2047 || data->caught_error)
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
	  if (!g_output_stream_write_all (data->first_stdin, "hello world!\n",
					  strlen ("hello world!\n"), &bytes_written,
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
  TestMultiSpliceData *data = user_data;
  GError *error = NULL;
  int exit_status;

  if (!g_subprocess_wait_finish ((GSubprocess*)object, result, &exit_status, &error))
    {
      if (!data->caught_error)
	{
	  data->caught_error = TRUE;
	  g_propagate_error (&data->error, error);
	}
    }
  g_spawn_check_exit_status (exit_status, &error);
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
  GSubprocessContext *context;
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
  context = g_subprocess_context_new ((gchar**)args->pdata);
  g_subprocess_context_set_stdin_disposition (context, G_SUBPROCESS_STREAM_DISPOSITION_PIPE);
  g_subprocess_context_set_stdout_disposition (context, G_SUBPROCESS_STREAM_DISPOSITION_PIPE);
  first = g_subprocess_new (context, error);
  g_assert_no_error (local_error);
  second = g_subprocess_new (context, error);
  g_assert_no_error (local_error);
  third = g_subprocess_new (context, error);
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
  g_subprocess_wait (first, NULL, on_subprocess_exited, &data);
  data.events_pending++;
  g_subprocess_wait (second, NULL, on_subprocess_exited, &data);
  data.events_pending++;
  g_subprocess_wait (third, NULL, on_subprocess_exited, &data);

  g_main_loop_run (data.loop);

  g_assert (!data.caught_error);
  g_assert_no_error (data.error);

  g_assert_cmpint (g_memory_output_stream_get_data_size ((GMemoryOutputStream*)membuf), ==, 26611);

  g_main_loop_unref (data.loop);
  g_object_unref (membuf);
  g_object_unref (context);
  g_object_unref (first);
  g_object_unref (second);
  g_object_unref (third);
}

static gboolean
send_terminate (gpointer   user_data)
{
  GSubprocess *proc = user_data;

  g_subprocess_force_exit (proc);

  return FALSE;
}

static void
on_request_quit_exited (GObject        *object,
			GAsyncResult   *result,
			gpointer        user_data)
{
  GError *error = NULL;
  int exit_status;
  
  (void)g_subprocess_wait_finish ((GSubprocess*)object, result, &exit_status, &error);
  g_assert_no_error (error);
#ifdef G_OS_UNIX
  g_assert (WIFSIGNALED (exit_status) && WTERMSIG (exit_status) == 9);
#endif
  g_spawn_check_exit_status (exit_status, &error);
  g_assert (error != NULL);
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

  args = get_test_subprocess_args ("sleep-forever", NULL);
  proc = g_subprocess_new_simple_argv ((gchar **) args->pdata,
                                       G_SUBPROCESS_STREAM_DISPOSITION_INHERIT,
                                       G_SUBPROCESS_STREAM_DISPOSITION_INHERIT,
                                       error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  loop = g_main_loop_new (NULL, TRUE);

  g_subprocess_wait (proc, NULL, on_request_quit_exited, loop);

  g_timeout_add_seconds (3, send_terminate, proc);

  g_main_loop_run (loop);

  g_main_loop_unref (loop);
  g_object_unref (proc);
}

#ifdef G_OS_UNIX
static void
test_stdout_file (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GSubprocessContext *context;
  GSubprocess *proc;
  GPtrArray *args;
  GFile *tmpfile;
  GFileIOStream *iostream;
  GOutputStream *stdin;
  const char *test_data = "this is some test data\n";
  char *tmp_contents;
  char *tmp_file_path;

  tmpfile = g_file_new_tmp ("gsubprocessXXXXXX", &iostream, error);
  g_assert_no_error (local_error);
  g_clear_object (&iostream);

  tmp_file_path = g_file_get_path (tmpfile);

  args = get_test_subprocess_args ("cat", NULL);
  context = g_subprocess_context_new ((gchar**)args->pdata);
  g_subprocess_context_set_stdin_disposition (context, G_SUBPROCESS_STREAM_DISPOSITION_PIPE);
  g_subprocess_context_set_stdout_file_path (context, tmp_file_path);
  proc = g_subprocess_new (context, error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  stdin = g_subprocess_get_stdin_pipe (proc);

  g_output_stream_write_all (stdin, test_data, strlen (test_data), NULL, NULL, error);
  g_assert_no_error (local_error);

  g_output_stream_close (stdin, NULL, error);
  g_assert_no_error (local_error);

  g_subprocess_wait_sync_check (proc, NULL, error);

  g_object_unref (context);
  g_object_unref (proc);

  g_file_load_contents (tmpfile, NULL, &tmp_contents, NULL, NULL, error);
  g_assert_no_error (local_error);

  g_assert_cmpstr (test_data, ==, tmp_contents);
  g_free (tmp_contents);

  (void) g_file_delete (tmpfile, NULL, NULL);
  g_free (tmp_file_path);
}

static void
test_stdout_fd (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GSubprocessContext *context;
  GSubprocess *proc;
  GPtrArray *args;
  GFile *tmpfile;
  GFileIOStream *iostream;
  GFileDescriptorBased *descriptor_stream;
  GOutputStream *stdin;
  const char *test_data = "this is some test data\n";
  char *tmp_contents;

  tmpfile = g_file_new_tmp ("gsubprocessXXXXXX", &iostream, error);
  g_assert_no_error (local_error);

  args = get_test_subprocess_args ("cat", NULL);
  context = g_subprocess_context_new ((gchar**)args->pdata);
  g_subprocess_context_set_stdin_disposition (context, G_SUBPROCESS_STREAM_DISPOSITION_PIPE);
  descriptor_stream = G_FILE_DESCRIPTOR_BASED (g_io_stream_get_output_stream (G_IO_STREAM (iostream)));
  g_subprocess_context_set_stdout_fd (context, g_file_descriptor_based_get_fd (descriptor_stream));
  proc = g_subprocess_new (context, error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  g_clear_object (&iostream);

  stdin = g_subprocess_get_stdin_pipe (proc);

  g_output_stream_write_all (stdin, test_data, strlen (test_data), NULL, NULL, error);
  g_assert_no_error (local_error);

  g_output_stream_close (stdin, NULL, error);
  g_assert_no_error (local_error);

  g_subprocess_wait_sync_check (proc, NULL, error);

  g_object_unref (context);
  g_object_unref (proc);

  g_file_load_contents (tmpfile, NULL, &tmp_contents, NULL, NULL, error);
  g_assert_no_error (local_error);

  g_assert_cmpstr (test_data, ==, tmp_contents);
  g_free (tmp_contents);

  (void) g_file_delete (tmpfile, NULL, NULL);
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
  GSubprocessContext *context;
  GSubprocess *proc;
  GPtrArray *args;
  GFile *tmpfile;
  GFileIOStream *iostream;
  GOutputStream *stdin;
  const char *test_data = "this is some test data\n";
  char *tmp_contents;
  int fd;

  tmpfile = g_file_new_tmp ("gsubprocessXXXXXX", &iostream, error);
  g_assert_no_error (local_error);

  fd = g_file_descriptor_based_get_fd (G_FILE_DESCRIPTOR_BASED (g_io_stream_get_output_stream (G_IO_STREAM (iostream))));

  args = get_test_subprocess_args ("cat", NULL);
  context = g_subprocess_context_new ((gchar**)args->pdata);
  g_subprocess_context_set_stdin_disposition (context, G_SUBPROCESS_STREAM_DISPOSITION_PIPE);
  g_subprocess_context_set_child_setup (context, child_setup, GINT_TO_POINTER (fd));
  proc = g_subprocess_new (context, error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  g_clear_object (&iostream);

  stdin = g_subprocess_get_stdin_pipe (proc);

  g_output_stream_write_all (stdin, test_data, strlen (test_data), NULL, NULL, error);
  g_assert_no_error (local_error);

  g_output_stream_close (stdin, NULL, error);
  g_assert_no_error (local_error);

  g_subprocess_wait_sync_check (proc, NULL, error);

  g_object_unref (context);
  g_object_unref (proc);

  g_file_load_contents (tmpfile, NULL, &tmp_contents, NULL, NULL, error);
  g_assert_no_error (local_error);

  g_assert_cmpstr (test_data, ==, tmp_contents);
  g_free (tmp_contents);

  (void) g_file_delete (tmpfile, NULL, NULL);
}
#endif

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gsubprocess/noop", test_noop);
  g_test_add_func ("/gsubprocess/noop-all-to-null", test_noop_all_to_null);
  g_test_add_func ("/gsubprocess/noop-no-wait", test_noop_no_wait);
  g_test_add_func ("/gsubprocess/noop-stdin-inherit", test_noop_stdin_inherit);
#ifdef G_OS_UNIX
  g_test_add_func ("/gsubprocess/search-path", test_search_path);
#endif
  g_test_add_func ("/gsubprocess/exit1", test_exit1);
  g_test_add_func ("/gsubprocess/echo1", test_echo1);
#ifdef G_OS_UNIX
  g_test_add_func ("/gsubprocess/echo-merged", test_echo_merged);
#endif
  g_test_add_func ("/gsubprocess/cat-utf8", test_cat_utf8);
  g_test_add_func ("/gsubprocess/cat-eof", test_cat_eof);
  g_test_add_func ("/gsubprocess/multi1", test_multi_1);
  g_test_add_func ("/gsubprocess/terminate", test_terminate);
#ifdef G_OS_UNIX
  g_test_add_func ("/gsubprocess/stdout-file", test_stdout_file);
  g_test_add_func ("/gsubprocess/stdout-fd", test_stdout_fd);
  g_test_add_func ("/gsubprocess/child-setup", test_child_setup);
#endif

  return g_test_run ();
}
