#include "config.h"

#include <glib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>

#define DATA_SIZE 1024
#define BLOCK_SIZE 32
#define NUM_BLOCKS 32
static guchar data[DATA_SIZE];

static void
test_incremental (gboolean line_break,
                  gint     length)
{
  char *p;
  gsize len, decoded_len, max, input_len, block_size;
  int state, save;
  guint decoder_save;
  char *text;
  guchar *data2;

  data2 = g_malloc (length);
  text = g_malloc (length * 4);

  len = 0;
  state = 0;
  save = 0;
  input_len = 0;
  while (input_len < length)
    {
      block_size = MIN (BLOCK_SIZE, length - input_len);
      len += g_base64_encode_step (data + input_len, block_size,
                                   line_break, text + len, &state, &save);
      input_len += block_size;
    }
  len += g_base64_encode_close (line_break, text + len, &state, &save);

  if (line_break)
    max = length * 4 / 3 + length * 4 / (3 * 72) + 7;
  else
    max = length * 4 / 3 + 6;

  /* Check encoded length */
  g_assert_cmpint (len, <=, max);

  decoded_len = 0;
  state = 0;
  decoder_save = 0;
  p = text;
  while (len > 0)
    {
      int chunk_len = MIN (BLOCK_SIZE, len);
      decoded_len += g_base64_decode_step (p,
                                           chunk_len,
                                           data2 + decoded_len,
                                           &state, &decoder_save);
      p += chunk_len;
      len -= chunk_len;
    }

  /* Check decoded length */
  g_assert_cmpint (decoded_len, ==, length);
  /* Check decoded data */
  g_assert (memcmp (data, data2, length) == 0);

  g_free (text);
  g_free (data2);
}

static void
test_incremental_break (gconstpointer d)
{
  gint length = GPOINTER_TO_INT (d);

  test_incremental (TRUE, length);
}

static void
test_incremental_nobreak (gconstpointer d)
{
  gint length = GPOINTER_TO_INT (d);

  test_incremental (FALSE, length);
}

static void
test_full (gconstpointer d)
{
  gint length = GPOINTER_TO_INT (d);
  char *text;
  guchar *data2;
  gsize len;

  text = g_base64_encode (data, length);
  data2 = g_base64_decode (text, &len);
  g_free (text);

  /* Check decoded length */
  g_assert_cmpint (len, ==, length);
  /* Check decoded base64 data */
  g_assert (memcmp (data, data2, length) == 0);

  g_free (data2);
}

int
main (int argc, char *argv[])
{
  gint i;

  g_test_init (&argc, &argv, NULL);

  for (i = 0; i < DATA_SIZE; i++)
    data[i] = (guchar)i;

  g_test_add_data_func ("/base64/full/1", GINT_TO_POINTER (DATA_SIZE), test_full);
  g_test_add_data_func ("/base64/full/2", GINT_TO_POINTER (1), test_full);
  g_test_add_data_func ("/base64/full/3", GINT_TO_POINTER (2), test_full);
  g_test_add_data_func ("/base64/full/4", GINT_TO_POINTER (3), test_full);

  g_test_add_data_func ("/base64/incremental/nobreak/1", GINT_TO_POINTER (DATA_SIZE), test_incremental_nobreak);
  g_test_add_data_func ("/base64/incremental/break/1", GINT_TO_POINTER (DATA_SIZE), test_incremental_break);

  g_test_add_data_func ("/base64/incremental/nobreak/2", GINT_TO_POINTER (DATA_SIZE - 1), test_incremental_nobreak);
  g_test_add_data_func ("/base64/incremental/break/2", GINT_TO_POINTER (DATA_SIZE - 1), test_incremental_break);

  g_test_add_data_func ("/base64/incremental/nobreak/3", GINT_TO_POINTER (DATA_SIZE - 2), test_incremental_nobreak);
  g_test_add_data_func ("/base64/incremental/break/3", GINT_TO_POINTER (DATA_SIZE - 2), test_incremental_break);

  g_test_add_data_func ("/base64/incremental/nobreak/4", GINT_TO_POINTER (1), test_incremental_nobreak);
  g_test_add_data_func ("/base64/incremental/nobreak/4", GINT_TO_POINTER (2), test_incremental_nobreak);
  g_test_add_data_func ("/base64/incremental/nobreak/4", GINT_TO_POINTER (3), test_incremental_nobreak);

  return g_test_run ();
}
