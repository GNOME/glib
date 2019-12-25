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
  int init_called;
  int copy_called;
  int free_called;
} TestErrorCheck;

static TestErrorCheck *init_check;

GQuark test_error_quark (void);
#define TEST_ERROR (test_error_quark ())

typedef struct
{
  int foo;
  TestErrorCheck *check;
} TestErrorPrivate;

static void
test_error_private_init (TestErrorPrivate *priv)
{
  priv->foo = 13;
  /* If that triggers, it's test bug.
   */
  g_assert_nonnull (init_check);
  /* Using global init_check, because error->check is still nil at
   * this point.
   */
  init_check->init_called++;
}

static void
test_error_private_copy (const TestErrorPrivate *src_priv,
                         TestErrorPrivate       *dest_priv)
{
  dest_priv->foo = src_priv->foo;
  dest_priv->check = src_priv->check;

  dest_priv->check->copy_called++;
}

static void
test_error_private_clear (TestErrorPrivate *priv)
{
  priv->check->free_called++;
}

G_DEFINE_EXTENDED_ERROR (TestError, test_error)

static TestErrorPrivate *
fill_test_error (GError *error, TestErrorCheck *check)
{
  TestErrorPrivate *test_error = test_error_get_private (error);

  test_error->check = check;

  return test_error;
}

static void
test_extended (void)
{
  TestErrorCheck check = { 0, 0, 0 };
  GError *error;
  TestErrorPrivate *test_priv;
  GError *copy_error;
  TestErrorPrivate *copy_test_priv;

  init_check = &check;
  error = g_error_new_literal (TEST_ERROR, 0, "foo");
  test_priv = fill_test_error (error, &check);

  g_assert_cmpint (check.init_called, ==, 1);
  g_assert_cmpint (check.copy_called, ==, 0);
  g_assert_cmpint (check.free_called, ==, 0);

  g_assert_cmpuint (error->domain, ==, TEST_ERROR);
  g_assert_cmpint (test_priv->foo, ==, 13);

  copy_error = g_error_copy (error);
  g_assert_cmpint (check.init_called, ==, 2);
  g_assert_cmpint (check.copy_called, ==, 1);
  g_assert_cmpint (check.free_called, ==, 0);

  g_assert_cmpuint (error->domain, ==, copy_error->domain);
  g_assert_cmpint (error->code, ==, copy_error->code);
  g_assert_cmpstr (error->message, ==, copy_error->message);

  copy_test_priv = test_error_get_private (copy_error);
  g_assert_cmpint (test_priv->foo, ==, copy_test_priv->foo);

  g_error_free (error);
  g_error_free (copy_error);

  g_assert_cmpint (check.init_called, ==, 2);
  g_assert_cmpint (check.copy_called, ==, 1);
  g_assert_cmpint (check.free_called, ==, 2);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/error/overwrite", test_overwrite);
  g_test_add_func ("/error/prefix", test_prefix);
  g_test_add_func ("/error/literal", test_literal);
  g_test_add_func ("/error/copy", test_copy);
  g_test_add_func ("/error/extended", test_extended);

  return g_test_run ();
}
