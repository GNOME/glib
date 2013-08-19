#include <gio/gio.h>

static void
test_basic (void)
{
  GNetworkAddress *address;
  guint port;
  gchar *hostname;
  gchar *scheme;

  address = (GNetworkAddress*)g_network_address_new ("www.gnome.org", 8080);

  g_assert_cmpstr (g_network_address_get_hostname (address), ==, "www.gnome.org");
  g_assert_cmpint (g_network_address_get_port (address), ==, 8080);

  g_object_get (address, "hostname", &hostname, "port", &port, "scheme", &scheme, NULL);
  g_assert_cmpstr (hostname, ==, "www.gnome.org");
  g_assert_cmpint (port, ==, 8080);
  g_assert (scheme == NULL);
  g_free (hostname);

  g_object_unref (address);
}

typedef struct {
  const gchar *input;
  const gchar *scheme;
  const gchar *hostname;
  guint16 port;
  gint error_code;
} ParseTest;

static ParseTest uri_tests[] = {
  { "http://www.gnome.org:2020/start", "http", "www.gnome.org", 2020, -1 },
  { "ftp://joe~:(*)%46@ftp.gnome.org:2020/start", "ftp", "ftp.gnome.org", 2020, -1 },
  { "ftp://[fec0::abcd]/start", "ftp", "fec0::abcd", 8080, -1 },
  { "ftp://[fec0::abcd]:999/start", "ftp", "fec0::abcd", 999, -1 },
  { "ftp://joe%x-@ftp.gnome.org:2020/start", NULL, NULL, 0, G_IO_ERROR_INVALID_ARGUMENT }
};

static void
test_parse_uri (gconstpointer d)
{
  const ParseTest *test = d;
  GNetworkAddress *address;
  GError *error;

  error = NULL;
  address = (GNetworkAddress*)g_network_address_parse_uri (test->input, 8080, &error);

  if (address)
    {
      g_assert_cmpstr (g_network_address_get_scheme (address), ==, test->scheme);
      g_assert_cmpstr (g_network_address_get_hostname (address), ==, test->hostname);
      g_assert_cmpint (g_network_address_get_port (address), ==, test->port);
      g_assert_no_error (error);
    }
  else
    g_assert_error (error, G_IO_ERROR, test->error_code);

  if (address)
    g_object_unref (address);
  if (error)
    g_error_free (error);
}

static ParseTest host_tests[] =
{
  { "www.gnome.org", NULL, "www.gnome.org", 1234, -1 },
  { "www.gnome.org:8080", NULL, "www.gnome.org", 8080, -1 },
  { "[2001:db8::1]", NULL, "2001:db8::1", 1234, -1 },
  { "[2001:db8::1]:888", NULL, "2001:db8::1", 888, -1 },
  { "[2001:db8::1%em1]", NULL, "2001:db8::1%em1", 1234, -1 },
  { "[hostname", NULL, NULL, 0, G_IO_ERROR_INVALID_ARGUMENT },
  { "[hostnam]e", NULL, NULL, 0, G_IO_ERROR_INVALID_ARGUMENT },
  { "hostname:", NULL, NULL, 0, G_IO_ERROR_INVALID_ARGUMENT },
  { "hostname:-1", NULL, NULL, 0, G_IO_ERROR_INVALID_ARGUMENT },
  { "hostname:9999999", NULL, NULL, 0, G_IO_ERROR_INVALID_ARGUMENT }
};

static void
test_parse_host (gconstpointer d)
{
  const ParseTest *test = d;
  GNetworkAddress *address;
  GError *error;

  error = NULL;
  address = (GNetworkAddress*)g_network_address_parse (test->input, 1234, &error);

  if (address)
    {
      g_assert_null (g_network_address_get_scheme (address));
      g_assert_cmpstr (g_network_address_get_hostname (address), ==, test->hostname);
      g_assert_cmpint (g_network_address_get_port (address), ==, test->port);
      g_assert_no_error (error);
    }
  else
    {
      g_assert_error (error, G_IO_ERROR, test->error_code);
    }

  if (address)
    g_object_unref (address);
  if (error)
    g_error_free (error);
}

int
main (int argc, char *argv[])
{
  gint i;
  gchar *path;

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/network-address/basic", test_basic);

  for (i = 0; i < G_N_ELEMENTS (host_tests); i++)
    {
      path = g_strdup_printf ("/network-address/parse-host/%d", i);
      g_test_add_data_func (path, &host_tests[i], test_parse_host);
      g_free (path);
    }

  for (i = 0; i < G_N_ELEMENTS (uri_tests); i++)
    {
      path = g_strdup_printf ("/network-address/parse-uri/%d", i);
      g_test_add_data_func (path, &uri_tests[i], test_parse_uri);
      g_free (path);
    }

  return g_test_run ();
}
