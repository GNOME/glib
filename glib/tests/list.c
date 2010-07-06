#include <glib.h>

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

  for (i = 0; i < SIZE; i++)
    list = g_list_append (list, GINT_TO_POINTER (array[i]));

  list = g_list_sort (list, sort);
  for (i = 0; i < SIZE - 1; i++)
    {
      gpointer p1, p2;

      p1 = g_list_nth_data (list, i);
      p2 = g_list_nth_data (list, i+1);

      g_assert (GPOINTER_TO_INT (p1) <= GPOINTER_TO_INT (p2));
    }

  g_list_free (list);
}

static void
test_list_sort_with_data (void)
{
  GList *list = NULL;
  gint   i;

  for (i = 0; i < SIZE; i++)
    list = g_list_append (list, GINT_TO_POINTER (array[i]));

  list = g_list_sort_with_data (list, (GCompareDataFunc)sort, NULL);
  for (i = 0; i < SIZE - 1; i++)
    {
      gpointer p1, p2;

      p1 = g_list_nth_data (list, i);
      p2 = g_list_nth_data (list, i+1);

      g_assert (GPOINTER_TO_INT (p1) <= GPOINTER_TO_INT (p2));
    }

  g_list_free (list);
}

static void
test_list_insert_sorted (void)
{
  GList *list = NULL;
  gint   i;

  for (i = 0; i < SIZE; i++)
    list = g_list_insert_sorted (list, GINT_TO_POINTER (array[i]), sort);

  for (i = 0; i < SIZE - 1; i++)
    {
      gpointer p1, p2;

      p1 = g_list_nth_data (list, i);
      p2 = g_list_nth_data (list, i+1);

      g_assert (GPOINTER_TO_INT (p1) <= GPOINTER_TO_INT (p2));
    }

  g_list_free (list);
}

static void
test_list_insert_sorted_with_data (void)
{
  GList *list = NULL;
  gint   i;

  for (i = 0; i < SIZE; i++)
    list = g_list_insert_sorted_with_data (list,
                                           GINT_TO_POINTER (array[i]),
                                           (GCompareDataFunc)sort,
                                           NULL);

  for (i = 0; i < SIZE - 1; i++)
    {
      gpointer p1, p2;

      p1 = g_list_nth_data (list, i);
      p2 = g_list_nth_data (list, i+1);

      g_assert (GPOINTER_TO_INT (p1) <= GPOINTER_TO_INT (p2));
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

  for (i = 0; i < 10; i++)
    list = g_list_append (list, &nums[i]);

  list = g_list_reverse (list);

  for (i = 0; i < 10; i++)
    {
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

  for (i = 0; i < 10; i++)
    list = g_list_append (list, &nums[i]);

  for (i = 0; i < 10; i++)
    {
      st = g_list_nth (list, i);
      g_assert (*((gint*) st->data) == i);
    }

  g_list_free (list);
}

int
main (int argc, char *argv[])
{
  gint i;

  g_test_init (&argc, &argv, NULL);

  /* Create an array of random numbers. */
  for (i = 0; i < SIZE; i++)
    array[i] = g_test_rand_int_range (NUMBER_MIN, NUMBER_MAX);

  g_test_add_func ("/list/sort", test_list_sort);
  g_test_add_func ("/list/sort-with-data", test_list_sort_with_data);
  g_test_add_func ("/list/insert-sorted", test_list_insert_sorted);
  g_test_add_func ("/list/insert-sorted-with-data", test_list_insert_sorted_with_data);
  g_test_add_func ("/list/reverse", test_list_reverse);
  g_test_add_func ("/list/nth", test_list_nth);

  return g_test_run ();
}
