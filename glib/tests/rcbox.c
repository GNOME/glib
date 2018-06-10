#include <glib.h>

typedef struct {
  float x, y;
} Point;

static Point *global_point;

static void
test_rcbox_new (void)
{
  Point *a = g_rc_box_new (Point);

  g_assert_nonnull (a);

  g_rc_box_release (a);

  a = g_rc_box_new0 (Point);
  g_assert_nonnull (a);
  g_assert_cmpfloat (a->x, ==, 0.f);
  g_assert_cmpfloat (a->y, ==, 0.f);

  g_rc_box_release (a);
}

static void
point_clear (Point *p)
{
  g_assert_nonnull (p);

  g_assert_cmpfloat (p->x, ==, 42.0f);
  g_assert_cmpfloat (p->y, ==, 47.0f);

  g_assert_true (global_point == p);
  global_point = NULL;
}

static void
test_rcbox_release_full (void)
{
  Point *p = g_rc_box_new (Point);

  g_assert_nonnull (p);
  global_point = p;

  p->x = 42.0f;
  p->y = 47.0f;

  g_assert_true (g_rc_box_acquire (p) == p);

  g_rc_box_release_full (p, (GDestroyNotify) point_clear);
  g_assert_nonnull (global_point);
  g_assert_true (p == global_point);

  g_rc_box_release_full (p, (GDestroyNotify) point_clear);
  g_assert_null (global_point);
}

static void
test_rcbox_dup (void)
{
  Point *a = g_rc_box_new (Point);
  Point *b = g_rc_box_dup (a);

  g_assert_true (a != b);

  g_rc_box_release (a);
  g_rc_box_release (b);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/rcbox/new", test_rcbox_new);
  g_test_add_func ("/rcbox/dup", test_rcbox_dup);
  g_test_add_func ("/rcbox/release-full", test_rcbox_release_full);

  return g_test_run ();
}
