#include <glib.h>

const gint32 first_numbers[] = 
{
  0x7a7a7a7a,
  0x20aea82a,
  0xcab337ab,
  0xdcf770ea,
  0xdf552b2f,
  0x32d1ef7f,
  0x6bed6dd9,
  0x7222df44,
  0x6b842128,
  0x07f8579a,
  0x9dad1004,
  0x2df264f2,
  0x13b48989,
  0xf2929475,
  0x30f30c97,
  0x3f9a1ea7,
  0x3bf04710,
  0xb85bd69e,
  0x790a48b0,
  0xfa06b85f,
  0xa64cc9e3
};

const gint length = sizeof (first_numbers) / sizeof (first_numbers[0]);

int main()
{
  guint n;

  GRand* rand = g_rand_new_with_seed (first_numbers[0]);

  for (n = 1; n < length; n++)
    g_assert (first_numbers[n] == g_rand_int (rand));

  for (n = 1; n < 100000; n++)
    {
      gint32 i;
      gdouble d;
      gboolean b;

      i = g_rand_int_range (rand, 8,16);
      g_assert (i >= 8 && i < 16);
      
      i = g_random_int_range (8,16);
      g_assert (i >= 8 && i < 16);

      d = g_rand_double (rand);
      g_assert (d >= 0 && d < 1);

      d = g_random_double ();
      g_assert (d >= 0 && d < 1);

      d = g_rand_double_range (rand, -8, 32);
      g_assert (d >= -8 && d < 32);
 
      d = g_random_double_range (-8, 32);
      g_assert (d >= -8 && d < 32);
 
      b = g_random_boolean ();
      g_assert (b == TRUE || b  == FALSE);
 
      b = g_rand_boolean (rand);
      g_assert (b == TRUE || b  == FALSE);     
    }

  g_rand_free (rand);

  return 0;
}

