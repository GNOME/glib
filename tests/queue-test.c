#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <glib.h>

int main()
{
  GQueue *q;
  GList *node;
  gpointer data;

  q = g_queue_new ();

  g_assert (g_queue_is_empty (q) == TRUE);

  g_queue_push_head (q, GINT_TO_POINTER (2));
  g_assert (g_queue_peek_head (q) == GINT_TO_POINTER (2));
  g_assert (g_queue_is_empty (q) == FALSE);
  g_assert (g_list_length (q->head) == 1);
  g_assert (q->head == q->tail);
  g_queue_push_head (q, GINT_TO_POINTER (1));
  g_assert (q->head->next == q->tail);
  g_assert (q->tail->prev == q->head);
  g_assert (g_list_length (q->head) == 2);
  g_assert (q->tail->data == GINT_TO_POINTER (2));
  g_assert (q->head->data == GINT_TO_POINTER (1));
  g_queue_push_tail (q, GINT_TO_POINTER (3));
  g_assert (g_list_length (q->head) == 3);
  g_assert (q->head->data == GINT_TO_POINTER (1));
  g_assert (q->head->next->data == GINT_TO_POINTER (2));
  g_assert (q->head->next->next == q->tail);
  g_assert (q->head->next == q->tail->prev);
  g_assert (q->tail->data == GINT_TO_POINTER (3));
  g_queue_push_tail (q, GINT_TO_POINTER (4));
  g_assert (g_list_length (q->head) == 4);
  g_assert (q->head->data == GINT_TO_POINTER (1));
  g_assert (g_queue_peek_tail (q) == GINT_TO_POINTER (4));
  g_queue_push_tail (q, GINT_TO_POINTER (5));
  g_assert (g_list_length (q->head) == 5);

  g_assert (g_queue_is_empty (q) == FALSE);

  g_assert (q->length == 5);
  g_assert (q->head->prev == NULL);
  g_assert (q->head->data == GINT_TO_POINTER (1));
  g_assert (q->head->next->data == GINT_TO_POINTER (2));
  g_assert (q->head->next->next->data == GINT_TO_POINTER (3));
  g_assert (q->head->next->next->next->data == GINT_TO_POINTER (4));
  g_assert (q->head->next->next->next->next->data == GINT_TO_POINTER (5));
  g_assert (q->head->next->next->next->next->next == NULL);
  g_assert (q->head->next->next->next->next == q->tail);
  g_assert (q->tail->data == GINT_TO_POINTER (5));
  g_assert (q->tail->prev->data == GINT_TO_POINTER (4));
  g_assert (q->tail->prev->prev->data == GINT_TO_POINTER (3));
  g_assert (q->tail->prev->prev->prev->data == GINT_TO_POINTER (2));
  g_assert (q->tail->prev->prev->prev->prev->data == GINT_TO_POINTER (1));
  g_assert (q->tail->prev->prev->prev->prev->prev == NULL);
  g_assert (q->tail->prev->prev->prev->prev == q->head);
  g_assert (g_queue_peek_tail (q) == GINT_TO_POINTER (5));
  g_assert (g_queue_peek_head (q) == GINT_TO_POINTER (1));

  g_assert (g_queue_pop_head (q) == GINT_TO_POINTER (1));
  g_assert (g_list_length (q->head) == 4 && q->length == 4);
  g_assert (g_queue_pop_tail (q) == GINT_TO_POINTER (5));
  g_assert (g_list_length (q->head) == 3);
  g_assert (g_queue_pop_head_link (q)->data == GINT_TO_POINTER (2));
  g_assert (g_list_length (q->head) == 2);
  g_assert (g_queue_pop_tail (q) == GINT_TO_POINTER (4));
  g_assert (g_list_length (q->head) == 1);
  g_assert (g_queue_pop_head_link (q)->data == GINT_TO_POINTER (3));
  g_assert (g_list_length (q->head) == 0);
  g_assert (g_queue_pop_tail (q) == NULL);
  g_assert (g_list_length (q->head) == 0);
  g_assert (g_queue_pop_head (q) == NULL);
  g_assert (g_list_length (q->head) == 0);

  g_assert (g_queue_is_empty (q) == TRUE);

  /************************/

  g_queue_push_head (q, GINT_TO_POINTER (1));
  g_assert (g_list_length (q->head) == 1 && 1 == q->length);
  g_queue_push_head (q, GINT_TO_POINTER (2));
  g_assert (g_list_length (q->head) == 2 && 2 == q->length);
  g_queue_push_head (q, GINT_TO_POINTER (3));
  g_assert (g_list_length (q->head) == 3 && 3 == q->length);
  g_queue_push_head (q, GINT_TO_POINTER (4));
  g_assert (g_list_length (q->head) == 4 && 4 == q->length);
  g_queue_push_head (q, GINT_TO_POINTER (5));
  g_assert (g_list_length (q->head) == 5 && 5 == q->length);

  g_assert (g_queue_pop_head (q) == GINT_TO_POINTER (5));
  g_assert (g_list_length (q->head) == 4);
  node = q->tail;
  g_assert (node == g_queue_pop_tail_link (q));
  g_assert (g_list_length (q->head) == 3);
  data = q->head->data;
  g_assert (data == g_queue_pop_head (q));
  g_assert (g_list_length (q->head) == 2);
  g_assert (g_queue_pop_tail (q) == GINT_TO_POINTER (2));
  g_assert (g_list_length (q->head) == 1);
  g_assert (q->head == q->tail);
  g_assert (g_queue_pop_tail (q) == GINT_TO_POINTER (3));
  g_assert (g_list_length (q->head) == 0);
  g_assert (g_queue_pop_head (q) == NULL);
  g_assert (g_queue_pop_head_link (q) == NULL);
  g_assert (g_list_length (q->head) == 0);
  g_assert (g_queue_pop_tail_link (q) == NULL);
  g_assert (g_list_length (q->head) == 0);

  g_queue_free (q);

  return 0;
}

