/* -*- mode: C; c-basic-offset: 4 -*- */
#include <glib.h>
#include <glib-object.h>

static guint foo_signal_id = 0;
static guint bar_signal_id = 0;


static GType test_i_get_type (void);
static GType test_a_get_type (void);
static GType test_b_get_type (void);
static GType test_c_get_type (void);

#define TEST_TYPE_I (test_i_get_type ())
#define TEST_I(object) (G_TYPE_CHECK_INSTANCE_CAST((object), TEST_TYPE_I, TestI))
#define TEST_I_CLASS(vtable) (G_TYPE_CHECK_CLASS_CAST((vtable), TEST_TYPE_I, TestIClass))
#define TEST_IS_I(object) (G_TYPE_CHECK_INSTANCE_TYPE((object), TEST_TYPE_I))
#define TEST_IS_I_CLASS(vtable) (G_TYPE_CHECK_CLASS_TYPE((vtable), TEST_TYPE_I))
#define TEST_I_GET_CLASS(object) (G_TYPE_INSTANCE_GET_INTERFACE((object), TEST_TYPE_I, TestIClass))

typedef struct _TestI TestI;
typedef struct _TestIClass TestIClass;

struct _TestIClass {
    GTypeInterface base_iface;
};

static void
test_i_foo (TestI *self)
{
    g_print("TestI::foo called.\n");
}


static void
test_i_base_init (gpointer g_class)
{
    static gboolean initialised = FALSE;

    if (!initialised) {
	foo_signal_id = g_signal_newv("foo",
				      TEST_TYPE_I,
				      G_SIGNAL_RUN_LAST,
				      g_cclosure_new(G_CALLBACK(test_i_foo),
						     NULL, NULL),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__VOID,
				      G_TYPE_NONE, 0, NULL);
    }
    initialised = TRUE;
}

static GType
test_i_get_type (void)
{
    static GType type = 0;
  
    if (!type) {
	static const GTypeInfo type_info = {
	    sizeof (TestIClass),
	    (GBaseInitFunc) test_i_base_init, /* base_init */
	    NULL,             /* base_finalize */
	};
      
	type = g_type_register_static (G_TYPE_INTERFACE, "TestI",
				       &type_info, 0);
    }
  
    return type;
}



#define TEST_TYPE_A (test_a_get_type())
#define TEST_A(object) (G_TYPE_CHECK_INSTANCE_CAST((object), TEST_TYPE_A, TestA))
#define TEST_A_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), TEST_TYPE_A, TestAClass))
#define TEST_IS_A(object) (G_TYPE_CHECK_INSTANCE_TYPE((object), TEST_TYPE_A))
#define TEST_IS_A_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), TEST_TYPE_A))
#define TEST_A_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), TEST_TYPE_A, TestAClass))

typedef struct _TestA TestA;
typedef struct _TestAClass TestAClass;

struct _TestA {
    GObject parent;
};
struct _TestAClass {
    GObjectClass parent_class;

    void (* bar) (TestA *self);
};

static void
test_a_foo (TestI *self)
{
    GValue args[1] = { { 0, } };

    g_print("TestA::foo called.  Chaining up.\n");

    g_value_init (&args[0], TEST_TYPE_A);
    g_value_set_object (&args[0], self);

    g_assert (g_signal_get_invocation_hint (self)->signal_id == foo_signal_id);
    g_signal_chain_from_overridden (args, NULL);

    g_value_unset (&args[0]);
}

static void
test_a_bar (TestA *self)
{
    g_print("TestA::bar called.\n");
}

static void
test_a_class_init (TestAClass *class)
{
    class->bar = test_a_bar;

    bar_signal_id = g_signal_new("bar",
				 TEST_TYPE_A,
				 G_SIGNAL_RUN_LAST,
				 G_STRUCT_OFFSET (TestAClass, bar),
				 NULL, NULL,
				 g_cclosure_marshal_VOID__VOID,
				 G_TYPE_NONE, 0);
}

static void
test_a_interface_init (TestIClass *iface)
{
    g_signal_override_class_closure (foo_signal_id,
				     TEST_TYPE_A,
				     g_cclosure_new (G_CALLBACK (test_a_foo),
						     NULL, NULL));
}

static GType
test_a_get_type (void)
{
    static GType type = 0;
  
    if (!type) {
	static const GTypeInfo type_info = {
            sizeof(TestAClass),
            (GBaseInitFunc) NULL,
            (GBaseFinalizeFunc) NULL,
            (GClassInitFunc) test_a_class_init,
            NULL, /* class_finalize */
            NULL, /* class_data */
            sizeof(TestAClass),
            0, /* n_preallocs */
            (GInstanceInitFunc) NULL,
	};
	static const GInterfaceInfo interface_info = {
	    (GInterfaceInitFunc) test_a_interface_init,
	    NULL,
	    NULL
	};

	type = g_type_register_static (G_TYPE_OBJECT, "TestA",
				       &type_info, 0);
	g_type_add_interface_static (type, TEST_TYPE_I, &interface_info);
    }
  
  return type;
}


#define TEST_TYPE_B (test_b_get_type())
#define TEST_B(object) (G_TYPE_CHECK_INSTANCE_CAST((object), TEST_TYPE_B, TestB))
#define TEST_B_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), TEST_TYPE_B, TestBClass))
#define TEST_IS_B(object) (G_TYPE_CHECK_INSTANCE_TYPE((object), TEST_TYPE_B))
#define TEST_IS_B_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), TEST_TYPE_B))
#define TEST_B_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), TEST_TYPE_B, TestBClass))

typedef struct _TestB TestB;
typedef struct _TestBClass TestBClass;

struct _TestB {
    TestA parent;
};
struct _TestBClass {
    TestAClass parent_class;
};

static void
test_b_foo (TestA *self)
{
    GValue args[1] = { { 0, } };

    g_print("TestB::foo called.  Chaining up.\n");

    g_value_init (&args[0], TEST_TYPE_A);
    g_value_set_object (&args[0], self);

    g_assert (g_signal_get_invocation_hint (self)->signal_id == foo_signal_id);
    g_signal_chain_from_overridden (args, NULL);

    g_value_unset (&args[0]);
}

static void
test_b_bar (TestI *self)
{
    GValue args[1] = { { 0, } };

    g_print("TestB::bar called.  Chaining up.\n");

    g_value_init (&args[0], TEST_TYPE_A);
    g_value_set_object (&args[0], self);

    g_assert (g_signal_get_invocation_hint (self)->signal_id == bar_signal_id);
    g_signal_chain_from_overridden (args, NULL);

    g_value_unset (&args[0]);
}

static void
test_b_class_init (TestBClass *class)
{
    g_signal_override_class_closure (foo_signal_id,
				     TEST_TYPE_B,
				     g_cclosure_new (G_CALLBACK (test_b_foo),
						     NULL, NULL));
    g_signal_override_class_closure (bar_signal_id,
				     TEST_TYPE_B,
				     g_cclosure_new (G_CALLBACK (test_b_bar),
						     NULL, NULL));
}

static GType
test_b_get_type (void)
{
    static GType type = 0;
  
    if (!type) {
	static const GTypeInfo type_info = {
            sizeof(TestBClass),
            (GBaseInitFunc) NULL,
            (GBaseFinalizeFunc) NULL,
            (GClassInitFunc) test_b_class_init,
            NULL, /* class_finalize */
            NULL, /* class_data */
            sizeof(TestBClass),
            0, /* n_preallocs */
            (GInstanceInitFunc) NULL,
	};

	type = g_type_register_static (TEST_TYPE_A, "TestB",
				       &type_info, 0);
    }
  
  return type;
}


#define TEST_TYPE_C (test_c_get_type())
#define TEST_C(object) (G_TYPE_CHECK_INSTANCE_CAST((object), TEST_TYPE_C, TestC))
#define TEST_C_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), TEST_TYPE_C, TestCClass))
#define TEST_IS_C(object) (G_TYPE_CHECK_INSTANCE_TYPE((object), TEST_TYPE_C))
#define TEST_IS_C_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), TEST_TYPE_C))
#define TEST_C_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), TEST_TYPE_C, TestCClass))

typedef struct _TestC TestC;
typedef struct _TestCClass TestCClass;

struct _TestC {
    TestB parent;
};
struct _TestCClass {
    TestBClass parent_class;
};

static void
test_c_foo (TestA *self)
{
    GValue args[1] = { { 0, } };

    g_print("TestC::foo called.  Chaining up.\n");

    g_value_init (&args[0], TEST_TYPE_A);
    g_value_set_object (&args[0], self);

    g_assert (g_signal_get_invocation_hint (self)->signal_id == foo_signal_id);
    g_signal_chain_from_overridden (args, NULL);

    g_value_unset (&args[0]);
}

static void
test_c_bar (TestI *self)
{
    GValue args[1] = { { 0, } };

    g_print("TestC::bar called.  Chaining up.\n");

    g_value_init (&args[0], TEST_TYPE_A);
    g_value_set_object (&args[0], self);

    g_assert (g_signal_get_invocation_hint (self)->signal_id == bar_signal_id);
    g_signal_chain_from_overridden (args, NULL);

    g_value_unset (&args[0]);
}

static void
test_c_class_init (TestBClass *class)
{
    g_signal_override_class_closure (foo_signal_id,
				     TEST_TYPE_C,
				     g_cclosure_new (G_CALLBACK (test_c_foo),
						     NULL, NULL));
    g_signal_override_class_closure (bar_signal_id,
				     TEST_TYPE_C,
				     g_cclosure_new (G_CALLBACK (test_c_bar),
						     NULL, NULL));
}

static GType
test_c_get_type (void)
{
    static GType type = 0;
  
    if (!type) {
	static const GTypeInfo type_info = {
            sizeof(TestCClass),
            (GBaseInitFunc) NULL,
            (GBaseFinalizeFunc) NULL,
            (GClassInitFunc) test_c_class_init,
            NULL, /* class_finalize */
            NULL, /* class_data */
            sizeof(TestCClass),
            0, /* n_preallocs */
            (GInstanceInitFunc) NULL,
	};

	type = g_type_register_static (TEST_TYPE_B, "TestC",
				       &type_info, 0);
    }
  
  return type;
}


int
main (int argc, char **argv)
{
    GObject *self;

    g_type_init();

    self = g_object_new(TEST_TYPE_A, NULL);
    g_print("*** emiting foo on a TestA instance (expect chain A->I)\n");
    g_signal_emit(self, foo_signal_id, 0);
    g_print("*** emiting bar on a TestA instance\n");
    g_signal_emit(self, bar_signal_id, 0);
    g_object_unref(self);

    g_print("\n");

    self = g_object_new(TEST_TYPE_B, NULL);
    g_print("*** emiting foo on a TestB instance (expect chain B->A->I)\n");
    g_signal_emit(self, foo_signal_id, 0);
    g_print("*** emiting bar on a TestB instance (expect chain B->A)\n");
    g_signal_emit(self, bar_signal_id, 0);
    g_object_unref(self);

    g_print("\n");

    self = g_object_new(TEST_TYPE_C, NULL);
    g_print("*** emiting foo on a TestC instance (expect chain C->B->A->I)\n");
    g_signal_emit(self, foo_signal_id, 0);
    g_print("*** emiting bar on a TestC instance (expect chain C->B->A)\n");
    g_signal_emit(self, bar_signal_id, 0);
    g_object_unref(self);

    return 0;
}
