/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 2001, 2003 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#undef	G_LOG_DOMAIN
#define	G_LOG_DOMAIN "TestAccumulator"

#undef G_DISABLE_ASSERT
#undef G_DISABLE_CHECKS
#undef G_DISABLE_CAST_CHECKS

#include <string.h>

#include	<glib-object.h>

#include "testmarshal.h"
#include "testcommon.h"

/* What this test tests is the behavior of signal accumulators
 * Two accumulators are tested:
 *
 * 1: A custom accumulator that appends the returned strings
 * 2: The standard g_signal_accumulator_true_handled that stops
 *    emission on TRUE returns.
 */

/*
 * TestObject, a parent class for TestObject
 */
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

  gchar*   (*test_signal1) (TestObject *tobject,
			    gint        param);
  gboolean (*test_signal2) (TestObject *tobject,
			    gint        param);
};

static GType test_object_get_type (void);

static gboolean
test_signal1_accumulator (GSignalInvocationHint *ihint,
			  GValue                *return_accu,
			  const GValue          *handler_return,
			  gpointer               data)
{
  const gchar *accu_string = g_value_get_string (return_accu);
  const gchar *new_string = g_value_get_string (handler_return);
  gchar *result_string;

  if (accu_string)
    result_string = g_strconcat (accu_string, new_string, NULL);
  else if (new_string)
    result_string = g_strdup (new_string);
  else
    result_string = NULL;

  g_value_set_string_take_ownership (return_accu, result_string);

  return TRUE;
}

gchar*
test_object_signal1_callback_before (TestObject *tobject,
				     gint        param,
				     gpointer    data)
{
  return g_strdup ("<before>");
}

gchar*
test_object_real_signal1 (TestObject *tobject,
			  gint        param)
{
  return g_strdup ("<default>");
}

gchar*
test_object_signal1_callback_after (TestObject *tobject,
				    gint        param,
				    gpointer    data)
{
  return g_strdup ("<after>");
}

gboolean
test_object_signal2_callback_before (TestObject *tobject,
				     gint        param)
{
  switch (param)
    {
    case 1: return TRUE;
    case 2: return FALSE;
    case 3: return FALSE;
    case 4: return FALSE;
    }

  g_assert_not_reached ();
  return FALSE;
}

gboolean
test_object_real_signal2 (TestObject *tobject,
			  gint        param)
{
  switch (param)
    {
    case 1: g_assert_not_reached (); return FALSE;
    case 2: return TRUE;
    case 3: return FALSE;
    case 4: return FALSE;
    }
  
  g_assert_not_reached ();
  return FALSE;
}

gboolean
test_object_signal2_callback_after (TestObject *tobject,
				     gint        param)
{
  switch (param)
    {
    case 1: g_assert_not_reached (); return FALSE;
    case 2: g_assert_not_reached (); return FALSE;
    case 3: return TRUE;
    case 4: return FALSE;
    }
      
  g_assert_not_reached ();
  return FALSE;
}

static void
test_object_class_init (TestObjectClass *class)
{
  class->test_signal1 = test_object_real_signal1;
  class->test_signal2 = test_object_real_signal2;
  
  g_signal_new ("test-signal1",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (TestObjectClass, test_signal1),
		test_signal1_accumulator, NULL,
		test_marshal_STRING__INT,
		G_TYPE_STRING, 1, G_TYPE_INT);
  g_signal_new ("test-signal2",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (TestObjectClass, test_signal2),
		g_signal_accumulator_true_handled, NULL,
		test_marshal_BOOLEAN__INT,
		G_TYPE_BOOLEAN, 1, G_TYPE_INT);
}

static DEFINE_TYPE(TestObject, test_object,
		   test_object_class_init, NULL, NULL,
		   G_TYPE_OBJECT)

int
main (int   argc,
      char *argv[])
{
  TestObject *object;
  gchar *string_result;
  gboolean bool_result;
	
  g_log_set_always_fatal (g_log_set_always_fatal (G_LOG_FATAL_MASK) |
			  G_LOG_LEVEL_WARNING |
			  G_LOG_LEVEL_CRITICAL);
  g_type_init ();

  object = g_object_new (TEST_TYPE_OBJECT, NULL);

  g_signal_connect (object, "test-signal1",
		    G_CALLBACK (test_object_signal1_callback_before), NULL);
  g_signal_connect_after (object, "test-signal1",
			  G_CALLBACK (test_object_signal1_callback_after), NULL);
  
  g_signal_emit_by_name (object, "test-signal1", 0, &string_result);
  g_assert (strcmp (string_result, "<before><default><after>") == 0);
  g_free (string_result);

  g_signal_connect (object, "test-signal2",
		    G_CALLBACK (test_object_signal2_callback_before), NULL);
  g_signal_connect_after (object, "test-signal2",
			  G_CALLBACK (test_object_signal2_callback_after), NULL);
  
  bool_result = FALSE;
  g_signal_emit_by_name (object, "test-signal2", 1, &bool_result);
  g_assert (bool_result == TRUE);
  bool_result = FALSE;
  g_signal_emit_by_name (object, "test-signal2", 2, &bool_result);
  g_assert (bool_result == TRUE);
  bool_result = FALSE;
  g_signal_emit_by_name (object, "test-signal2", 3, &bool_result);
  g_assert (bool_result == TRUE);
  bool_result = TRUE;
  g_signal_emit_by_name (object, "test-signal2", 4, &bool_result);
  g_assert (bool_result == FALSE);

  return 0;
}
