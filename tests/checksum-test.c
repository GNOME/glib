#include "config.h"

#include <glib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>

#define FIXED_STR               "The quick brown fox jumps over the lazy dog"
#define FIXED_LEN               (strlen (FIXED_STR))

#define MD5_FIXED_SUM           "9e107d9d372bb6826bd81d3542a419d6"
#define SHA1_FIXED_SUM          "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12"
#define SHA256_FIXED_SUM        "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592"

#define BLOCK_SIZE      256

static gchar *
digest_to_string (guint8 *digest,
                  gsize   digest_len)
{
  static const gchar hex_digits[] = "0123456789abcdef";
  gint len = digest_len * 2;
  gchar *retval;
  gint i;

  retval = g_new (gchar, len + 1);

  for (i = 0; i < digest_len; i++)
    {
      retval[2 * i] = hex_digits[digest[i] >> 4];
      retval[2 * i + 1] = hex_digits[digest[i] & 0xf];
    }

  retval[len] = 0;

  return retval;
}

static void
test_checksum (GChecksumType  checksum_type,
               const gchar   *type,
               const gchar   *sum,
               const gchar   *filename)
{
  GChecksum *checksum0, *checksum1, *checksum2;
  gchar *data;
  guchar *p;
  gsize data_len;
  guint8 *digest1, *digest2;
  gsize len1, len2;
  gchar *digest_str1, *digest_str2;

  checksum0 = g_checksum_new (checksum_type);
  g_checksum_update (checksum0, (const guchar *) FIXED_STR, FIXED_LEN);
  if (strcmp (g_checksum_get_string (checksum0), sum) != 0)
    {
      g_print ("Invalid %s checksum for `%s': %s (expecting: %s)\n",
               type,
               FIXED_STR,
               g_checksum_get_string (checksum0),
               sum);
      exit (1);
    }

  g_checksum_free (checksum0);

  checksum1 = g_checksum_new (checksum_type);
  checksum2 = g_checksum_new (checksum_type);

  if (!g_file_get_contents (filename, &data, &data_len, NULL))
    {
      g_print ("Could not load `%s' contents\n", filename);
      exit (1);
    }

  g_checksum_update (checksum1, (const guchar *) data, data_len);

  p = (guchar *) data;
  do
    {
      if (data_len > BLOCK_SIZE)
        {
          g_checksum_update (checksum2, p, BLOCK_SIZE);
          data_len -= BLOCK_SIZE;
          p += BLOCK_SIZE;
        }
      else
        {
          g_checksum_update (checksum2, p, data_len);
          break;
        }
    }
  while (*p != '\0');
 
  g_free (data);

  digest1 = NULL;
  g_checksum_get_digest (checksum1, &digest1, &len1);
  if (!digest1)
    {
      g_print ("No %s digest found for checksum1\n", type);
      exit (1);
    }

  digest2 = NULL;
  g_checksum_get_digest (checksum2, &digest2, &len2);
  if (!digest2)
    {
      g_print ("No %s digest found for checksum2\n", type);
      exit (1);
    }
  
  digest_str1 = digest_to_string (digest1, len1);
  digest_str2 = digest_to_string (digest2, len2);

  if (strcmp (digest_str1, digest_str2) != 0)
    {
      g_print ("Wrong %s digest `%s' (expecting: %s)\n",
               type,
               digest_str1,
               digest_str2);
      exit (1);
    }

  g_free (digest_str1);
  g_free (digest_str2);
  g_free (digest1);
  g_free (digest2);

  digest_str1 = g_strdup (g_checksum_get_string (checksum1));
  digest_str2 = g_strdup (g_checksum_get_string (checksum2));

  if (!digest_str1 || !digest_str2)
    {
      g_print ("No %s digest string found\n", type);
      exit (1);
    }

  if (strcmp (digest_str1, digest_str2) != 0)
    {
      g_print ("Wrong %s digest string `%s' (expecting: %s)\n",
               type,
               digest_str1,
               digest_str2);
      exit (1);
    }

  g_free (digest_str1);
  g_free (digest_str2);

  g_checksum_free (checksum1);
  g_checksum_free (checksum2);
}

#define test(type)      test_checksum (G_CHECKSUM_##type, \
                                       #type, \
                                       type##_FIXED_SUM, \
                                       __FILE__)

int
main (int argc, char *argv[])
{
  test (MD5);
  test (SHA1);
  test (SHA256);

  return EXIT_SUCCESS;
}
