#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include <glib.h>

const gint32 first_numbers[] = 
{
  0x7a7a7a7a,
  0xfdcc2d54,
  0x3a279ceb,
  0xc4d39c33,
  0xf31895cd,
  0x46ca0afc,
  0x3f5484ff,
  0x54bc9557,
  0xed2c24b1,
  0x84062503,
  0x8f6404b3,
  0x599a94b3,
  0xe46d03d5,
  0x310beb78,
  0x7bee5d08,
  0x760d09be,
  0x59b6e163,
  0xbf6d16ec,
  0xcca5fb54,
  0x5de7259b,
  0x1696330c,
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

