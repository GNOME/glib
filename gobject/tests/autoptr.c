#include <glib-object.h>
#include <string.h>

G_DECLARE_FINAL_TYPE (TestAutoCleanup, test_auto_cleanup, TEST, AUTO_CLEANUP, GObject);

struct _TestAutoCleanup
{
  GObject parent_instance;
};

G_DEFINE_TYPE (TestAutoCleanup, test_auto_cleanup, G_TYPE_OBJECT)

static void
test_auto_cleanup_class_init (TestAutoCleanupClass *class)
{
}

static void
test_auto_cleanup_init (TestAutoCleanup *tac)
{
}

static TestAutoCleanup *
test_auto_cleanup_new (void)
{
  return g_object_new (test_auto_cleanup_get_type (), NULL);
}

static void
test_autoptr (void)
{
  TestAutoCleanup *tac_ptr = test_auto_cleanup_new ();
  g_object_add_weak_pointer (G_OBJECT (tac_ptr), (gpointer *) &tac_ptr);

  {
    g_autoptr (TestAutoCleanup) tac = tac_ptr;
  }
#ifdef __GNUC__
  g_assert(!tac_ptr);
#endif
}

static void
test_autoptr_steal (void)
{
  g_autoptr (TestAutoCleanup) tac1 = test_auto_cleanup_new ();
  TestAutoCleanup *tac_ptr = tac1;

  g_object_add_weak_pointer (G_OBJECT (tac_ptr), (gpointer *) &tac_ptr);

  {
    g_autoptr (TestAutoCleanup) tac2 = g_steal_pointer (&tac1);
    g_assert (tac_ptr);
    g_assert (!tac1);
    g_assert (tac2 == tac_ptr);
  }
#ifdef __GNUC__
  g_assert(!tac_ptr);
#endif
}

static void
test_autolist (void)
{
  TestAutoCleanup *tac1 = test_auto_cleanup_new ();
  TestAutoCleanup *tac2 = test_auto_cleanup_new ();
  g_autoptr (TestAutoCleanup) tac3 = test_auto_cleanup_new ();

  g_object_add_weak_pointer (G_OBJECT (tac1), (gpointer *) &tac1);
  g_object_add_weak_pointer (G_OBJECT (tac2), (gpointer *) &tac2);
  g_object_add_weak_pointer (G_OBJECT (tac3), (gpointer *) &tac3);

  {
    g_autolist(TestAutoCleanup) l = NULL;

    l = g_list_prepend (l, tac1);
    l = g_list_prepend (l, tac2);
  }

  /* Only assert if autoptr works */
#ifdef __GNUC__
  g_assert (!tac1);
  g_assert (!tac2);
#endif
  g_assert (tac3);

  g_clear_object(&tac3);
  g_assert (!tac3);
}

static void
test_autoslist (void)
{
  TestAutoCleanup *tac1 = test_auto_cleanup_new ();
  TestAutoCleanup *tac2 = test_auto_cleanup_new ();
  g_autoptr (TestAutoCleanup) tac3 = test_auto_cleanup_new ();

  g_object_add_weak_pointer (G_OBJECT (tac1), (gpointer *) &tac1);
  g_object_add_weak_pointer (G_OBJECT (tac2), (gpointer *) &tac2);
  g_object_add_weak_pointer (G_OBJECT (tac3), (gpointer *) &tac3);

  {
    g_autoslist(TestAutoCleanup) l = NULL;

    l = g_slist_prepend (l, tac1);
    l = g_slist_prepend (l, tac2);
  }

  /* Only assert if autoptr works */
#ifdef __GNUC__
  g_assert (!tac1);
  g_assert (!tac2);
#endif
  g_assert (tac3);

  g_clear_object(&tac3);
  g_assert (!tac3);
}

int
main (int argc, gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/autoptr/autoptr", test_autoptr);
  g_test_add_func ("/autoptr/autoptr_steal", test_autoptr_steal);
  g_test_add_func ("/autoptr/autolist", test_autolist);
  g_test_add_func ("/autoptr/autoslist", test_autoslist);

  return g_test_run ();
}
