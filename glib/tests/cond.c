/* Unit tests for GCond
 * Copyright (C) 2011 Red Hat, Inc
 * Author: Matthias Clasen
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In no event shall the authors or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 */

#include <glib.h>

static GCond cond = G_COND_INIT;
static GMutex mutex = G_MUTEX_INIT;
static volatile gint next;

static void
push_value (gint value)
{
  g_mutex_lock (&mutex);
  next = value;
  if (g_test_verbose ())
    g_print ("Thread %p producing next value: %d\n", g_thread_self (), value);
  if (value % 10 == 0)
    g_cond_broadcast (&cond);
  else
    g_cond_signal (&cond);
  g_mutex_unlock (&mutex);
}

static gint
pop_value (void)
{
  gint value;

  g_mutex_lock (&mutex);
  while (next == 0)
    {
      if (g_test_verbose ())
        g_print ("Thread %p waiting for cond\n", g_thread_self ());
      g_cond_wait (&cond, &mutex);
    }
  value = next;
  next = 0;
  if (g_test_verbose ())
    g_print ("Thread %p consuming value %d\n", g_thread_self (), value);
  g_mutex_unlock (&mutex);

  return value;
}

static gpointer
produce_values (gpointer data)
{
  gint total;
  gint i;

  for (i = 1; i < 100; i++)
    {
      total += i;
      push_value (i);
      g_usleep (1000);
    }

  push_value (-1);
  g_usleep (1000);
  push_value (-1);

  if (g_test_verbose ())
    g_print ("Thread %p produced %d altogether\n", g_thread_self (), total);

  return GINT_TO_POINTER (total);
}

static gpointer
consume_values (gpointer data)
{
  gint accum = 0;
  gint value;

  while (TRUE)
    {
      value = pop_value ();
      if (value == -1)
        break;

      accum += value;
    }

  if (g_test_verbose ())
    g_print ("Thread %p accumulated %d\n", g_thread_self (), accum);

  return GINT_TO_POINTER (accum);
}

static GThread *producer, *consumer1, *consumer2;

static void
test_cond1 (void)
{
  gint total, acc1, acc2;

  producer = g_thread_create (produce_values, NULL, TRUE, NULL);
  consumer1 = g_thread_create (consume_values, NULL, TRUE, NULL);
  consumer2 = g_thread_create (consume_values, NULL, TRUE, NULL);

  total = GPOINTER_TO_INT (g_thread_join (producer));
  acc1 = GPOINTER_TO_INT (g_thread_join (consumer1));
  acc2 = GPOINTER_TO_INT (g_thread_join (consumer2));

  g_assert_cmpint (total, ==, acc1 + acc2);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/thread/cond1", test_cond1);

  return g_test_run ();
}
