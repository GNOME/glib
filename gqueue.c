/*
* GQueue, opaque ADT with interface:

	q = g_queue_new ();
	count = g_queue_size (q);

	q_queue_push_front (q, data);
	q_queue_push_back (q, data);
	data = q_queue_pop_front (q);
	data = q_queue_pop_back (q);
	#define q_queue_push q_queue_push_back
	#define q_queue_pop q_queue_pop_front

	list = g_queue_get_list (q);
	#define g_queue_get_front g_queue_get_list
	list_end = g_queue_get_back (q);
*/


#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>




GQueue *
g_queue_new (void)
{
  GQueue *q = g_new (GQueue, 1);

  q->list = q->list_end = NULL;
  q->list_size = 0;

  return q;
}


void
g_queue_free (GQueue *q)
{
  if (q) {
    if (q->list)
      g_list_free (q->list);
    g_free (q);
  }
}


guint
g_queue_get_size (GQueue *q)
{
  return (q == NULL) ? 0 : q->list_size;
}


void
g_queue_push_front (GQueue *q, gpointer data)
{
  if (q) {
    q->list = g_list_prepend (q->list, data);

    if (q->list_end == NULL)
      q->list_end = q->list;

    q->list_size++;
  }
}


void
g_queue_push_back (GQueue *q, gpointer data)
{
  if (q) {
    q->list_end = g_list_append (q->list_end, data);

    if (! q->list)
      q->list = q->list_end;
    else
      q->list_end = q->list_end->next;

    q->list_size++;
  }
}


gpointer
g_queue_pop_front (GQueue *q)
{
  gpointer data = NULL;

  if ((q) && (q->list)) {
    GList *node;

    node = q->list;
    data = node->data;

    if (! node->next) {
      q->list = q->list_end = NULL;
      q->list_size = 0;
    }
    else {
      q->list = node->next;
      q->list->prev = NULL;
      q->list_size--;
    }

    g_list_free_1 (node);
  }

  return data;
}


gpointer
g_queue_pop_back (GQueue *q)
{
  gpointer data = NULL;

  if ((q) && (q->list)) {
    GList *node;

    node = q->list_end;
    data = node->data;

    if (! node->prev) {
      q->list = q->list_end = NULL;
      q->list_size = 0;
    }
    else {
      q->list_end = node->prev;
      q->list_end->next = NULL;
      q->list_size--;
    }

    g_list_free_1 (node);
  }

  return data;
}


