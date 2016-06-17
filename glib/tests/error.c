#include <glib.h>

static void
test_overwrite (void)
{
  GError *error, *dest, *src;

  if (!g_test_undefined ())
    return;

  error = g_error_new_literal (G_MARKUP_ERROR, G_MARKUP_ERROR_EMPTY, "bla");

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                         "*set over the top*");
  g_set_error_literal (&error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE, "bla");
  g_test_assert_expected_messages ();

  g_assert_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_EMPTY);
  g_error_free (error);


  dest = g_error_new_literal (G_MARKUP_ERROR, G_MARKUP_ERROR_EMPTY, "bla");
  src = g_error_new_literal (G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE, "bla");

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                         "*set over the top*");
  g_propagate_error (&dest, src);
  g_test_assert_expected_messages ();

  g_assert_error (dest, G_MARKUP_ERROR, G_MARKUP_ERROR_EMPTY);
  g_error_free (dest);
}

static void
test_prefix (void)
{
  GError *error;
  GError *dest, *src;

  error = NULL;
  g_prefix_error (&error, "foo %d %s: ", 1, "two");
  g_assert (error == NULL);

  error = g_error_new_literal (G_MARKUP_ERROR, G_MARKUP_ERROR_EMPTY, "bla");
  g_prefix_error (&error, "foo %d %s: ", 1, "two");
  g_assert_cmpstr (error->message, ==, "foo 1 two: bla");
  g_error_free (error);

  dest = NULL;
  src = g_error_new_literal (G_MARKUP_ERROR, G_MARKUP_ERROR_EMPTY, "bla");
  g_propagate_prefixed_error (&dest, src, "foo %d %s: ", 1, "two");
  g_assert_cmpstr (dest->message, ==, "foo 1 two: bla");
  g_error_free (dest);
}

static void
test_literal (void)
{
  GError *error;

  error = NULL;
  g_set_error_literal (&error, G_MARKUP_ERROR, G_MARKUP_ERROR_EMPTY, "%s %d %x");
  g_assert_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_EMPTY);
  g_assert_cmpstr (error->message, ==, "%s %d %x");
  g_error_free (error);
}

static void
test_copy (void)
{
  GError *error;
  GError *copy;

  error = NULL;
  g_set_error_literal (&error, G_MARKUP_ERROR, G_MARKUP_ERROR_EMPTY, "%s %d %x");
  copy = g_error_copy (error);

  g_assert_error (copy, G_MARKUP_ERROR, G_MARKUP_ERROR_EMPTY);
  g_assert_cmpstr (copy->message, ==, "%s %d %x");

  g_error_free (error);
  g_error_free (copy);
}

typedef struct
{
  guint copy_calls;
  guint free_calls;
} ExtraDataStats;

static gpointer
extra_data_stats_fake_copy (gpointer data)
{
  ExtraDataStats *stats = data;

  stats->copy_calls++;

  return stats;
}

static void
extra_data_stats_fake_free (gpointer data)
{
  ExtraDataStats *stats = data;

  stats->free_calls++;
}

static void
test_extra_data (void)
{
  GError *error;
  GError *copy;
  ExtraDataStats stats = { 0, 0 };
  gboolean exists;
  const gchar** keys;

  error = g_error_new_literal (G_MARKUP_ERROR, G_MARKUP_ERROR_EMPTY, "foo");
  copy = NULL;

  /* check if we get an empty strings array when there are no extra
   * data
   */
  keys = g_error_get_extra_data_keys (error);
  g_assert_nonnull (keys);
  g_assert_cmpuint (g_strv_length ((gchar**)keys), ==, 0);
  g_free (g_steal_pointer (&keys));

  /* check if extra data is stolen, so no copies happen */
  g_add_error_extra_data (&error, "test", &stats, extra_data_stats_fake_copy, extra_data_stats_fake_free);
  g_assert_cmpuint (stats.copy_calls, ==, 0);
  g_assert_cmpuint (stats.free_calls, ==, 0);

  /* check if extra data is freed when no error is passed */
  g_add_error_extra_data (NULL, "test", &stats, extra_data_stats_fake_copy, extra_data_stats_fake_free);
  g_assert_cmpuint (stats.copy_calls, ==, 0);
  g_assert_cmpuint (stats.free_calls, ==, 1);

  /* check if extra data is freed when being replaced */
  g_add_error_extra_data (&error, "test", &stats, extra_data_stats_fake_copy, extra_data_stats_fake_free);
  g_assert_cmpuint (stats.copy_calls, ==, 0);
  g_assert_cmpuint (stats.free_calls, ==, 2);

  /* add another extra data */
  g_add_error_extra_data (&error, "test2", &stats, extra_data_stats_fake_copy, extra_data_stats_fake_free);
  g_assert_cmpuint (stats.copy_calls, ==, 0);
  g_assert_cmpuint (stats.free_calls, ==, 2);

  /* check if copy func was called twice, once for "test" and once for
   * "test2"
   */
  copy = g_error_copy (error);
  g_assert_cmpuint (stats.copy_calls, ==, 2);
  g_assert_cmpuint (stats.free_calls, ==, 2);

  g_assert_nonnull (g_error_get_extra_data (copy, "test", &exists));
  g_assert_true (exists);
  g_assert_null (g_error_get_extra_data (copy, "foo", &exists));
  g_assert_false (exists);
  /* also possible to pass NULL for exists parameter */
  g_assert_null (g_error_get_extra_data (copy, "bar", NULL));
  g_assert_nonnull (g_error_get_extra_data (copy, "test", NULL));
  /* ensure no copying happened in extra data getters */
  g_assert_cmpuint (stats.copy_calls, ==, 2);
  g_assert_cmpuint (stats.free_calls, ==, 2);

  /* check if we get the keys we expect (test and test2) */
  keys = g_error_get_extra_data_keys (copy);
  g_assert_nonnull (keys);
  g_assert_cmpuint (g_strv_length ((gchar**)keys), ==, 2);
  g_assert_true (g_strv_contains (keys, "test"));
  g_assert_true (g_strv_contains (keys, "test2"));
  g_free (g_steal_pointer (&keys));
  /* ensure no copying happened in extra data keys getter */
  g_assert_cmpuint (stats.copy_calls, ==, 2);
  g_assert_cmpuint (stats.free_calls, ==, 2);

  /* check if extra data is freed when error is freed */
  g_error_free (g_steal_pointer (&error));
  g_assert_cmpuint (stats.copy_calls, ==, 2);
  g_assert_cmpuint (stats.free_calls, ==, 4);

  /* check if extra data is freed when error is freed */
  g_clear_error (&copy);
  g_assert_cmpuint (stats.copy_calls, ==, 2);
  g_assert_cmpuint (stats.free_calls, ==, 6);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/error/overwrite", test_overwrite);
  g_test_add_func ("/error/prefix", test_prefix);
  g_test_add_func ("/error/literal", test_literal);
  g_test_add_func ("/error/copy", test_copy);
  g_test_add_func ("/error/extra-data", test_extra_data);

  return g_test_run ();
}

