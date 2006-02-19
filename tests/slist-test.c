#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include <glib.h>

#define DEBUG_MSG(args) 
/* #define DEBUG_MSG(args) g_printerr args ; g_printerr ("\n"); */
#define PRINT_MSG(args) 
/* #define PRINT_MSG(args) g_print args ; g_print ("\n"); */

#define SIZE       50
#define NUMBER_MIN 0000
#define NUMBER_MAX 9999


static guint32 array[SIZE];


static gint
sort (gconstpointer p1, gconstpointer p2)
{
  gint32 a, b;

  a = GPOINTER_TO_INT (p1);
  b = GPOINTER_TO_INT (p2);

  return (a > b ? +1 : a == b ? 0 : -1);
}

/*
 * gslist sort tests
 */
static void
test_slist_sort (void)
{
  GSList *slist = NULL;
  gint    i;

  PRINT_MSG (("testing g_slist_sort()"));

  for (i = 0; i < SIZE; i++) {
    slist = g_slist_append (slist, GINT_TO_POINTER (array[i]));
  }

  slist = g_slist_sort (slist, sort);
  for (i = 0; i < SIZE - 1; i++) {
    gpointer p1, p2;

    p1 = g_slist_nth_data (slist, i);
    p2 = g_slist_nth_data (slist, i+1);

    g_assert (GPOINTER_TO_INT (p1) <= GPOINTER_TO_INT (p2));
    DEBUG_MSG (("slist_sort #%3.3d ---> %d", i, GPOINTER_TO_INT (p1)));
  }
}

static void
test_slist_sort_with_data (void)
{
  GSList *slist = NULL;
  gint    i;

  PRINT_MSG (("testing g_slist_sort_with_data()"));

  for (i = 0; i < SIZE; i++) {
    slist = g_slist_append (slist, GINT_TO_POINTER (array[i]));
  }

  slist = g_slist_sort_with_data (slist, (GCompareDataFunc)sort, NULL);
  for (i = 0; i < SIZE - 1; i++) {
    gpointer p1, p2;

    p1 = g_slist_nth_data (slist, i);
    p2 = g_slist_nth_data (slist, i+1);

    g_assert (GPOINTER_TO_INT (p1) <= GPOINTER_TO_INT (p2));
    DEBUG_MSG (("slist_sort_with_data #%3.3d ---> %d", i, GPOINTER_TO_INT (p1)));
  }
}

static void
test_slist_insert_sorted (void)
{
  GSList *slist = NULL;
  gint    i;

  PRINT_MSG (("testing g_slist_insert_sorted()"));

  for (i = 0; i < SIZE; i++) {
    slist = g_slist_insert_sorted (slist, GINT_TO_POINTER (array[i]), sort);
  }

  for (i = 0; i < SIZE - 1; i++) {
    gpointer p1, p2;

    p1 = g_slist_nth_data (slist, i);
    p2 = g_slist_nth_data (slist, i+1);

    g_assert (GPOINTER_TO_INT (p1) <= GPOINTER_TO_INT (p2));
    DEBUG_MSG (("slist_insert_sorted #%3.3d ---> %d", i, GPOINTER_TO_INT (p1)));
  }
}

static void
test_slist_insert_sorted_with_data (void)
{
  GSList *slist = NULL;
  gint    i;

  PRINT_MSG (("testing g_slist_insert_sorted_with_data()"));

  for (i = 0; i < SIZE; i++) {
    slist = g_slist_insert_sorted_with_data (slist, 
					   GINT_TO_POINTER (array[i]), 
					   (GCompareDataFunc)sort, 
					   NULL);
  }

  for (i = 0; i < SIZE - 1; i++) {
    gpointer p1, p2;

    p1 = g_slist_nth_data (slist, i);
    p2 = g_slist_nth_data (slist, i+1);

    g_assert (GPOINTER_TO_INT (p1) <= GPOINTER_TO_INT (p2));
    DEBUG_MSG (("slist_insert_sorted_with_data #%3.3d ---> %d", i, GPOINTER_TO_INT (p1)));
  }
}

static void
test_slist_reverse (void)
{
  GSList *slist = NULL;
  GSList *st;
  gint    nums[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  gint    i;

  PRINT_MSG (("testing g_slist_reverse()"));

  for (i = 0; i < 10; i++) {
    slist = g_slist_append (slist, &nums[i]);
  }

  slist = g_slist_reverse (slist);

  for (i = 0; i < 10; i++) {
    st = g_slist_nth (slist, i);
    g_assert (*((gint*) st->data) == (9 - i));
  }

  g_slist_free (slist);
}

static void
test_slist_nth (void)
{
  GSList *slist = NULL;
  GSList *st;
  gint    nums[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  gint    i;

  PRINT_MSG (("testing g_slist_nth()"));

  for (i = 0; i < 10; i++) {
    slist = g_slist_append (slist, &nums[i]);
  }

  for (i = 0; i < 10; i++) {
    st = g_slist_nth (slist, i);
    g_assert (*((gint*) st->data) == i);
  }

  g_slist_free (slist);
}

int
main (int argc, char *argv[])
{
  gint i;

  DEBUG_MSG (("debugging messages turned on"));

  DEBUG_MSG (("creating %d random numbers", SIZE));

  /* Create an array of random numbers. */
  for (i = 0; i < SIZE; i++) {
    array[i] = g_random_int_range (NUMBER_MIN, NUMBER_MAX);
    DEBUG_MSG (("number #%3.3d ---> %d", i, array[i]));
  }

  /* Start tests. */
  test_slist_sort ();
  test_slist_sort_with_data ();

  test_slist_insert_sorted ();
  test_slist_insert_sorted_with_data ();

  test_slist_reverse ();
  test_slist_nth ();

  PRINT_MSG (("testing finished"));

  return 0;
}
