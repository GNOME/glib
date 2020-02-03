/* Unit tests for GThreadPool
 * Copyright (C) 2020 Sebastian Dröge <sebastian@centricular.com>
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In no event shall the authors or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 */

#include <config.h>

#include <glib.h>

typedef struct {
  GMutex mutex;
  GCond cond;
  gboolean signalled;
} MutexCond;

static void
pool_func (gpointer data, gpointer user_data)
{
  MutexCond *m = user_data;

  g_mutex_lock (&m->mutex);
  g_assert_false (m->signalled);
  g_assert (data == GUINT_TO_POINTER (123));
  m->signalled = TRUE;
  g_cond_signal (&m->cond);
  g_mutex_unlock (&m->mutex);
}

static void
test_shared (void)
{
  GThreadPool *pool;
  GError *err = NULL;
  MutexCond m;
  gboolean success;

  g_mutex_init (&m.mutex);
  g_cond_init (&m.cond);

  pool = g_thread_pool_new (pool_func, &m, -1, FALSE, &err);
  g_assert_no_error (err);
  g_assert_nonnull (pool);

  g_mutex_lock (&m.mutex);
  m.signalled = FALSE;

  success = g_thread_pool_push (pool, GUINT_TO_POINTER (123), &err);
  g_assert_no_error (err);
  g_assert_true (success);

  while (!m.signalled)
    g_cond_wait (&m.cond, &m.mutex);
  g_mutex_unlock (&m.mutex);

  g_thread_pool_free (pool, TRUE, TRUE);
}

static void
test_exclusive (void)
{
  GThreadPool *pool;
  GError *err = NULL;
  MutexCond m;
  gboolean success;

  g_mutex_init (&m.mutex);
  g_cond_init (&m.cond);

  pool = g_thread_pool_new (pool_func, &m, 2, TRUE, &err);
  g_assert_no_error (err);
  g_assert_nonnull (pool);

  g_mutex_lock (&m.mutex);
  m.signalled = FALSE;

  success = g_thread_pool_push (pool, GUINT_TO_POINTER (123), &err);
  g_assert_no_error (err);
  g_assert_true (success);

  while (!m.signalled)
    g_cond_wait (&m.cond, &m.mutex);
  g_mutex_unlock (&m.mutex);

  g_thread_pool_free (pool, TRUE, TRUE);
}

static void
dummy_pool_func (gpointer data, gpointer user_data)
{
  g_assert (data == GUINT_TO_POINTER (123));
}

static void
test_create_shared_after_exclusive (void)
{
  GThreadPool *pool;
  GError *err = NULL;
  gboolean success;

  if (!g_test_subprocess ())
    {
      g_test_trap_subprocess (NULL, 0, 0);
      g_test_trap_assert_passed ();
      return;
    }

  g_thread_pool_set_max_unused_threads (0);

  pool = g_thread_pool_new (dummy_pool_func, NULL, 2, TRUE, &err);
  g_assert_no_error (err);
  g_assert_nonnull (pool);

  success = g_thread_pool_push (pool, GUINT_TO_POINTER (123), &err);
  g_assert_no_error (err);
  g_assert_true (success);

  g_thread_pool_free (pool, TRUE, TRUE);

  pool = g_thread_pool_new (dummy_pool_func, NULL, -1, FALSE, &err);
  g_assert_no_error (err);
  g_assert_nonnull (pool);

  success = g_thread_pool_push (pool, GUINT_TO_POINTER (123), &err);
  g_assert_no_error (err);
  g_assert_true (success);

  g_thread_pool_free (pool, TRUE, TRUE);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/thread_pool/shared", test_shared);
  g_test_add_func ("/thread_pool/exclusive", test_exclusive);
  g_test_add_func ("/thread_pool/create_shared_after_exclusive", test_create_shared_after_exclusive);

  return g_test_run ();
}
