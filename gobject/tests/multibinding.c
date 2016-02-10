#include <stdlib.h>
#include <gstdio.h>
#include <glib-object.h>

typedef struct _BindingSource
{
  GObject parent_instance;

  gint foo;
  gint bar;
  gdouble value;
  gboolean toggle;
} BindingSource;

typedef struct _BindingSourceClass
{
  GObjectClass parent_class;
} BindingSourceClass;

enum
{
  PROP_SOURCE_0,

  PROP_SOURCE_FOO,
  PROP_SOURCE_BAR,
  PROP_SOURCE_VALUE,
  PROP_SOURCE_TOGGLE
};

static GType binding_source_get_type (void);
G_DEFINE_TYPE (BindingSource, binding_source, G_TYPE_OBJECT);

static void
binding_source_set_property (GObject      *gobject,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  BindingSource *source = (BindingSource *) gobject;

  switch (prop_id)
    {
    case PROP_SOURCE_FOO:
      source->foo = g_value_get_int (value);
      break;

    case PROP_SOURCE_BAR:
      source->bar = g_value_get_int (value);
      break;

    case PROP_SOURCE_VALUE:
      source->value = g_value_get_double (value);
      break;

    case PROP_SOURCE_TOGGLE:
      source->toggle = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
binding_source_get_property (GObject    *gobject,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  BindingSource *source = (BindingSource *) gobject;

  switch (prop_id)
    {
    case PROP_SOURCE_FOO:
      g_value_set_int (value, source->foo);
      break;

    case PROP_SOURCE_BAR:
      g_value_set_int (value, source->bar);
      break;

    case PROP_SOURCE_VALUE:
      g_value_set_double (value, source->value);
      break;

    case PROP_SOURCE_TOGGLE:
      g_value_set_boolean (value, source->toggle);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
binding_source_class_init (BindingSourceClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = binding_source_set_property;
  gobject_class->get_property = binding_source_get_property;

  g_object_class_install_property (gobject_class, PROP_SOURCE_FOO,
                                   g_param_spec_int ("foo", "Foo", "Foo",
                                                     -1, 100,
                                                     0,
                                                     G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_SOURCE_BAR,
                                   g_param_spec_int ("bar", "Bar", "Bar",
                                                     -1, 100,
                                                     0,
                                                     G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_SOURCE_VALUE,
                                   g_param_spec_double ("value", "Value", "Value",
                                                        -100.0, 200.0,
                                                        0.0,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_SOURCE_TOGGLE,
                                   g_param_spec_boolean ("toggle", "Toggle", "Toggle",
                                                         FALSE,
                                                         G_PARAM_READWRITE));
}

static void
binding_source_init (BindingSource *self)
{
}

typedef struct _BindingTarget
{
  GObject parent_instance;

  gint bar;
  gdouble value;
  gboolean toggle;
} BindingTarget;

typedef struct _BindingTargetClass
{
  GObjectClass parent_class;
} BindingTargetClass;

enum
{
  PROP_TARGET_0,

  PROP_TARGET_BAR,
  PROP_TARGET_VALUE,
  PROP_TARGET_TOGGLE
};

static GType binding_target_get_type (void);
G_DEFINE_TYPE (BindingTarget, binding_target, G_TYPE_OBJECT);

static void
binding_target_set_property (GObject      *gobject,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  BindingTarget *target = (BindingTarget *) gobject;

  switch (prop_id)
    {
    case PROP_TARGET_BAR:
      target->bar = g_value_get_int (value);
      break;

    case PROP_TARGET_VALUE:
      target->value = g_value_get_double (value);
      break;

    case PROP_TARGET_TOGGLE:
      target->toggle = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
binding_target_get_property (GObject    *gobject,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  BindingTarget *target = (BindingTarget *) gobject;

  switch (prop_id)
    {
    case PROP_TARGET_BAR:
      g_value_set_int (value, target->bar);
      break;

    case PROP_TARGET_VALUE:
      g_value_set_double (value, target->value);
      break;

    case PROP_TARGET_TOGGLE:
      g_value_set_boolean (value, target->toggle);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
binding_target_class_init (BindingTargetClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = binding_target_set_property;
  gobject_class->get_property = binding_target_get_property;

  g_object_class_install_property (gobject_class, PROP_TARGET_BAR,
                                   g_param_spec_int ("bar", "Bar", "Bar",
                                                     -1, 100,
                                                     0,
                                                     G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_TARGET_VALUE,
                                   g_param_spec_double ("value", "Value", "Value",
                                                        -100.0, 200.0,
                                                        0.0,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_TARGET_TOGGLE,
                                   g_param_spec_boolean ("toggle", "Toggle", "Toggle",
                                                         FALSE,
                                                         G_PARAM_READWRITE));
}

static void
binding_target_init (BindingTarget *self)
{
}

static gboolean
munge_two_ints (GMultiBinding  *binding,
                gint            notified,
                const GValue    from_values[],
                GValue          to_values[],
                gpointer        user_data)
{
  int v0, v1;

  v0 = g_value_get_int (&from_values[0]);
  v1 = g_value_get_int (&from_values[1]);

  g_value_set_int (&to_values[0], v0 + v1);
  g_value_set_int (&to_values[1], v0 - v1);
}

static void
multibinding_basic (void)
{
  BindingSource *source0 = g_object_new (binding_source_get_type (), NULL);
  BindingSource *source1 = g_object_new (binding_source_get_type (), NULL);
  GObject *sources[2];
  const char *source_props[2];
  BindingTarget *target0 = g_object_new (binding_target_get_type (), NULL);
  BindingTarget *target1 = g_object_new (binding_target_get_type (), NULL);
  GObject *targets[2];
  const char *target_props[2];
  GMultiBinding *binding;

  sources[0] = G_OBJECT (source0);
  sources[1] = G_OBJECT (source1);
  source_props[0] = "foo";
  source_props[1] = "bar";
  targets[0] = G_OBJECT (target0);
  targets[1] = G_OBJECT (target1);
  target_props[0] = "bar";
  target_props[1] = "bar";
  binding = g_object_multi_bind_property_v (2, sources, source_props,
                                            2, targets, target_props,
                                            G_MULTI_BINDING_DEFAULT,
                                            munge_two_ints,
                                            NULL, NULL);
  g_object_add_weak_pointer (G_OBJECT (binding), (gpointer *) &binding);

  g_assert_cmpint (g_multi_binding_get_n_sources (binding), ==, 2);
  g_assert (g_multi_binding_get_source (binding, 0) == sources[0]);
  g_assert (g_multi_binding_get_source (binding, 1) == sources[1]);
  g_assert_cmpstr (g_multi_binding_get_source_property (binding, 0), ==, source_props[0]);
  g_assert_cmpstr (g_multi_binding_get_source_property (binding, 1), ==, source_props[1]);
  g_assert_cmpint (g_multi_binding_get_n_targets (binding), ==, 2);
  g_assert (g_multi_binding_get_target (binding, 0) == targets[0]);
  g_assert (g_multi_binding_get_target (binding, 1) == targets[1]);
  g_assert_cmpstr (g_multi_binding_get_target_property (binding, 0), ==, target_props[0]);
  g_assert_cmpstr (g_multi_binding_get_target_property (binding, 1), ==, target_props[1]);

  g_assert_cmpint (source0->foo, ==, 0);
  g_assert_cmpint (source1->bar, ==, 0);
  g_assert_cmpint (target0->bar, ==, 0);
  g_assert_cmpint (target1->bar, ==, 0);

  g_object_set (source0, "foo", 1, NULL);

  g_assert_cmpint (source0->foo, ==, 1);
  g_assert_cmpint (source1->bar, ==, 0);
  g_assert_cmpint (target0->bar, ==, source0->foo + source1->bar);
  g_assert_cmpint (target1->bar, ==, source0->foo - source1->bar);

  g_object_set (source1, "foo", 1, NULL);

  g_assert_cmpint (source0->foo, ==, 1);
  g_assert_cmpint (source1->bar, ==, 0);
  g_assert_cmpint (target0->bar, ==, source0->foo + source1->bar);
  g_assert_cmpint (target1->bar, ==, source0->foo - source1->bar);

  g_object_set (source0, "bar", 1, NULL);

  g_assert_cmpint (source0->foo, ==, 1);
  g_assert_cmpint (source1->bar, ==, 0);
  g_assert_cmpint (target0->bar, ==, source0->foo + source1->bar);
  g_assert_cmpint (target1->bar, ==, source0->foo - source1->bar);

  g_object_set (source1, "bar", 1, NULL);

  g_assert_cmpint (source0->foo, ==, 1);
  g_assert_cmpint (source1->bar, ==, 1);
  g_assert_cmpint (target0->bar, ==, source0->foo + source1->bar);
  g_assert_cmpint (target1->bar, ==, source0->foo - source1->bar);

  g_object_unref (source0);
  g_object_unref (source1);
  g_object_unref (target0);
  g_object_unref (target1);
  g_assert (binding == NULL);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_bug_base ("http://bugzilla.gnome.org/");

  g_test_add_func ("/multibinding/basic", multibinding_basic);

  return g_test_run ();
}
