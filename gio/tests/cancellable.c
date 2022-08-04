/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2011 Collabora Ltd.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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

#include "glib/glib-private.h"

/* How long to wait in ms for each iteration */
#define WAIT_ITERATION (10)

static gint num_async_operations = 0;

typedef struct
{
  guint iterations_requested;  /* construct-only */
  guint iterations_done;  /* (atomic) */
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
  g_main_context_wakeup (NULL);
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

  for (i = 0; i < 45; i++)
    {
      iterations = i + 10;
      mock_operation_async (iterations, g_random_boolean (), cancellable,
                            on_mock_operation_ready, GUINT_TO_POINTER (iterations));
      num_async_operations++;
    }

  /* Wait for the threads to start up */
  while (num_async_operations != 45)
    g_main_context_iteration (NULL, TRUE);
  g_assert_cmpint (num_async_operations, ==, 45);\

  if (g_test_verbose ())
    g_test_message ("CANCEL: %d operations", num_async_operations);
  g_cancellable_cancel (cancellable);
  g_assert_true (g_cancellable_is_cancelled (cancellable));

  /* Wait for all operations to be cancelled */
  while (num_async_operations != 0)
    g_main_context_iteration (NULL, TRUE);
  g_assert_cmpint (num_async_operations, ==, 0);

  g_object_unref (cancellable);
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
#ifdef _GLIB_ADDRESS_SANITIZER
  g_test_incomplete ("FIXME: Leaks lots of GCancellableSource objects, see glib#2309");
  (void) cancelled_cb;
  (void) threaded_dispose_thread_cb;
#else
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
       * when signal handlers are connected to it.
       *
       * So this is a workaround for a disposal-in-another-thread bug for
       * #GCancellable, but there’s no hope of debugging and resolving it with
       * this test setup, and the bug is orthogonal to what’s being tested here
       * (a race between #GCancellable and #GCancellableSource). */
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
#endif
}

static void
test_cancellable_poll_fd (void)
{
  GCancellable *cancellable;
  GPollFD pollfd = {.fd = -1};
  int fd = -1;

#ifdef G_OS_WIN32
  g_test_skip ("Platform not supported");
  return;
#endif

  cancellable = g_cancellable_new ();

  g_assert_true (g_cancellable_make_pollfd (cancellable, &pollfd));
  g_assert_cmpint (pollfd.fd, >, 0);

  fd = g_cancellable_get_fd (cancellable);
  g_assert_cmpint (fd, >, 0);

  g_cancellable_release_fd (cancellable);
  g_cancellable_release_fd (cancellable);

  g_object_unref (cancellable);
}

static void
test_cancellable_cancelled_poll_fd (void)
{
  GCancellable *cancellable;
  GPollFD pollfd;

#ifdef G_OS_WIN32
  g_test_skip ("Platform not supported");
  return;
#endif

  g_test_summary ("Tests that cancellation wakes up a pollable FD on creation");

  cancellable = g_cancellable_new ();
  g_assert_true (g_cancellable_make_pollfd (cancellable, &pollfd));
  g_cancellable_cancel (cancellable);

  g_poll (&pollfd, 1, -1);

  g_cancellable_release_fd (cancellable);
  g_object_unref (cancellable);
}

typedef struct {
  GCancellable *cancellable;
  gboolean polling_started; /* Atomic */
} CancellablePollThreadData;

static gpointer
cancel_cancellable_thread (gpointer user_data)
{
  CancellablePollThreadData *thread_data = user_data;

  while (!g_atomic_int_get (&thread_data->polling_started))
    ;

  /* Let's just wait a moment before cancelling, this is not really needed
   * but we do it to simulate that the thread is actually doing something.
   */
  g_usleep (G_USEC_PER_SEC / 10);
  g_cancellable_cancel (thread_data->cancellable);

  return NULL;
}

static gpointer
polling_cancelled_cancellable_thread (gpointer user_data)
{
  CancellablePollThreadData *thread_data = user_data;
  GPollFD pollfd;

  g_assert_true (g_cancellable_make_pollfd (thread_data->cancellable, &pollfd));
  g_atomic_int_set (&thread_data->polling_started, TRUE);

  g_poll (&pollfd, 1, -1);

  g_cancellable_release_fd (thread_data->cancellable);

  return NULL;
}

static void
test_cancellable_cancelled_poll_fd_threaded (void)
{
  GCancellable *cancellable;
  CancellablePollThreadData thread_data = {0};
  GThread *polling_thread = NULL;
  GThread *cancelling_thread = NULL;
  GPollFD pollfd;

#ifdef G_OS_WIN32
  g_test_skip ("Platform not supported");
  return;
#endif

  g_test_summary ("Tests that a cancellation wakes up a pollable FD");

  cancellable = g_cancellable_new ();
  g_assert_true (g_cancellable_make_pollfd (cancellable, &pollfd));

  thread_data.cancellable = cancellable;

  polling_thread = g_thread_new ("/cancellable/poll-fd-cancelled-threaded/polling",
                                 polling_cancelled_cancellable_thread,
                                 &thread_data);
  cancelling_thread = g_thread_new ("/cancellable/poll-fd-cancelled-threaded/cancelling",
                                    cancel_cancellable_thread, &thread_data);

  g_poll (&pollfd, 1, -1);
  g_assert_true (g_cancellable_is_cancelled (cancellable));
  g_cancellable_release_fd (cancellable);

  g_thread_join (g_steal_pointer (&cancelling_thread));
  g_thread_join (g_steal_pointer (&polling_thread));

  g_object_unref (cancellable);
}

typedef struct {
  GMainLoop *loop;
  GCancellable *cancellable;
  GCallback callback;
  gboolean is_disconnecting;
  gboolean is_resetting;
  gpointer handler_id;
} ConnectingThreadData;

static void
on_cancellable_connect_disconnect (GCancellable *cancellable,
                                   ConnectingThreadData *data)
{
  gulong handler_id = (gulong) (guintptr) g_atomic_pointer_exchange (&data->handler_id, 0);
  g_atomic_int_set (&data->is_disconnecting, TRUE);
  g_cancellable_disconnect (cancellable, handler_id);
  g_atomic_int_set (&data->is_disconnecting, FALSE);
}

static gpointer
connecting_thread (gpointer user_data)
{
  GMainContext *context;
  ConnectingThreadData *data = user_data;
  gulong handler_id;
  GMainLoop *loop;

  handler_id =
    g_cancellable_connect (data->cancellable, data->callback, data, NULL);

  context = g_main_context_new ();
  g_main_context_push_thread_default (context);
  loop = g_main_loop_new (context, FALSE);

  g_atomic_pointer_set (&data->handler_id, (gpointer) (guintptr) handler_id);
  g_atomic_pointer_set (&data->loop, loop);
  g_main_loop_run (loop);

  g_main_context_pop_thread_default (context);
  g_main_context_unref (context);
  g_main_loop_unref (loop);

  return NULL;
}

static void
test_cancellable_disconnect_on_cancelled_callback_hangs (void)
{
  GCancellable *cancellable;
  GThread *thread = NULL;
  GThread *cancelling_thread = NULL;
  ConnectingThreadData thread_data = {0};
  GMainLoop *thread_loop;
  gpointer waited;

  /* While this is not convenient, it's done to ensure that we don't have a
   * race when trying to cancelling a cancellable that is about to be cancelled
   * in another thread
   */
  g_test_summary ("Tests that trying to disconnect a cancellable from the "
                  "cancelled signal callback will result in a deadlock "
                  "as per #GCancellable::cancelled");

  if (!g_test_undefined ())
    {
      g_test_skip ("Skipping testing disallowed behaviour of disconnecting from "
                  "a cancellable from its cancelled callback");
      return;
    }

  cancellable = g_cancellable_new ();
  thread_data.cancellable = cancellable;
  thread_data.callback = G_CALLBACK (on_cancellable_connect_disconnect);

  g_assert_false (g_atomic_int_get (&thread_data.is_disconnecting));
  g_assert_cmpuint ((gulong) (guintptr) g_atomic_pointer_get (&thread_data.handler_id), ==, 0);

  thread = g_thread_new ("/cancellable/disconnect-on-cancelled-callback-hangs",
                         connecting_thread, &thread_data);

  while (!g_atomic_pointer_get (&thread_data.loop))
    ;

  thread_loop = thread_data.loop;
  g_assert_cmpuint ((gulong) (guintptr) g_atomic_pointer_get (&thread_data.handler_id), !=, 0);

  /* FIXME: This thread will hang (at least that's what this test wants to
   *        ensure), but we can't stop it from the caller, unless we'll expose
   *        pthread_cancel (and similar) to GLib.
   *        So it will keep hanging till the test process is alive.
   */
  cancelling_thread = g_thread_new ("/cancellable/disconnect-on-cancelled-callback-hangs",
                                    (GThreadFunc) g_cancellable_cancel,
                                    cancellable);

  while (!g_cancellable_is_cancelled (cancellable) ||
         !g_atomic_int_get (&thread_data.is_disconnecting))
    ;

  g_assert_true (g_atomic_int_get (&thread_data.is_disconnecting));
  g_assert_cmpuint ((gulong) (guintptr) g_atomic_pointer_get (&thread_data.handler_id), ==, 0);

  waited = &waited;
  g_timeout_add_once (100, (GSourceOnceFunc) g_nullify_pointer, &waited);
  while (waited != NULL)
    g_main_context_iteration (NULL, TRUE);

  g_assert_true (g_atomic_int_get (&thread_data.is_disconnecting));

  g_main_loop_quit (thread_loop);
  g_assert_true (g_atomic_int_get (&thread_data.is_disconnecting));

  g_thread_join (g_steal_pointer (&thread));
  g_thread_unref (cancelling_thread);
  g_object_unref (cancellable);
}

static void
on_cancelled_reset (GCancellable *cancellable,
                    gpointer data)
{
  ConnectingThreadData *thread_data = data;

  g_assert_true (g_cancellable_is_cancelled (cancellable));
  g_atomic_int_set (&thread_data->is_resetting, TRUE);
  g_cancellable_reset (cancellable);
  g_assert_false (g_cancellable_is_cancelled (cancellable));
  g_atomic_int_set (&thread_data->is_resetting, TRUE);
}

static void
test_cancellable_reset_on_cancelled_callback_hangs (void)
{
  GCancellable *cancellable;
  GThread *thread = NULL;
  GThread *cancelling_thread = NULL;
  ConnectingThreadData thread_data = {0};
  GMainLoop *thread_loop;
  gpointer waited;

  /* While this is not convenient, it's done to ensure that we don't have a
   * race when trying to cancelling a cancellable that is about to be cancelled
   * in another thread
   */
  g_test_summary ("Tests that trying to reset a cancellable from the "
                  "cancelled signal callback will result in a deadlock "
                  "as per #GCancellable::cancelled");

  if (!g_test_undefined ())
    {
      g_test_skip ("Skipping testing disallowed behaviour of resetting a "
                  "cancellable from its callback");
      return;
    }

  cancellable = g_cancellable_new ();
  thread_data.cancellable = cancellable;
  thread_data.callback = G_CALLBACK (on_cancelled_reset);

  g_assert_false (g_atomic_int_get (&thread_data.is_resetting));
  g_assert_cmpuint ((gulong) (guintptr) g_atomic_pointer_get (&thread_data.handler_id), ==, 0);

  thread = g_thread_new ("/cancellable/reset-on-cancelled-callback-hangs",
                         connecting_thread, &thread_data);

  while (!g_atomic_pointer_get (&thread_data.loop))
    ;

  thread_loop = thread_data.loop;
  g_assert_cmpuint ((gulong) (guintptr) g_atomic_pointer_get (&thread_data.handler_id), !=, 0);

  /* FIXME: This thread will hang (at least that's what this test wants to
   *        ensure), but we can't stop it from the caller, unless we'll expose
   *        pthread_cancel (and similar) to GLib.
   *        So it will keep hanging till the test process is alive.
   */
  cancelling_thread = g_thread_new ("/cancellable/reset-on-cancelled-callback-hangs",
                                    (GThreadFunc) g_cancellable_cancel,
                                    cancellable);

  while (!g_cancellable_is_cancelled (cancellable) ||
         !g_atomic_int_get (&thread_data.is_resetting))
    ;

  g_assert_true (g_atomic_int_get (&thread_data.is_resetting));
  g_assert_cmpuint ((gulong) (guintptr) g_atomic_pointer_get (&thread_data.handler_id), >, 0);

  waited = &waited;
  g_timeout_add_once (100, (GSourceOnceFunc) g_nullify_pointer, &waited);
  while (waited != NULL)
    g_main_context_iteration (NULL, TRUE);

  g_assert_true (g_atomic_int_get (&thread_data.is_resetting));

  g_main_loop_quit (thread_loop);
  g_assert_true (g_atomic_int_get (&thread_data.is_resetting));

  g_thread_join (g_steal_pointer (&thread));
  g_thread_unref (cancelling_thread);
  g_object_unref (cancellable);
}

static gpointer
repeatedly_cancelling_thread (gpointer data)
{
  GCancellable *cancellable = data;
  const guint iterations = 10000;

  for (guint i = 0; i < iterations; ++i)
    g_cancellable_cancel (cancellable);

  return NULL;
}

static gpointer
repeatedly_resetting_thread (gpointer data)
{
  GCancellable *cancellable = data;
  const guint iterations = 10000;

  for (guint i = 0; i < iterations; ++i)
    g_cancellable_reset (cancellable);

  return NULL;
}

static void
on_racy_cancellable_cancelled (GCancellable *cancellable,
                               gpointer data)
{
  gboolean *callback_called = data;

  g_assert_true (g_cancellable_is_cancelled (cancellable));
  g_atomic_int_set (callback_called, TRUE);
}

static void
test_cancellable_cancel_reset_races (void)
{
  GCancellable *cancellable;
  GThread *resetting_thread = NULL;
  GThread *cancelling_thread = NULL;
  gboolean callback_called = FALSE;

  g_test_summary ("Tests threads racing for cancelling and resetting a GCancellable");

  cancellable = g_cancellable_new ();

  g_cancellable_connect (cancellable, G_CALLBACK (on_racy_cancellable_cancelled),
                         &callback_called, NULL);
  g_assert_false (callback_called);

  resetting_thread = g_thread_new ("/cancellable/cancel-reset-races/resetting",
                                   repeatedly_resetting_thread,
                                   cancellable);
  cancelling_thread = g_thread_new ("/cancellable/cancel-reset-races/cancelling",
                                    repeatedly_cancelling_thread, cancellable);

  g_thread_join (g_steal_pointer (&cancelling_thread));
  g_thread_join (g_steal_pointer (&resetting_thread));

  g_assert_true (callback_called);

  g_object_unref (cancellable);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/cancellable/multiple-concurrent", test_cancel_multiple_concurrent);
  g_test_add_func ("/cancellable/null", test_cancel_null);
  g_test_add_func ("/cancellable/disconnect-on-cancelled-callback-hangs", test_cancellable_disconnect_on_cancelled_callback_hangs);
  g_test_add_func ("/cancellable/resets-on-cancel-callback-hangs", test_cancellable_reset_on_cancelled_callback_hangs);
  g_test_add_func ("/cancellable/poll-fd", test_cancellable_poll_fd);
  g_test_add_func ("/cancellable/poll-fd-cancelled", test_cancellable_cancelled_poll_fd);
  g_test_add_func ("/cancellable/poll-fd-cancelled-threaded", test_cancellable_cancelled_poll_fd_threaded);
  g_test_add_func ("/cancellable/cancel-reset-races", test_cancellable_cancel_reset_races);
  g_test_add_func ("/cancellable-source/threaded-dispose", test_cancellable_source_threaded_dispose);

  return g_test_run ();
}
