#include <glib.h>

int
main (void)
{
  guint u, u2;
  gint s, s2;
  gpointer vp, vp2;
  int *ip, *ip2;
  gsize gs, gs2;
  gboolean res;

  g_atomic_int_set (&u, 5);
  u2 = g_atomic_int_get (&u);
  g_assert_cmpint (u2, ==, 5);
  res = g_atomic_int_compare_and_exchange (&u, 6, 7);
  g_assert (!res);
  g_assert_cmpint (u, ==, 5);
  g_atomic_int_add (&u, 1);
  g_assert_cmpint (u, ==, 6);
  g_atomic_int_inc (&u);
  g_assert_cmpint (u, ==, 7);
  res = g_atomic_int_dec_and_test (&u);
  g_assert (!res);
  g_assert_cmpint (u, ==, 6);
  u2 = g_atomic_int_and (&u, 5);
  g_assert_cmpint (u2, ==, 6);
  g_assert_cmpint (u, ==, 4);
  u2 = g_atomic_int_or (&u, 8);
  g_assert_cmpint (u2, ==, 4);
  g_assert_cmpint (u, ==, 12);
  u2 = g_atomic_int_xor (&u, 4);
  g_assert_cmpint (u2, ==, 12);
  g_assert_cmpint (u, ==, 8);

  g_atomic_int_set (&s, 5);
  s2 = g_atomic_int_get (&s);
  g_assert_cmpint (s2, ==, 5);
  res = g_atomic_int_compare_and_exchange (&s, 6, 7);
  g_assert (!res);
  g_assert_cmpint (s, ==, 5);
  g_atomic_int_add (&s, 1);
  g_assert_cmpint (s, ==, 6);
  g_atomic_int_inc (&s);
  g_assert_cmpint (s, ==, 7);
  res = g_atomic_int_dec_and_test (&s);
  g_assert (!res);
  g_assert_cmpint (s, ==, 6);
  s2 = g_atomic_int_and (&s, 5);
  g_assert_cmpint (s2, ==, 6);
  g_assert_cmpint (s, ==, 4);
  s2 = g_atomic_int_or (&s, 8);
  g_assert_cmpint (s2, ==, 4);
  g_assert_cmpint (s, ==, 12);
  s2 = g_atomic_int_xor (&s, 4);
  g_assert_cmpint (s2, ==, 12);
  g_assert_cmpint (s, ==, 8);

  g_atomic_pointer_set (&vp, 0);
  vp2 = g_atomic_pointer_get (&vp);
  g_assert (vp2 == 0);
  res = g_atomic_pointer_compare_and_exchange (&vp, 0, 0);
  g_assert (res);
  g_assert (vp == 0);

  g_atomic_pointer_set (&ip, 0);
  ip2 = g_atomic_pointer_get (&ip);
  g_assert (ip2 == 0);
  res = g_atomic_pointer_compare_and_exchange (&ip, 0, 0);
  g_assert (res);
  g_assert (ip == 0);

  g_atomic_pointer_set (&gs, 0);
  gs2 = (gsize) g_atomic_pointer_get (&gs);
  g_assert (gs2 == 0);
  res = g_atomic_pointer_compare_and_exchange (&gs, 0, 0);
  g_assert (res);
  g_assert (gs == 0);
  gs2 = g_atomic_pointer_add (&gs, 5);
  g_assert (gs2 == 0);
  g_assert (gs == 5);
  gs2 = g_atomic_pointer_and (&gs, 6);
  g_assert (gs2 == 5);
  g_assert (gs == 4);
  gs2 = g_atomic_pointer_or (&gs, 8);
  g_assert (gs2 == 4);
  g_assert (gs == 12);
  gs2 = g_atomic_pointer_xor (&gs, 4);
  g_assert (gs2 == 12);
  g_assert (gs == 8);

  /* repeat, without the macros */
#undef g_atomic_int_set
#undef g_atomic_int_get
#undef g_atomic_int_compare_and_exchange
#undef g_atomic_int_add
#undef g_atomic_int_inc
#undef g_atomic_int_and
#undef g_atomic_int_or
#undef g_atomic_int_xor
#undef g_atomic_int_dec_and_test
#undef g_atomic_pointer_set
#undef g_atomic_pointer_get
#undef g_atomic_pointer_compare_and_exchange
#undef g_atomic_pointer_add
#undef g_atomic_pointer_and
#undef g_atomic_pointer_or
#undef g_atomic_pointer_xor

  g_atomic_int_set ((gint*)&u, 5);
  u2 = g_atomic_int_get ((gint*)&u);
  g_assert_cmpint (u2, ==, 5);
  res = g_atomic_int_compare_and_exchange ((gint*)&u, 6, 7);
  g_assert (!res);
  g_assert_cmpint (u, ==, 5);
  g_atomic_int_add ((gint*)&u, 1);
  g_assert_cmpint (u, ==, 6);
  g_atomic_int_inc ((gint*)&u);
  g_assert_cmpint (u, ==, 7);
  res = g_atomic_int_dec_and_test ((gint*)&u);
  g_assert (!res);
  g_assert_cmpint (u, ==, 6);
  u2 = g_atomic_int_and (&u, 5);
  g_assert_cmpint (u2, ==, 6);
  g_assert_cmpint (u, ==, 4);
  u2 = g_atomic_int_or (&u, 8);
  g_assert_cmpint (u2, ==, 4);
  g_assert_cmpint (u, ==, 12);
  u2 = g_atomic_int_xor (&u, 4);
  g_assert_cmpint (u2, ==, 12);

  g_atomic_int_set (&s, 5);
  s2 = g_atomic_int_get (&s);
  g_assert_cmpint (s2, ==, 5);
  res = g_atomic_int_compare_and_exchange (&s, 6, 7);
  g_assert (!res);
  g_assert_cmpint (s, ==, 5);
  g_atomic_int_add (&s, 1);
  g_assert_cmpint (s, ==, 6);
  g_atomic_int_inc (&s);
  g_assert_cmpint (s, ==, 7);
  res = g_atomic_int_dec_and_test (&s);
  g_assert (!res);
  g_assert_cmpint (s, ==, 6);
  s2 = g_atomic_int_and ((guint*)&s, 5);
  g_assert_cmpint (s2, ==, 6);
  g_assert_cmpint (s, ==, 4);
  s2 = g_atomic_int_or ((guint*)&s, 8);
  g_assert_cmpint (s2, ==, 4);
  g_assert_cmpint (s, ==, 12);
  s2 = g_atomic_int_xor ((guint*)&s, 4);
  g_assert_cmpint (s2, ==, 12);
  g_assert_cmpint (s, ==, 8);

  g_atomic_pointer_set (&vp, 0);
  vp2 = g_atomic_pointer_get (&vp);
  g_assert (vp2 == 0);
  res = g_atomic_pointer_compare_and_exchange (&vp, 0, 0);
  g_assert (res);
  g_assert (vp == 0);

  g_atomic_pointer_set (&ip, 0);
  ip2 = g_atomic_pointer_get (&ip);
  g_assert (ip2 == 0);
  res = g_atomic_pointer_compare_and_exchange (&ip, 0, 0);
  g_assert (res);
  g_assert (ip == 0);

  g_atomic_pointer_set (&gs, 0);
  gs2 = (gsize) g_atomic_pointer_get (&gs);
  g_assert (gs2 == 0);
  res = g_atomic_pointer_compare_and_exchange (&gs, 0, 0);
  g_assert (res);
  g_assert (gs == 0);
  gs2 = g_atomic_pointer_add (&gs, 5);
  g_assert (gs2 == 0);
  g_assert (gs == 5);
  gs2 = g_atomic_pointer_and (&gs, 6);
  g_assert (gs2 == 5);
  g_assert (gs == 4);
  gs2 = g_atomic_pointer_or (&gs, 8);
  g_assert (gs2 == 4);
  g_assert (gs == 12);
  gs2 = g_atomic_pointer_xor (&gs, 4);
  g_assert (gs2 == 12);
  g_assert (gs == 8);

  return 0;
}
