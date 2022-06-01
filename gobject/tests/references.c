/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 2005 Red Hat, Inc.
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
 */

#include <glib-object.h>

/* This test tests weak and toggle references */

static GObject *global_object;

static gboolean object_destroyed;
static gboolean weak_ref1_notified;
static gboolean weak_ref2_notified;
static gboolean toggle_ref1_weakened;
static gboolean toggle_ref1_strengthened;
static gboolean toggle_ref2_weakened;
static gboolean toggle_ref2_strengthened;
static gboolean toggle_ref3_weakened;
static gboolean toggle_ref3_strengthened;

/* TestObject, a parent class for TestObject */
static GType test_object_get_type (void);
#define TEST_TYPE_OBJECT          (test_object_get_type ())
typedef struct _TestObject        TestObject;
typedef struct _TestObjectClass   TestObjectClass;

struct _TestObject
{
  GObject parent_instance;
};
struct _TestObjectClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE (TestObject, test_object, G_TYPE_OBJECT)

static void
test_object_finalize (GObject *object)
{
  object_destroyed = TRUE;

  G_OBJECT_CLASS (test_object_parent_class)->finalize (object);
}

static void
test_object_class_init (TestObjectClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = test_object_finalize;
}

static void
test_object_init (TestObject *test_object)
{
}

static void
clear_flags (void)
{
  object_destroyed = FALSE;
  weak_ref1_notified = FALSE;
  weak_ref2_notified = FALSE;
  toggle_ref1_weakened = FALSE;
  toggle_ref1_strengthened = FALSE;
  toggle_ref2_weakened = FALSE;
  toggle_ref2_strengthened = FALSE;
  toggle_ref3_weakened = FALSE;
  toggle_ref3_strengthened = FALSE;
}

static void
weak_ref1 (gpointer data,
           GObject *object)
{
  g_assert_true (object == global_object);
  g_assert_cmpint (GPOINTER_TO_INT (data), ==, 42);

  weak_ref1_notified = TRUE;
}

static void
weak_ref2 (gpointer data,
           GObject *object)
{
  g_assert_true (object == global_object);
  g_assert_cmpint (GPOINTER_TO_INT (data), ==, 24);

  weak_ref2_notified = TRUE;
}

static void
toggle_ref1 (gpointer data,
             GObject *object,
             gboolean is_last_ref)
{
  g_assert_true (object == global_object);
  g_assert_cmpint (GPOINTER_TO_INT (data), ==, 42);

  if (is_last_ref)
    toggle_ref1_weakened = TRUE;
  else
    toggle_ref1_strengthened = TRUE;
}

static void
toggle_ref2 (gpointer data,
             GObject *object,
             gboolean is_last_ref)
{
  g_assert_true (object == global_object);
  g_assert_cmpint (GPOINTER_TO_INT (data), ==, 24);

  if (is_last_ref)
    toggle_ref2_weakened = TRUE;
  else
    toggle_ref2_strengthened = TRUE;
}

static void
toggle_ref3 (gpointer data,
             GObject *object,
             gboolean is_last_ref)
{
  g_assert_true (object == global_object);
  g_assert_cmpint (GPOINTER_TO_INT (data), ==, 34);

  if (is_last_ref)
    {
      toggle_ref3_weakened = TRUE;
      g_object_remove_toggle_ref (object, toggle_ref3, GUINT_TO_POINTER (34));
    }
  else
    toggle_ref3_strengthened = TRUE;
}

static void
test_references (void)
{
  GObject *object;

  /* Test basic weak reference operation */
  global_object = object = g_object_new (TEST_TYPE_OBJECT, NULL);

  g_object_weak_ref (object, weak_ref1, GUINT_TO_POINTER (42));

  clear_flags ();
  g_object_unref (object);
  g_assert_true (weak_ref1_notified);
  g_assert_true (object_destroyed);

  /* Test two weak references at once
   */
  global_object = object = g_object_new (TEST_TYPE_OBJECT, NULL);

  g_object_weak_ref (object, weak_ref1, GUINT_TO_POINTER (42));
  g_object_weak_ref (object, weak_ref2, GUINT_TO_POINTER (24));

  clear_flags ();
  g_object_unref (object);
  g_assert_true (weak_ref1_notified);
  g_assert_true (weak_ref2_notified);
  g_assert_true (object_destroyed);

  /* Test remove weak references */
  global_object = object = g_object_new (TEST_TYPE_OBJECT, NULL);

  g_object_weak_ref (object, weak_ref1, GUINT_TO_POINTER (42));
  g_object_weak_ref (object, weak_ref2, GUINT_TO_POINTER (24));
  g_object_weak_unref (object, weak_ref1, GUINT_TO_POINTER (42));

  clear_flags ();
  g_object_unref (object);
  g_assert_false (weak_ref1_notified);
  g_assert_true (weak_ref2_notified);
  g_assert_true (object_destroyed);

  /* Test basic toggle reference operation */
  global_object = object = g_object_new (TEST_TYPE_OBJECT, NULL);

  g_object_add_toggle_ref (object, toggle_ref1, GUINT_TO_POINTER (42));

  clear_flags ();
  g_object_unref (object);
  g_assert_true (toggle_ref1_weakened);
  g_assert_false (toggle_ref1_strengthened);
  g_assert_false (object_destroyed);

  clear_flags ();
  g_object_ref (object);
  g_assert_false (toggle_ref1_weakened);
  g_assert_true (toggle_ref1_strengthened);
  g_assert_false (object_destroyed);

  g_object_unref (object);

  clear_flags ();
  g_object_remove_toggle_ref (object, toggle_ref1, GUINT_TO_POINTER (42));
  g_assert_false (toggle_ref1_weakened);
  g_assert_false (toggle_ref1_strengthened);
  g_assert_true (object_destroyed);

  global_object = object = g_object_new (TEST_TYPE_OBJECT, NULL);

  /* Test two toggle references at once */
  g_object_add_toggle_ref (object, toggle_ref1, GUINT_TO_POINTER (42));
  g_object_add_toggle_ref (object, toggle_ref2, GUINT_TO_POINTER (24));

  clear_flags ();
  g_object_unref (object);
  g_assert_false (toggle_ref1_weakened);
  g_assert_false (toggle_ref1_strengthened);
  g_assert_false (toggle_ref2_weakened);
  g_assert_false (toggle_ref2_strengthened);
  g_assert_false (object_destroyed);

  clear_flags ();
  g_object_remove_toggle_ref (object, toggle_ref1, GUINT_TO_POINTER (42));
  g_assert_false (toggle_ref1_weakened);
  g_assert_false (toggle_ref1_strengthened);
  g_assert_true (toggle_ref2_weakened);
  g_assert_false (toggle_ref2_strengthened);
  g_assert_false (object_destroyed);

  clear_flags ();
  /* Check that removing a toggle ref with %NULL data works fine. */
  g_object_remove_toggle_ref (object, toggle_ref2, NULL);
  g_assert_false (toggle_ref1_weakened);
  g_assert_false (toggle_ref1_strengthened);
  g_assert_false (toggle_ref2_weakened);
  g_assert_false (toggle_ref2_strengthened);
  g_assert_true (object_destroyed);

  /* Test a toggle reference that removes itself */
  global_object = object = g_object_new (TEST_TYPE_OBJECT, NULL);

  g_object_add_toggle_ref (object, toggle_ref3, GUINT_TO_POINTER (34));

  clear_flags ();
  g_object_unref (object);
  g_assert_true (toggle_ref3_weakened);
  g_assert_false (toggle_ref3_strengthened);
  g_assert_true (object_destroyed);
}

int
main (int   argc,
      char *argv[])
{
  g_log_set_always_fatal (g_log_set_always_fatal (G_LOG_FATAL_MASK) |
                          G_LOG_LEVEL_WARNING |
                          G_LOG_LEVEL_CRITICAL);

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gobject/references", test_references);

  return g_test_run ();
}
