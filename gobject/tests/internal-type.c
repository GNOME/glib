/* internal-type.c: Test internal type
 *
 * SPDX-FileCopyrightText: 2024 Bilal Elmoussaoui
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <glib-object.h>

struct _RandomClass {
  GObjectClass parent_class;
  gint some_value;
};

struct _Random {
  GObject parent;
};

G_DECLARE_INTERNAL_TYPE (Random, random, G, RANDOM, GObject)

G_DEFINE_FINAL_TYPE (Random, random, G_TYPE_OBJECT)

static void
random_class_init (RandomClass *klass)
{
  klass->some_value = 3;
}

static void
random_init (Random *self)
{
}

static void
test_internal_type (void)
{
  GObject *object;
  RandomClass *klass;

  object = g_object_new (random_get_type (), NULL);
  klass = G_RANDOM_GET_CLASS (object);
  
  g_assert_cmpint (klass->some_value, ==, 3);
  g_assert_true (G_IS_RANDOM (object));
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/type/internal-type", test_internal_type);
  return g_test_run ();
}
