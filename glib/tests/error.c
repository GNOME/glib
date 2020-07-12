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
  int copy_called;
  int free_called;
} TestErrorCheck;

typedef struct
{
  GError error;

  int foo;
  TestErrorCheck *check;
} TestErrorInstance;

static void
test_error_copy (const GError *src_error, GError *dest_error)
{
  const TestErrorInstance *src_test_error = (const TestErrorInstance *) src_error;
  TestErrorInstance *dest_test_error = (TestErrorInstance *) dest_error;

  dest_test_error->foo = src_test_error->foo;
  dest_test_error->check = src_test_error->check;

  dest_test_error->check->copy_called++;
}

static void
test_error_cleanup (GError *error)
{
  TestErrorInstance *test_error = (TestErrorInstance *) error;

  test_error->check->free_called++;
}

static TestErrorInstance *
fill_test_error (GError *error, TestErrorCheck *check)
{
  TestErrorInstance *test_error = (TestErrorInstance *) error;

  test_error->foo = 42;
  test_error->check = check;

  return test_error;
}

GQuark test_error_quark (void);
#define TEST_ERROR_DOMAIN (test_error_quark ())
G_DEFINE_ERROR_TYPE (TestError, test_error)

static TestErrorInstance *
test_error_new (TestErrorCheck *check)
{
  GError *error = g_error_new_literal (TEST_ERROR_DOMAIN, 0, "foo");

  return fill_test_error (error, check);
}

static void
test_extended (void)
{
  TestErrorCheck check = { 0, 0 };
  TestErrorInstance *error = test_error_new (&check);
  GError *copy;
  TestErrorInstance *copy_test_error;

  g_assert_cmpint (check.copy_called, ==, 0);
  g_assert_cmpint (check.free_called, ==, 0);

  g_assert_cmpuint (error->error.domain, ==, TEST_ERROR_DOMAIN);

  copy = g_error_copy ((GError *) error);
  g_assert_cmpint (check.copy_called, ==, 1);

  g_assert_cmpuint (error->error.domain, ==, copy->domain);
  g_assert_cmpint (error->error.code, ==, copy->code);
  g_assert_cmpstr (error->error.message, ==, copy->message);

  copy_test_error = (TestErrorInstance *) copy;
  g_assert_cmpint (error->foo, ==, copy_test_error->foo);

  g_error_free ((GError *) error);
  g_error_free (copy);

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
