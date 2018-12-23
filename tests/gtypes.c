/* GLib testing framework examples and tests
 *
 * Copyright © 2018 Tapasweni Pathak
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Tapasweni Pathak <tapaswenipathak@gmail.com>
 */

#include <glib.h>
#include <gtypes.h>
#include <math.h>

/* Test the GFLOAT_SWAP_LE_BE, GFLOAT_UNSWAP_LE_BE and GDOUBLE_SWAP_LE_BE,
 * GFLOAT_UNSWAP_LE_BE. For float and double type shouldn't be used again as float
 * and double type until it is unswapped.
 *
 * This is necessary to make sure FROM_LE, FROM_BE, TO_LE, TO_BE macros works
 * as expected.
 */

static void
test_gfloat_swap_le_be_unswap (void)
{
#define GFLOAT_SWAP_LE_BE(val) \
  g_assert_cmpint (G_FLOAT_SWAP_LE_BE (val), ==, GFLOAT_UNSWAP_LE_BE (val))

  gfloat a, c;
  gulong b;
  for (a = 0.0; a < 100.0; a += 0.01)
  {
    b = GFLOAT_SWAP_LE_BE(a);
    c = GFLOAT_UNSWAP_LE_BE(b);
    if (a != c)
	{
    }
  }
}

static void
test_gdouble_swap_le_be_unswap (void)
{
  #define GDOUBLE_SWAP_LE_BE(val) \
    g_assert_cmpint (G_DOUBLE_SWAP_LE_BE (val), ==, GDOUBLE_UNSWAP_LE_BE (val))
  gdouble a, c;
  gullong b;
  for (a = 0.0; a < 100.0; a += 0.01)
  {
    b = GFLOAT_SWAP_LE_BE(a);
    c = GFLOAT_UNSWAP_LE_BE(b);
    if (a != c)
	{
    }
  }
}


int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/GFLOAT_SWAP_LE_BE/SWAP_UNSWAP", test_gfloat_swap_le_be_unswap);
  g_test_add_func ("/GDOUBLE_SWAP_LE_BE/SWAP", test_gfloat_swap_le_be_unswap);

  return g_test_run ();
}

