#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#if STDC_HEADERS
#  include <string.h>
#endif

#include <glib.h>

struct test_t {
	int age;
	char name [19];
	int stuph;
};


void test_alloca (gint recursions)
{
  gpointer data;
  struct test_t *t;

  data = g_alloca (15);
  g_assert (data != NULL);
  strcpy ((char *) data, "blah blah blah");
  g_assert (strcmp ((char *) data, "blah blah blah") == 0);

  t = g_alloca_new (struct test_t, 1);
  g_assert (t != NULL);
  t->age = 142;
  t->stuph = 0xBEDAC0ED;
  strcpy (t->name, "nyognyou hoddypeak");
  g_assert (t->stuph == 0xBEDAC0ED);
  g_assert (strcmp (t->name, "nyognyou hoddypeak") == 0);

  if (recursions > 0)
    test_alloca (recursions - 1);

  g_assert (strcmp ((char *) data, "blah blah blah") == 0);
  g_assert (t->stuph == 0xBEDAC0ED);
  g_assert (strcmp (t->name, "nyognyou hoddypeak") == 0);
}


/* prototype in case we have regular alloca as well */
gpointer _g_alloca (guint size);

void test_alloca_replacement (gint recursions)
{
  gpointer data;
  struct test_t *t;

  data = _g_alloca (15);
  g_assert (data != NULL);
  strcpy ((char *) data, "blah blah blah");
  g_assert (strcmp ((char *) data, "blah blah blah") == 0);

  t = (struct test_t *) _g_alloca (sizeof (struct test_t));
  g_assert (t != NULL);
  t->age = 142;
  t->stuph = 0xBEDAC0ED;
  strcpy (t->name, "nyognyou hoddypeak");
  g_assert (t->stuph == 0xBEDAC0ED);
  g_assert (strcmp (t->name, "nyognyou hoddypeak") == 0);

  if (recursions > 0)
    test_alloca_replacement (recursions - 1);

  g_assert (strcmp ((char *) data, "blah blah blah") == 0);
  g_assert (t->stuph == 0xBEDAC0ED);
  g_assert (strcmp (t->name, "nyognyou hoddypeak") == 0);
}

const int AllocaTestIterations = 3;

int main()
{
  int i;

  for (i = 0; i < AllocaTestIterations; i++) {
    test_alloca (3);
    test_alloca_replacement (3);

    g_alloca_gc ();
    _g_alloca (0);
  }

  return 0;
}

