#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>

int main()
{
  GStack *s;

  s = g_stack_new ();

  g_assert (g_stack_empty (s) == TRUE);

  g_stack_push (s, GINT_TO_POINTER (1));
  g_assert (g_list_length (s->list) == 1);
  g_stack_push (s, GINT_TO_POINTER (2));
  g_assert (g_list_length (s->list) == 2);
  g_stack_push (s, GINT_TO_POINTER (3));
  g_assert (g_list_length (s->list) == 3);
  g_stack_push (s, GINT_TO_POINTER (4));
  g_assert (g_list_length (s->list) == 4);
  g_stack_push (s, GINT_TO_POINTER (5));
  g_assert (g_list_length (s->list) == 5);

  g_assert (g_stack_index (s, GINT_TO_POINTER (2)) == 3);

  g_assert (g_stack_empty (s) == FALSE);

  g_assert (g_stack_peek (s) == GINT_TO_POINTER (5));

  g_assert (g_stack_pop (s) == GINT_TO_POINTER (5));
  g_assert (g_list_length (s->list) == 4);
  g_assert (g_stack_pop (s) == GINT_TO_POINTER (4));
  g_assert (g_list_length (s->list) == 3);
  g_assert (g_stack_pop (s) == GINT_TO_POINTER (3));
  g_assert (g_list_length (s->list) == 2);
  g_assert (g_stack_pop (s) == GINT_TO_POINTER (2));
  g_assert (g_list_length (s->list) == 1);
  g_assert (g_stack_pop (s) == GINT_TO_POINTER (1));
  g_assert (g_list_length (s->list) == 0);
  g_assert (g_stack_pop (s) == NULL);
  g_assert (g_list_length (s->list) == 0);
  g_assert (g_stack_pop (s) == NULL);
  g_assert (g_list_length (s->list) == 0);

  g_stack_free (s);

  return 0;
}

