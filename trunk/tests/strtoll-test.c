#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include <errno.h>
#include <string.h>
#include <glib.h>


static void
test_uint64 (const gchar *str,
	     const gchar *end,
	     gint         base,
	     guint64      result,
	     gint         error)
{
  guint64 actual;
  gchar *endptr = NULL;
  gint err;

  errno = 0;
  actual = g_ascii_strtoull (str, &endptr, base);
  err = errno;

  g_assert (actual == result);
  g_assert (strcmp (end, endptr) == 0);
  g_assert (err == error);
}

static void
test_int64 (const gchar *str,
	    const gchar *end,
	    gint         base,
	    gint64       result,
	    gint         error)
{
  gint64 actual;
  gchar *endptr = NULL;
  gint err;

  errno = 0;
  actual = g_ascii_strtoll (str, &endptr, base);
  err = errno;

  g_assert (actual == result);
  g_assert (strcmp (end, endptr) == 0);
  g_assert (err == error);
}

int 
main (int argc, char *argv[])
{
  test_uint64 ("0", "", 10, 0, 0);
  test_uint64 ("+0", "", 10, 0, 0);
  test_uint64 ("-0", "", 10, 0, 0);
  test_uint64 ("18446744073709551615", "", 10, G_MAXUINT64, 0);
  test_uint64 ("18446744073709551616", "", 10, G_MAXUINT64, ERANGE);
  test_uint64 ("20xyz", "xyz", 10, 20, 0);
  test_uint64 ("-1", "", 10, G_MAXUINT64, 0);

  test_int64 ("0", "", 10, 0, 0);
  test_int64 ("9223372036854775807", "", 10, G_MAXINT64, 0);
  test_int64 ("9223372036854775808", "", 10, G_MAXINT64, ERANGE);
  test_int64 ("-9223372036854775808", "", 10, G_MININT64, 0);
  test_int64 ("-9223372036854775809", "", 10, G_MININT64, ERANGE);
  test_int64 ("32768", "", 10, 32768, 0);
  test_int64 ("-32768", "", 10, -32768, 0);
  test_int64 ("001", "", 10, 1, 0);
  test_int64 ("-001", "", 10, -1, 0);

  return 0;
}
