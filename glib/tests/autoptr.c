#include <glib.h>

static void
test_autofree (void)
{
  g_autofree char *p = NULL;
  g_autofree char *p2 = NULL;
  g_autofree char *alwaysnull = NULL;

  p = g_malloc (10);
  p2 = g_malloc (42);

  if (TRUE)
    {
      g_autofree guint8 *buf = g_malloc (128);
      g_autofree char *alwaysnull_again = NULL;

      buf[0] = 1;
    }

  if (TRUE)
    {
      g_autofree guint8 *buf2 = g_malloc (256);

      buf2[255] = 42;
    }
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/autoptr/autofree", test_autofree);

  return g_test_run ();
}
