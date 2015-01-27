#include <glib.h>
#include <string.h>

typedef struct {
  int x, y;
  int width, height;
} Rect;

static gboolean check_free_did_run;

static void
check_rect_free (gpointer data)
{
  Rect *r = data;

  g_assert_false (check_free_did_run);
  g_assert_nonnull (r);

  check_free_did_run = TRUE;
}

static void
refs_generic (void)
{
  Rect *r = g_ref_pointer_new0 (Rect, check_rect_free);

  check_free_did_run = FALSE;

  g_assert_cmpint (r->x, ==, 0);
  g_assert_cmpint (r->height, ==, 0);

  g_ref_pointer_acquire (r);
  r->y = 100;
  g_assert_cmpint (r->y, ==, 100);
  g_ref_pointer_release (r);

  g_assert_false (check_free_did_run);
  g_assert_cmpint (r->y, ==, 100);

  g_ref_pointer_release (r);
  g_assert_true (check_free_did_run);
}

static void
refs_strings (void)
{
  char *orig = g_strdup ("hello");
  const char *new = g_string_ref_new (orig);

  g_assert_false (orig == new);
  g_assert_cmpint (strlen (new), ==, strlen (orig));
  g_assert_true (strcmp (orig, new) == 0);

  memset (orig, 'a', strlen (orig)); 
  g_assert_false (strcmp (orig, new) == 0);
  g_assert_true (strcmp (new, "hello") == 0);

  g_free (orig);

  orig = g_strdup (new);
  g_assert_false (orig == new);
  g_assert_cmpint (strlen (new), ==, strlen (orig));
  g_assert_true (strcmp (orig, new) == 0);

  g_string_unref (new);
  g_free (orig);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/refs/generic", refs_generic);
  g_test_add_func ("/refs/strings", refs_strings);

  return g_test_run ();
}
