#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <glib.h>

int main()
{
  GQueue *q;

  q = g_queue_new ();

  g_queue_push (q, GINT_TO_POINTER (1));
  g_assert (g_list_length (q->list) == 1);
  g_queue_push (q, GINT_TO_POINTER (2));
  g_assert (g_list_length (q->list) == 2);
  g_queue_push (q, GINT_TO_POINTER (3));
  g_assert (g_list_length (q->list) == 3);
  g_queue_push (q, GINT_TO_POINTER (4));
  g_assert (g_list_length (q->list) == 4);
  g_queue_push (q, GINT_TO_POINTER (5));
  g_assert (g_list_length (q->list) == 5);

  g_assert (g_queue_pop (q) == GINT_TO_POINTER (1));
  g_assert (g_list_length (q->list) == 4);
  g_assert (g_queue_pop (q) == GINT_TO_POINTER (2));
  g_assert (g_list_length (q->list) == 3);
  g_assert (g_queue_pop (q) == GINT_TO_POINTER (3));
  g_assert (g_list_length (q->list) == 2);
  g_assert (g_queue_pop (q) == GINT_TO_POINTER (4));
  g_assert (g_list_length (q->list) == 1);
  g_assert (g_queue_pop (q) == GINT_TO_POINTER (5));
  g_assert (g_list_length (q->list) == 0);
  g_assert (g_queue_pop (q) == NULL);
  g_assert (g_list_length (q->list) == 0);
  g_assert (g_queue_pop (q) == NULL);
  g_assert (g_list_length (q->list) == 0);

  /************************/

  g_queue_push_front (q, GINT_TO_POINTER (1));
  g_assert (g_list_length (q->list) == 1);
  g_queue_push_front (q, GINT_TO_POINTER (2));
  g_assert (g_list_length (q->list) == 2);
  g_queue_push_front (q, GINT_TO_POINTER (3));
  g_assert (g_list_length (q->list) == 3);
  g_queue_push_front (q, GINT_TO_POINTER (4));
  g_assert (g_list_length (q->list) == 4);
  g_queue_push_front (q, GINT_TO_POINTER (5));
  g_assert (g_list_length (q->list) == 5);

  g_assert (g_queue_pop_front (q) == GINT_TO_POINTER (5));
  g_assert (g_list_length (q->list) == 4);
  g_assert (g_queue_pop_front (q) == GINT_TO_POINTER (4));
  g_assert (g_list_length (q->list) == 3);
  g_assert (g_queue_pop_front (q) == GINT_TO_POINTER (3));
  g_assert (g_list_length (q->list) == 2);
  g_assert (g_queue_pop_front (q) == GINT_TO_POINTER (2));
  g_assert (g_list_length (q->list) == 1);
  g_assert (g_queue_pop_front (q) == GINT_TO_POINTER (1));
  g_assert (g_list_length (q->list) == 0);
  g_assert (g_queue_pop_front (q) == NULL);
  g_assert (g_list_length (q->list) == 0);
  g_assert (g_queue_pop_front (q) == NULL);
  g_assert (g_list_length (q->list) == 0);

  g_queue_free (q);

  return 0;
}

