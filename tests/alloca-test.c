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


void test_alloca (void)
{
  gpointer data;
  struct test_t *t;

  data = g_alloca (15);
  strcpy ((char *) data, "blah blah blah");
  g_assert (strcmp ((char *) data, "blah blah blah") == 0);

  t = g_alloca_new (struct test_t, 1);
  t->age = 142;
  t->stuph = 0xBEDAC0ED;
  strcpy (t->name, "nyognyou hoddypeak");
  g_assert (t->stuph == 0xBEDAC0ED);
  g_assert (strcmp (t->name, "nyognyou hoddypeak") == 0);
}


int main()
{
  test_alloca ();

  g_alloca_gc ();

  return 0;
}

