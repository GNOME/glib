// SPDX-License-Identifier: LGPL-2.1-or-later
// SPDX-FileCopyrightText: 2021  Emmanuele Bassi

#include <glib-object.h>

#define TEST_TYPE_FINAL (test_final_get_type())
G_DECLARE_FINAL_TYPE (TestFinal, test_final, TEST, FINAL, GObject)

struct _TestFinal
{
  GObject parent_instance;
};

struct _TestFinalClass
{
  GObjectClass parent_class;
};

G_DEFINE_FINAL_TYPE (TestFinal, test_final, G_TYPE_OBJECT)

static void
test_final_class_init (TestFinalClass *klass)
{
}

static void
test_final_init (TestFinal *self)
{
}

#define TEST_TYPE_FINAL2 (test_final2_get_type())
G_DECLARE_FINAL_TYPE (TestFinal2, test_final2, TEST, FINAL2, TestFinal)

struct _TestFinal2
{
  TestFinal parent_instance;
};

struct _TestFinal2Class
{
  TestFinalClass parent_class;
};

G_DEFINE_TYPE (TestFinal2, test_final2, TEST_TYPE_FINAL)

static void
test_final2_class_init (TestFinal2Class *klass)
{
}

static void
test_final2_init (TestFinal2 *self)
{
}

/* test_type_flags_final: Check that trying to derive from a final class
 * will result in a warning from the type system
 */
static void
test_type_flags_final (void)
{
  GType final2_type;

  /* This is the message we print out when registering the type */
  g_test_expect_message ("GLib-GObject", G_LOG_LEVEL_WARNING,
                         "*cannot derive*");

  /* This is the message when we fail to return from the GOnce init
   * block within the test_final2_get_type() function
   */
  g_test_expect_message ("GLib", G_LOG_LEVEL_CRITICAL,
                         "*g_once_init_leave: assertion*");

  final2_type = TEST_TYPE_FINAL2;
  g_assert_true (final2_type == G_TYPE_INVALID);

  g_test_assert_expected_messages ();
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/type/flags/final", test_type_flags_final);

  return g_test_run ();
}
