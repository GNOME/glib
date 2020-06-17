/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2011 Collabora Ltd.
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
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#include <locale.h>

#include <gio/gio.h>

/* How long to wait in ms for each iteration */
#define WAIT_ITERATION (10)

static gint num_async_operations = 0;

typedef struct
{
  guint iterations_requested;  /* construct-only */
  volatile guint iterations_done;  /* (atomic) */
} MockOperationData;

static void
mock_operation_free (gpointer user_data)
{
  MockOperationData *data = user_data;
  g_free (data);
}

static void
mock_operation_thread (GTask        *task,
                       gpointer      source_object,
                       gpointer      task_data,
                       GCancellable *cancellable)
{
  MockOperationData *data = task_data;
  guint i;

  for (i = 0; i < data->iterations_requested; i++)
    {
      if (g_cancellable_is_cancelled (cancellable))
        break;
      if (g_test_verbose ())
        g_test_message ("THRD: %u iteration %u", data->iterations_requested, i);
      g_usleep (WAIT_ITERATION * 1000);
    }

  if (g_test_verbose ())
    g_test_message ("THRD: %u stopped at %u", data->iterations_requested, i);
  g_atomic_int_add (&data->iterations_done, i);

  g_task_return_boolean (task, TRUE);
}

static gboolean
mock_operation_timeout (gpointer user_data)
{
  GTask *task;
  MockOperationData *data;
  gboolean done = FALSE;
  guint iterations_done;

  task = G_TASK (user_data);
  data = g_task_get_task_data (task);
  iterations_done = g_atomic_int_get (&data->iterations_done);

  if (iterations_done >= data->iterations_requested)
      done = TRUE;

  if (g_cancellable_is_cancelled (g_task_get_cancellable (task)))
      done = TRUE;

  if (done)
    {
      if (g_test_verbose ())
        g_test_message ("LOOP: %u stopped at %u",
                        data->iterations_requested, iterations_done);
      g_task_return_boolean (task, TRUE);
      return G_SOURCE_REMOVE;
    }
  else
    {
      g_atomic_int_inc (&data->iterations_done);
      if (g_test_verbose ())
        g_test_message ("LOOP: %u iteration %u",
                        data->iterations_requested, iterations_done + 1);
      return G_SOURCE_CONTINUE;
    }
}

static void
mock_operation_async (guint                wait_iterations,
                      gboolean             run_in_thread,
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data)
{
  GTask *task;
  MockOperationData *data;

  task = g_task_new (NULL, cancellable, callback, user_data);
  data = g_new0 (MockOperationData, 1);
  data->iterations_requested = wait_iterations;
  g_task_set_task_data (task, data, mock_operation_free);

  if (run_in_thread)
    {
      g_task_run_in_thread (task, mock_operation_thread);
      if (g_test_verbose ())
        g_test_message ("THRD: %d started", wait_iterations);
    }
  else
    {
      g_timeout_add_full (G_PRIORITY_DEFAULT, WAIT_ITERATION, mock_operation_timeout,
                          g_object_ref (task), g_object_unref);
      if (g_test_verbose ())
        g_test_message ("LOOP: %d started", wait_iterations);
    }

  g_object_unref (task);
}

static guint
mock_operation_finish (GAsyncResult  *result,
                       GError       **error)
{
  MockOperationData *data;
  GTask *task;

  g_assert_true (g_task_is_valid (result, NULL));

  /* This test expects the return value to be iterations_done even
   * when an error is set.
   */
  task = G_TASK (result);
  data = g_task_get_task_data (task);

  g_task_propagate_boolean (task, error);
  return g_atomic_int_get (&data->iterations_done);
}

GMainLoop *loop;

static void
on_mock_operation_ready (GObject      *source,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  guint iterations_requested;
  guint iterations_done;
  GError *error = NULL;

  iterations_requested = GPOINTER_TO_UINT (user_data);
  iterations_done = mock_operation_finish (result, &error);

  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
  g_error_free (error);

  g_assert_cmpint (iterations_requested, >, iterations_done);
  num_async_operations--;

  if (!num_async_operations)
    g_main_loop_quit (loop);
}

static gboolean
on_main_loop_timeout_quit (gpointer user_data)
{
  GMainLoop *loop = user_data;
  g_main_loop_quit (loop);
  return FALSE;
}

static void
test_cancel_multiple_concurrent (void)
{
  GCancellable *cancellable;
  guint i, iterations;

  if (!g_test_thorough ())
    {
      g_test_skip ("Not running timing heavy test");
      return;
    }

  cancellable = g_cancellable_new ();
  loop = g_main_loop_new (NULL, FALSE);

  for (i = 0; i < 45; i++)
    {
      iterations = i + 10;
      mock_operation_async (iterations, g_random_boolean (), cancellable,
                            on_mock_operation_ready, GUINT_TO_POINTER (iterations));
      num_async_operations++;
    }

  /* Wait for two iterations, to give threads a chance to start up */
  g_timeout_add (WAIT_ITERATION * 2, on_main_loop_timeout_quit, loop);
  g_main_loop_run (loop);
  g_assert_cmpint (num_async_operations, ==, 45);
  if (g_test_verbose ())
    g_test_message ("CANCEL: %d operations", num_async_operations);
  g_cancellable_cancel (cancellable);
  g_assert_true (g_cancellable_is_cancelled (cancellable));

  /* Wait for all operations to be cancelled */
  g_main_loop_run (loop);
  g_assert_cmpint (num_async_operations, ==, 0);

  g_object_unref (cancellable);
  g_main_loop_unref (loop);
}

static void
test_cancel_null (void)
{
  g_cancellable_cancel (NULL);
}

typedef struct
{
  GCond cond;
  GMutex mutex;
  gboolean thread_ready;
  GAsyncQueue *cancellable_source_queue;  /* (owned) (element-type GCancellableSource) */
} ThreadedDisposeData;

static gboolean
cancelled_cb (GCancellable *cancellable,
              gpointer      user_data)
{
  /* Nothing needs to be done here. */
  return G_SOURCE_CONTINUE;
}

static gpointer
threaded_dispose_thread_cb (gpointer user_data)
{
  ThreadedDisposeData *data = user_data;
  GSource *cancellable_source;

  g_mutex_lock (&data->mutex);
  data->thread_ready = TRUE;
  g_cond_broadcast (&data->cond);
  g_mutex_unlock (&data->mutex);

  while ((cancellable_source = g_async_queue_pop (data->cancellable_source_queue)) != (gpointer) 1)
    {
      /* Race with cancellation of the cancellable. */
      g_source_unref (cancellable_source);
    }

  return NULL;
}

static void
test_cancellable_source_threaded_dispose (void)
{
  ThreadedDisposeData data;
  GThread *thread = NULL;
  guint i;
  GPtrArray *cancellables_pending_unref = g_ptr_array_new_with_free_func (g_object_unref);

  g_test_summary ("Test a thread race between disposing of a GCancellableSource "
                  "(in one thread) and cancelling the GCancellable it refers "
                  "to (in another thread)");
  g_test_bug ("https://gitlab.gnome.org/GNOME/glib/issues/1841");

  /* Create a new thread and wait until it’s ready to execute. Each iteration of
   * the test will pass it a new #GCancellableSource. */
  g_cond_init (&data.cond);
  g_mutex_init (&data.mutex);
  data.cancellable_source_queue = g_async_queue_new_full ((GDestroyNotify) g_source_unref);
  data.thread_ready = FALSE;

  g_mutex_lock (&data.mutex);
  thread = g_thread_new ("/cancellable-source/threaded-dispose",
                         threaded_dispose_thread_cb, &data);

  while (!data.thread_ready)
    g_cond_wait (&data.cond, &data.mutex);
  g_mutex_unlock (&data.mutex);

  for (i = 0; i < 100000; i++)
    {
      GCancellable *cancellable = NULL;
      GSource *cancellable_source = NULL;

      /* Create a cancellable and a cancellable source for it. For this test,
       * there’s no need to attach the source to a #GMainContext. */
      cancellable = g_cancellable_new ();
      cancellable_source = g_cancellable_source_new (cancellable);
      g_source_set_callback (cancellable_source, G_SOURCE_FUNC (cancelled_cb), NULL, NULL);

      /* Send it to the thread and wait until it’s ready to execute before
       * cancelling our cancellable. */
      g_async_queue_push (data.cancellable_source_queue, g_steal_pointer (&cancellable_source));

      /* Race with disposal of the cancellable source. */
      g_cancellable_cancel (cancellable);

      /* This thread can’t drop its reference to the #GCancellable here, as it
       * might not be the final reference (depending on how the race is
       * resolved: #GCancellableSource holds a strong ref on the #GCancellable),
       * and at this point we can’t guarantee to support disposing of a
       * #GCancellable in a different thread from where it’s created, especially
       * when signal handlers are connected to it. */
      g_ptr_array_add (cancellables_pending_unref, g_steal_pointer (&cancellable));
    }

  /* Indicate that the test has finished. Can’t use %NULL as #GAsyncQueue
   * doesn’t allow that.*/
  g_async_queue_push (data.cancellable_source_queue, (gpointer) 1);

  g_thread_join (g_steal_pointer (&thread));

  g_assert (g_async_queue_length (data.cancellable_source_queue) == 0);
  g_async_queue_unref (data.cancellable_source_queue);
  g_mutex_clear (&data.mutex);
  g_cond_clear (&data.cond);

  g_ptr_array_unref (cancellables_pending_unref);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/cancellable/multiple-concurrent", test_cancel_multiple_concurrent);
  g_test_add_func ("/cancellable/null", test_cancel_null);
  g_test_add_func ("/cancellable-source/threaded-dispose", test_cancellable_source_threaded_dispose);

  return g_test_run ();
}
