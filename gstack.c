#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <glib.h>

/*
* GStack, opaque ADT with interface:

	stack = g_stack_new ();
	count = g_stack_size (stack);

	g_stack_push (stack, data);
	data = g_stack_pop (stack);

	slist = g_stack_get_list (stack);
*/

GStack *
g_stack_new (void)
{
  GStack *s;
  
  s = g_new (GStack, 1);
  if (!s)
    return NULL;

  s->list = NULL;

  return s;
}


void
g_stack_free (GStack *stack)
{
  if (stack) {
    if (stack->list)
      g_list_free (stack->list);

    g_free (stack);
  }
}


gpointer
g_stack_pop (GStack *stack)
{
  gpointer data = NULL;

  if ((stack) && (stack->list)) {
    GList *node = stack->list;

    stack->list = stack->list->next;

    data = node->data;

    g_list_free_1 (node);
  }

  return data;
}


