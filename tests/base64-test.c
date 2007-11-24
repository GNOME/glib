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
  if (len > max)
    {
      g_print ("Too long encoded length: got %d, expected max %d\n",
	       len, max);
      exit (1);
    }

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
 
  if (decoded_len != length)
    {
      g_print ("Wrong decoded length: got %d, expected %d\n",
	       decoded_len, length);
      exit (1);
    }

  if (memcmp (data, data2, length) != 0)
    {
      g_print ("Wrong decoded base64 data\n");
      exit (1);
    }

  g_free (text);
  g_free (data2);
}

static void
test_full (gint length)
{
  char *text;
  guchar *data2;
  gsize len;

  text = g_base64_encode (data, length);
  data2 = g_base64_decode (text, &len);
  g_free (text);

  if (len != length)
    {
      g_print ("Wrong decoded length: got %d, expected %d\n",
	       len, length);
      exit (1);
    }

  if (memcmp (data, data2, length) != 0)
    {
      g_print ("Wrong decoded base64 data\n");
      exit (1);
    }

  g_free (data2);
}

int
main (int argc, char *argv[])
{
  int i;
  for (i = 0; i < DATA_SIZE; i++)
    data[i] = (guchar)i;

  test_full (DATA_SIZE);
  test_full (1);
  test_full (2);
  test_full (3);

  test_incremental (FALSE, DATA_SIZE);
  test_incremental (TRUE, DATA_SIZE);

  test_incremental (FALSE, DATA_SIZE - 1);
  test_incremental (TRUE, DATA_SIZE - 1);

  test_incremental (FALSE, DATA_SIZE - 2);
  test_incremental (TRUE, DATA_SIZE - 2);

  test_incremental (FALSE, 1);
  test_incremental (FALSE, 2);
  test_incremental (FALSE, 3);

  return 0;
}
