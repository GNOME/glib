#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#define DATA_SIZE 1024
#define BLOCK_SIZE 32
#define NUM_BLOCKS 32
static guchar data[DATA_SIZE];

static void
test_incremental (gboolean line_break)
{
  char *p;
  int i;
  gsize len, decoded_len, max;
  int state, save;
  guint decoder_save;
  char *text;
  guchar *data2;

  data2 = g_malloc (DATA_SIZE);
  text = g_malloc (DATA_SIZE * 2);

  len = 0;
  state = 0;
  save = 0;
  for (i = 0; i < NUM_BLOCKS; i++)
    len += g_base64_encode_step (data + i * BLOCK_SIZE, BLOCK_SIZE,
				 line_break, text + len, &state, &save);
  len += g_base64_encode_close (line_break, text + len, &state, &save);

  if (line_break)
    max = DATA_SIZE * 4 / 3 + DATA_SIZE * 4 / (3 * 72) + 7;
  else
    max = DATA_SIZE * 4 / 3 + 6;
  if (len > max)
    {
      g_print ("To long encoded length: got %d, expected max %d\n",
	       len, max);
      exit (1);
    }

  decoded_len = 0;
  state = 0;
  decoder_save = 0;
  p = text;
  while (len > 0)
    {
      int chunk_len = MAX (32, len);
      decoded_len += g_base64_decode_step (p, 
					   chunk_len, 
					   data2 + decoded_len,
					   &state, &decoder_save);
      p += chunk_len;
      len -= chunk_len;
    }
  
  if (decoded_len != DATA_SIZE)
    {
      g_print ("Wrong decoded length: got %d, expected %d\n",
	       decoded_len, DATA_SIZE);
      exit (1);
    }

  if (memcmp (data, data2, DATA_SIZE) != 0)
    {
      g_print ("Wrong decoded base64 data\n");
      exit (1);
    }

  g_free (text);
  g_free (data2);
}

static void
test_full (void)
{
  char *text;
  guchar *data2;
  gsize len;

  text = g_base64_encode (data, DATA_SIZE);
  data2 = g_base64_decode (text, &len);
  g_free (text);

  if (len != DATA_SIZE)
    {
      g_print ("Wrong decoded length: got %d, expected %d\n",
	       len, DATA_SIZE);
      exit (1);
    }

  if (memcmp (data, data2, DATA_SIZE) != 0)
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

  test_full ();
  test_incremental (FALSE);
  test_incremental (TRUE);
  
  return 0;
}
