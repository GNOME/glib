#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include <glib.h>

#define DEBUG_MSG(args)
/* #define DEBUG_MSG(args) g_printerr args ; g_printerr ("\n");  */
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
 * glist sort tests
 */
static void
test_list_sort (void)
{
  GList *list = NULL;
  gint   i;

  PRINT_MSG (("testing g_list_sort()"));

  for (i = 0; i < SIZE; i++) {
    list = g_list_append (list, GINT_TO_POINTER (array[i]));
  }

  list = g_list_sort (list, sort);
  for (i = 0; i < SIZE - 1; i++) {
    gpointer p1, p2;

    p1 = g_list_nth_data (list, i);
    p2 = g_list_nth_data (list, i+1);

    g_assert (GPOINTER_TO_INT (p1) <= GPOINTER_TO_INT (p2));
    DEBUG_MSG (("list_sort #%3.3d ---> %d", i, GPOINTER_TO_INT (p1)));
  }

  g_list_free (list);
}

static void
test_list_sort_with_data (void)
{
  GList *list = NULL;
  gint   i;

  PRINT_MSG (("testing g_list_sort_with_data()"));

  for (i = 0; i < SIZE; i++) {
    list = g_list_append (list, GINT_TO_POINTER (array[i]));
  }

  list = g_list_sort_with_data (list, (GCompareDataFunc)sort, NULL);
  for (i = 0; i < SIZE - 1; i++) {
    gpointer p1, p2;

    p1 = g_list_nth_data (list, i);
    p2 = g_list_nth_data (list, i+1);

    g_assert (GPOINTER_TO_INT (p1) <= GPOINTER_TO_INT (p2));
    DEBUG_MSG (("list_sort_with_data #%3.3d ---> %d", i, GPOINTER_TO_INT (p1)));
  }

  g_list_free (list);
}

static void
test_list_insert_sorted (void)
{
  GList *list = NULL;
  gint   i;

  PRINT_MSG (("testing g_list_insert_sorted()"));

  for (i = 0; i < SIZE; i++) {
    list = g_list_insert_sorted (list, GINT_TO_POINTER (array[i]), sort);
  }

  for (i = 0; i < SIZE - 1; i++) {
    gpointer p1, p2;

    p1 = g_list_nth_data (list, i);
    p2 = g_list_nth_data (list, i+1);

    g_assert (GPOINTER_TO_INT (p1) <= GPOINTER_TO_INT (p2));
    DEBUG_MSG (("list_insert_sorted #%3.3d ---> %d", i, GPOINTER_TO_INT (p1)));
  }

  g_list_free (list);
}

static void
test_list_insert_sorted_with_data (void)
{
  GList *list = NULL;
  gint   i;

  PRINT_MSG (("testing g_list_insert_sorted_with_data()"));

  for (i = 0; i < SIZE; i++) {
    list = g_list_insert_sorted_with_data (list, 
					   GINT_TO_POINTER (array[i]), 
					   (GCompareDataFunc)sort, 
					   NULL);
  }

  for (i = 0; i < SIZE - 1; i++) {
    gpointer p1, p2;

    p1 = g_list_nth_data (list, i);
    p2 = g_list_nth_data (list, i+1);

    g_assert (GPOINTER_TO_INT (p1) <= GPOINTER_TO_INT (p2));
    DEBUG_MSG (("list_insert_sorted_with_data #%3.3d ---> %d", i, GPOINTER_TO_INT (p1)));
  }

  g_list_free (list);
}

static void
test_list_reverse (void)
{
  GList *list = NULL;
  GList *st;
  gint   nums[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  gint   i;

  PRINT_MSG (("testing g_list_reverse()"));

  for (i = 0; i < 10; i++) {
    list = g_list_append (list, &nums[i]);
  }

  list = g_list_reverse (list);

  for (i = 0; i < 10; i++) {
    st = g_list_nth (list, i);
    g_assert (*((gint*) st->data) == (9 - i));
  }

  g_list_free (list);
}

static void
test_list_nth (void)
{
  GList *list = NULL;
  GList *st;
  gint   nums[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  gint   i;

  PRINT_MSG (("testing g_list_nth()"));

  for (i = 0; i < 10; i++) {
    list = g_list_append (list, &nums[i]);
  }

  for (i = 0; i < 10; i++) {
    st = g_list_nth (list, i);
    g_assert (*((gint*) st->data) == i);
  }

  g_list_free (list);
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
  test_list_sort ();
  test_list_sort_with_data ();

  test_list_insert_sorted ();
  test_list_insert_sorted_with_data ();

  test_list_reverse ();
  test_list_nth ();

  PRINT_MSG (("testing finished"));

  return 0;
}
