/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 2001 Red Hat, Inc.
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
#define	G_LOG_DOMAIN "TestObject"
#include	<glib-object.h>

#define TEST_TYPE_OBJECT            (test_object_get_type ())
#define TEST_OBJECT(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), TEST_TYPE_OBJECT, TestObject))
#define TEST_OBJECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TEST_TYPE_OBJECT, TestObjectClass))
#define TEST_IS_OBJECT(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), TEST_TYPE_OBJECT))
#define TEST_IS_OBJECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TEST_TYPE_OBJECT))
#define TEST_OBJECT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TEST_TYPE_OBJECT, TestObjectClass))

typedef struct _TestIface TestIface;
typedef struct
{
  GTypeInterface base_iface;
} TestIfaceClass;
typedef struct
{
  GObject parent_instance;
} TestObject;

#define TEST_TYPE_IFACE           (test_iface_get_type ())
#define TEST_IFACE(obj)		  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TEST_TYPE_IFACE, TestIface))
#define TEST_IS_IFACE(obj)	  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TEST_TYPE_IFACE))
#define TEST_IFACE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), TEST_TYPE_IFACE, TestIfaceClass))

GType
test_iface_get_type (void)
{
  static GType test_iface_type = 0;

  if (!test_iface_type)
    {
      static const GTypeInfo test_iface_info =
      {
	sizeof (TestIfaceClass),
	NULL,           /* base_init */
	NULL,           /* base_finalize */
      };

      test_iface_type = g_type_register_static (G_TYPE_INTERFACE, "TestIface", &test_iface_info, 0);
      g_type_interface_add_prerequisite (test_iface_type, G_TYPE_OBJECT);
    }

  return test_iface_type;
}

typedef struct
{
  GObjectClass parent_class;

  gchar* (*test_signal) (TestObject *tobject,
			 TestIface  *iface_object,
			 gpointer    tdata);
} TestObjectClass;

static void
test_object_init (TestObject *tobject)
{
}

static gboolean
test_signal_accumulator (GSignalInvocationHint *ihint,
			 GValue                *return_accu,
			 const GValue          *handler_return,
			 gpointer               data)
{
  gchar *accu_string = g_value_get_string (return_accu);
  gchar *new_string = g_value_get_string (handler_return);
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

static gchar*
test_object_test_signal (TestObject *tobject,
			 TestIface  *iface_object,
			 gpointer    tdata)
{
  g_message ("::test_signal default_handler called");

  g_return_val_if_fail (TEST_IS_IFACE (iface_object), NULL);
  
  return g_strdup ("<default_handler>");
}

static void
test_object_class_init (TestObjectClass *class)
{
  /*  GObjectClass *gobject_class = G_OBJECT_CLASS (class); */

  class->test_signal = test_object_test_signal;

  g_signal_newc ("test-signal",
		 G_OBJECT_CLASS_TYPE (class),
		 G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST | G_SIGNAL_RUN_CLEANUP,
		 G_STRUCT_OFFSET (TestObjectClass, test_signal),
		 test_signal_accumulator, NULL,
		 g_cclosure_marshal_STRING__OBJECT_POINTER,
		 G_TYPE_STRING, 2, TEST_TYPE_IFACE, G_TYPE_POINTER);
}

GType
test_object_get_type (void)
{
  static GType test_object_type = 0;

  if (!test_object_type)
    {
      static const GTypeInfo test_object_info =
      {
	sizeof (TestObjectClass),
	NULL,           /* base_init */
	NULL,           /* base_finalize */
	(GClassInitFunc) test_object_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data */
	sizeof (TestObject),
	5,              /* n_preallocs */
	(GInstanceInitFunc) test_object_init,
      };
      GInterfaceInfo iface_info = { NULL, NULL, NULL };

      test_object_type = g_type_register_static (G_TYPE_OBJECT, "TestObject", &test_object_info, 0);
      g_type_add_interface_static (test_object_type, TEST_TYPE_IFACE, &iface_info);
    }

  return test_object_type;
}



int
main (int   argc,
      char *argv[])
{
  TestObject *tobject, *sigarg;
  gchar *string = NULL;

  g_log_set_always_fatal (g_log_set_always_fatal (G_LOG_FATAL_MASK) |
			  G_LOG_LEVEL_WARNING |
			  G_LOG_LEVEL_CRITICAL);
  g_type_init (G_TYPE_DEBUG_OBJECTS | G_TYPE_DEBUG_SIGNALS);

  tobject = g_object_new (TEST_TYPE_OBJECT, NULL);
  sigarg = g_object_new (TEST_TYPE_OBJECT, NULL);
  g_signal_emit_by_name (tobject, "test-signal", sigarg, NULL, &string);
  g_message ("signal return: \"%s\"", string);
  g_free (string);
  
  g_object_unref (sigarg);
  g_object_unref (tobject);

  g_message ("%s done", argv[0]);

  return 0;
}
