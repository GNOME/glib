#include <glib-object.h>

typedef struct _Test Test;
typedef struct _TestClass TestClass;

struct _Test
{
  GObject parent_instance;
};

struct _TestClass
{
  GObjectClass parent_class;

  void (* variant_changed) (Test *, GVariant *);
};

static GType test_get_type (void);
G_DEFINE_TYPE (Test, test, G_TYPE_OBJECT)

static void
test_init (Test *test)
{
}

static void
test_class_init (TestClass *klass)
{
  g_signal_new ("variant-changed-no-slot",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST | G_SIGNAL_MUST_COLLECT,
                0,
                NULL, NULL,
                g_cclosure_marshal_VOID__VARIANT,
                G_TYPE_NONE,
                1,
                G_TYPE_VARIANT);
  g_signal_new ("variant-changed",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST | G_SIGNAL_MUST_COLLECT,
                G_STRUCT_OFFSET (TestClass, variant_changed),
                NULL, NULL,
                g_cclosure_marshal_VOID__VARIANT,
                G_TYPE_NONE,
                1,
                G_TYPE_VARIANT);
}

static void
test_variant_signal (void)
{
  Test *test;
  GVariant *v;

  /* Tests that the signal emission consumes the variant,
   * even if there are no handlers connected.
   */

  test = g_object_new (test_get_type (), NULL);

  v = g_variant_new_boolean (TRUE);
  g_variant_ref (v);
  g_assert (g_variant_is_floating (v));
  g_signal_emit_by_name (test, "variant-changed-no-slot", v);
  g_assert (!g_variant_is_floating (v));
  g_variant_unref (v);

  v = g_variant_new_boolean (TRUE);
  g_variant_ref (v);
  g_assert (g_variant_is_floating (v));
  g_signal_emit_by_name (test, "variant-changed", v);
  g_assert (!g_variant_is_floating (v));
  g_variant_unref (v);

  g_object_unref (test);
}

/* --- */

int
main (int argc,
     char *argv[])
{
  g_type_init ();

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gobject/signals/variant", test_variant_signal);

  return g_test_run ();
}
