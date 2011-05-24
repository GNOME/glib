#include <glib.h>

int
main (void)
{
  guint u;
  gint s;
  gpointer vp;
  int *ip;
  gsize gs;

  g_atomic_int_set (&u, 5);
  g_atomic_int_get (&u);
  g_atomic_int_compare_and_exchange (&u, 6, 7);
  g_atomic_int_exchange_and_add (&u, 1);
  g_atomic_int_add (&u, 1);
  g_atomic_int_inc (&u);
  (void) g_atomic_int_dec_and_test (&u);

  g_atomic_int_set (&s, 5);
  g_atomic_int_get (&s);
  g_atomic_int_compare_and_exchange (&s, 6, 7);
  g_atomic_int_exchange_and_add (&s, 1);
  g_atomic_int_add (&s, 1);
  g_atomic_int_inc (&s);
  (void) g_atomic_int_dec_and_test (&s);

  g_atomic_pointer_set (&vp, 0);
  g_atomic_pointer_get (&vp);
  g_atomic_pointer_compare_and_exchange (&vp, 0, 0);

  g_atomic_pointer_set (&ip, 0);
  g_atomic_pointer_get (&ip);
  g_atomic_pointer_compare_and_exchange (&ip, 0, 0);

  g_atomic_pointer_set (&gs, 0);
  g_atomic_pointer_get (&gs);
  g_atomic_pointer_compare_and_exchange (&gs, 0, 0);

  return 0;
}
