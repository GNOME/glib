#include "config.h"
#include "mock-resolver.h"

#include <gio/gio.h>
#include <gio/gnetworking.h>

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
  { "ftp://joe%x-@ftp.gnome.org:2020/start", NULL, NULL, 0, G_IO_ERROR_INVALID_ARGUMENT },
  { "http://[fec0::abcd%em1]/start", "http", "fec0::abcd%em1", 8080, -1 },
  { "http://[fec0::abcd%25em1]/start", "http", "fec0::abcd%em1", 8080, -1 },
  { "http://[fec0::abcd%10]/start", "http", "fec0::abcd%10", 8080, -1 },
  { "http://[fec0::abcd%25em%31]/start", NULL, NULL, 0, G_IO_ERROR_INVALID_ARGUMENT },
  { "ftp://ftp.gnome.org/start?foo=bar@baz", "ftp", "ftp.gnome.org", 8080, -1 }
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

typedef struct {
  const gchar *input;
  gboolean valid_parse, valid_resolve, valid_ip;
} ResolveTest;

static ResolveTest address_tests[] = {
  { "192.168.1.2",         TRUE,  TRUE,  TRUE },
  { "fe80::42",            TRUE,  TRUE,  TRUE },

  /* g_network_address_parse() accepts these, but they are not
   * (just) IP addresses.
   */
  { "192.168.1.2:80",      TRUE,  FALSE, FALSE },
  { "[fe80::42]",          TRUE,  FALSE, FALSE },
  { "[fe80::42]:80",       TRUE,  FALSE, FALSE },

  /* These should not be considered IP addresses by anyone. */
  { "192.168.258",         FALSE, FALSE, FALSE },
  { "192.11010306",        FALSE, FALSE, FALSE },
  { "3232235778",          FALSE, FALSE, FALSE },
  { "0300.0250.0001.0001", FALSE, FALSE, FALSE },
  { "0xC0.0xA8.0x01.0x02", FALSE, FALSE, FALSE },
  { "0xc0.0xa8.0x01.0x02", FALSE, FALSE, FALSE },
  { "0xc0a80102",          FALSE, FALSE, FALSE }
};

static void
test_resolve_address (gconstpointer d)
{
  const ResolveTest *test = d;
  GSocketConnectable *connectable;
  GSocketAddressEnumerator *addr_enum;
  GSocketAddress *addr;
  GError *error = NULL;

  g_test_message ("Input: %s", test->input);

  g_assert_cmpint (test->valid_ip, ==, g_hostname_is_ip_address (test->input));

  connectable = g_network_address_parse (test->input, 1234, &error);
  g_assert_no_error (error);

  addr_enum = g_socket_connectable_enumerate (connectable);
  addr = g_socket_address_enumerator_next (addr_enum, NULL, &error);
  g_object_unref (addr_enum);
  g_object_unref (connectable);

  if (addr)
    {
      g_assert_true (test->valid_parse);
      g_assert_true (G_IS_INET_SOCKET_ADDRESS (addr));
      g_object_unref (addr);
    }
  else
    {
      g_assert_false (test->valid_parse);
      g_assert_error (error, G_RESOLVER_ERROR, G_RESOLVER_ERROR_NOT_FOUND);
      g_error_free (error);
      return;
    }
}

/* Technically this should be in a GResolver test program, but we don't
 * have one of those since it's mostly impossible to test programmatically.
 * So it goes here so it can share the tests.
 */
static void
test_resolve_address_gresolver (gconstpointer d)
{
  const ResolveTest *test = d;
  GResolver *resolver;
  GList *addrs;
  GInetAddress *iaddr;
  GError *error = NULL;

  g_test_message ("Input: %s", test->input);

  resolver = g_resolver_get_default ();
  addrs = g_resolver_lookup_by_name (resolver, test->input, NULL, &error);
  g_object_unref (resolver);

  if (addrs)
    {
      g_assert_true (test->valid_resolve);
      g_assert_cmpint (g_list_length (addrs), ==, 1);

      iaddr = addrs->data;
      g_assert_true (G_IS_INET_ADDRESS (iaddr));

      g_object_unref (iaddr);
      g_list_free (addrs);
    }
  else
    {
      g_assert_nonnull (error);
      g_test_message ("Error: %s", error->message);
      g_assert_false (test->valid_resolve);

      if (!test->valid_parse)
        {
          /* GResolver should have rejected the address internally, in
           * which case we're guaranteed to get G_RESOLVER_ERROR_NOT_FOUND.
           */
          g_assert_error (error, G_RESOLVER_ERROR, G_RESOLVER_ERROR_NOT_FOUND);
        }
      else
        {
          /* If GResolver didn't reject the string itself, then we
           * might have attempted to send it over the network. If that
           * attempt succeeded, we'd get back NOT_FOUND, but if
           * there's no network available we might have gotten some
           * other error instead.
           */
        }

      g_error_free (error);
      return;
    }
}

#define SCOPE_ID_TEST_ADDR "fe80::42"
#define SCOPE_ID_TEST_PORT 99

#if defined (HAVE_IF_INDEXTONAME) && defined (HAVE_IF_NAMETOINDEX)
static char SCOPE_ID_TEST_IFNAME[IF_NAMESIZE];
static int SCOPE_ID_TEST_INDEX;
#else
#define SCOPE_ID_TEST_IFNAME "1"
#define SCOPE_ID_TEST_INDEX 1
#endif

static void
find_ifname_and_index (void)
{
  if (SCOPE_ID_TEST_INDEX != 0)
    return;

#if defined (HAVE_IF_INDEXTONAME) && defined (HAVE_IF_NAMETOINDEX)
  SCOPE_ID_TEST_INDEX = if_nametoindex ("lo");
  if (SCOPE_ID_TEST_INDEX != 0)
    {
      g_strlcpy (SCOPE_ID_TEST_IFNAME, "lo", sizeof (SCOPE_ID_TEST_IFNAME));
      return;
    }

  for (SCOPE_ID_TEST_INDEX = 1; SCOPE_ID_TEST_INDEX < 1024; SCOPE_ID_TEST_INDEX++) {
    if (if_indextoname (SCOPE_ID_TEST_INDEX, SCOPE_ID_TEST_IFNAME))
      break;
  }
  g_assert_cmpstr (SCOPE_ID_TEST_IFNAME, !=, "");
#endif
}

static void
test_scope_id (GSocketConnectable *addr)
{
  GSocketAddressEnumerator *addr_enum;
  GSocketAddress *saddr;
  GInetSocketAddress *isaddr;
  GInetAddress *iaddr;
  char *tostring;
  GError *error = NULL;

  addr_enum = g_socket_connectable_enumerate (addr);
  saddr = g_socket_address_enumerator_next (addr_enum, NULL, &error);
  g_assert_no_error (error);

  g_assert (saddr != NULL);
  g_assert (G_IS_INET_SOCKET_ADDRESS (saddr));

  isaddr = G_INET_SOCKET_ADDRESS (saddr);
  g_assert_cmpint (g_inet_socket_address_get_scope_id (isaddr), ==, SCOPE_ID_TEST_INDEX);
  g_assert_cmpint (g_inet_socket_address_get_port (isaddr), ==, SCOPE_ID_TEST_PORT);

  iaddr = g_inet_socket_address_get_address (isaddr);
  tostring = g_inet_address_to_string (iaddr);
  g_assert_cmpstr (tostring, ==, SCOPE_ID_TEST_ADDR);
  g_free (tostring);

  g_object_unref (saddr);
  saddr = g_socket_address_enumerator_next (addr_enum, NULL, &error);
  g_assert_no_error (error);
  g_assert (saddr == NULL);

  g_object_unref (addr_enum);
}

static void
test_host_scope_id (void)
{
  GSocketConnectable *addr;
  char *str;

  find_ifname_and_index ();

  str = g_strdup_printf ("%s%%%s", SCOPE_ID_TEST_ADDR, SCOPE_ID_TEST_IFNAME);
  addr = g_network_address_new (str, SCOPE_ID_TEST_PORT);
  g_free (str);

  test_scope_id (addr);
  g_object_unref (addr);
}

static void
test_uri_scope_id (void)
{
  GSocketConnectable *addr;
  char *uri;
  GError *error = NULL;

  find_ifname_and_index ();

  uri = g_strdup_printf ("http://[%s%%%s]:%d/foo",
                         SCOPE_ID_TEST_ADDR,
                         SCOPE_ID_TEST_IFNAME,
                         SCOPE_ID_TEST_PORT);
  addr = g_network_address_parse_uri (uri, 0, &error);
  g_free (uri);
  g_assert_no_error (error);

  test_scope_id (addr);
  g_object_unref (addr);

  uri = g_strdup_printf ("http://[%s%%25%s]:%d/foo",
                         SCOPE_ID_TEST_ADDR,
                         SCOPE_ID_TEST_IFNAME,
                         SCOPE_ID_TEST_PORT);
  addr = g_network_address_parse_uri (uri, 0, &error);
  g_free (uri);
  g_assert_no_error (error);

  test_scope_id (addr);
  g_object_unref (addr);
}

static void
test_loopback_basic (void)
{
  GNetworkAddress *addr;  /* owned */

  addr = G_NETWORK_ADDRESS (g_network_address_new_loopback (666));

  /* Test basic properties. */
  g_assert_cmpstr (g_network_address_get_hostname (addr), ==, "localhost");
  g_assert_cmpuint (g_network_address_get_port (addr), ==, 666);
  g_assert_null (g_network_address_get_scheme (addr));

  g_object_unref (addr);
}

static void
assert_socket_address_matches (GSocketAddress *a,
                               const gchar    *expected_address,
                               guint16         expected_port)
{
  GInetSocketAddress *sa;
  gchar *str;  /* owned */

  g_assert (G_IS_INET_SOCKET_ADDRESS (a));

  sa = G_INET_SOCKET_ADDRESS (a);
  g_assert_cmpint (g_inet_socket_address_get_port (sa), ==, expected_port);

  str = g_inet_address_to_string (g_inet_socket_address_get_address (sa));
  g_assert_cmpstr (str, ==, expected_address);
  g_free (str);
}

static void
test_loopback_sync (void)
{
  GSocketConnectable *addr;  /* owned */
  GSocketAddressEnumerator *enumerator;  /* owned */
  GSocketAddress *a;  /* owned */
  GError *error = NULL;

  addr = g_network_address_new_loopback (616);
  enumerator = g_socket_connectable_enumerate (addr);

  /* IPv6 address. */
  a = g_socket_address_enumerator_next (enumerator, NULL, &error);
  g_assert_no_error (error);
  assert_socket_address_matches (a, "::1", 616);
  g_object_unref (a);

  /* IPv4 address. */
  a = g_socket_address_enumerator_next (enumerator, NULL, &error);
  g_assert_no_error (error);
  assert_socket_address_matches (a, "127.0.0.1", 616);
  g_object_unref (a);

  /* End of results. */
  g_assert_null (g_socket_address_enumerator_next (enumerator, NULL, &error));
  g_assert_no_error (error);

  g_object_unref (enumerator);
  g_object_unref (addr);
}

typedef struct {
  GList/*<owned GSocketAddress> */ *addrs;  /* owned */
  GMainLoop *loop;  /* owned */
  guint delay_ms;
  gint expected_error_code;
} AsyncData;

static void
got_addr (GObject *source_object, GAsyncResult *result, gpointer user_data)
{
  GSocketAddressEnumerator *enumerator;
  AsyncData *data;
  GSocketAddress *a;  /* owned */
  GError *error = NULL;

  enumerator = G_SOCKET_ADDRESS_ENUMERATOR (source_object);
  data = user_data;

  a = g_socket_address_enumerator_next_finish (enumerator, result, &error);

  if (data->expected_error_code)
    {
      g_assert_error (error, G_IO_ERROR, data->expected_error_code);
      g_error_free (error);
    }
  else
    g_assert_no_error (error);

  if (a == NULL)
    {
      /* End of results. */
      data->addrs = g_list_reverse (data->addrs);
      g_main_loop_quit (data->loop);
    }
  else
    {
      g_assert (G_IS_INET_SOCKET_ADDRESS (a));
      data->addrs = g_list_prepend (data->addrs, a);

      if (data->delay_ms)
        g_usleep (data->delay_ms * 1000);

      g_socket_address_enumerator_next_async (enumerator, NULL,
                                              got_addr, user_data);
    }
}

static void
test_loopback_async (void)
{
  GSocketConnectable *addr;  /* owned */
  GSocketAddressEnumerator *enumerator;  /* owned */
  AsyncData data = { 0, };

  addr = g_network_address_new_loopback (610);
  enumerator = g_socket_connectable_enumerate (addr);

  /* Get all the addresses. */
  data.addrs = NULL;
  data.loop = g_main_loop_new (NULL, FALSE);

  g_socket_address_enumerator_next_async (enumerator, NULL, got_addr, &data);

  g_main_loop_run (data.loop);
  g_main_loop_unref (data.loop);

  /* Check results. */
  g_assert_cmpuint (g_list_length (data.addrs), ==, 2);
  assert_socket_address_matches (data.addrs->data, "::1", 610);
  assert_socket_address_matches (data.addrs->next->data, "127.0.0.1", 610);

  g_list_free_full (data.addrs, (GDestroyNotify) g_object_unref);

  g_object_unref (enumerator);
  g_object_unref (addr);
}

static void
test_to_string (void)
{
  GSocketConnectable *addr = NULL;
  gchar *str = NULL;
  GError *error = NULL;

  /* Without port. */
  addr = g_network_address_new ("some-hostname", 0);
  str = g_socket_connectable_to_string (addr);
  g_assert_cmpstr (str, ==, "some-hostname");
  g_free (str);
  g_object_unref (addr);

  /* With port. */
  addr = g_network_address_new ("some-hostname", 123);
  str = g_socket_connectable_to_string (addr);
  g_assert_cmpstr (str, ==, "some-hostname:123");
  g_free (str);
  g_object_unref (addr);

  /* With scheme and port. */
  addr = g_network_address_parse_uri ("http://some-hostname:123", 80, &error);
  g_assert_no_error (error);
  str = g_socket_connectable_to_string (addr);
  g_assert_cmpstr (str, ==, "http:some-hostname:123");
  g_free (str);
  g_object_unref (addr);

  /* Loopback. */
  addr = g_network_address_new ("localhost", 456);
  str = g_socket_connectable_to_string (addr);
  g_assert_cmpstr (str, ==, "localhost:456");
  g_free (str);
  g_object_unref (addr);
}

static ResolveTest he_ipv4_addresses[] = {
  {.input="1.1.1.1",},
  {.input="2.2.2.2",},
};

static ResolveTest he_ipv6_addresses[] = {
  {.input="ff::11",},
  {.input="ff::22",},
};

static void
assert_list_contains (GList *list, GInetAddress *address)
{
  while (list)
    {
      if (g_inet_address_equal (list->data, address))
        return;

      list = g_list_next (list);
    }

  g_assert_not_reached ();
}

static void
assert_list_matches_expected (GList *result, GList *expected)
{
  g_assert_cmpint (g_list_length (result), ==, g_list_length (expected));

  while (result)
    {
      GInetAddress *address = g_inet_socket_address_get_address (G_INET_SOCKET_ADDRESS (result->data));
      assert_list_contains (expected, address);
      result = g_list_next (result);
    }
}

static void
test_happy_eyeballs_async (void)
{
  GResolver *old_resolver;
  MockResolver *mock;
  GSocketConnectable *addr = NULL;  /* owned */
  GSocketAddressEnumerator *enumerator = NULL;  /* owned */
  AsyncData data = { 0 };
  GList *input_ipv4_results = NULL;
  GList *input_ipv6_results = NULL;
  GList *input_all_results = NULL;
  GError *ipv6_error = NULL;
  GError *ipv4_error = NULL;
  int i;

  /* This test tries to reproduce some of the situations that
   * RFC 8305 (Happy Eyeballs v2) is designed to handle.
   */

  /* Setup mock resolver. */
  old_resolver = g_resolver_get_default ();
  mock = mock_resolver_new ();
  g_resolver_set_default (G_RESOLVER (mock));

#define CLEANUP() G_STMT_START { \
  g_list_free_full (data.addrs, (GDestroyNotify) g_object_unref); \
  g_clear_object (&enumerator); \
  g_clear_object (&addr); \
  addr = g_network_address_new ("test.fake", 80); \
  enumerator = g_socket_connectable_enumerate (addr); \
  data.addrs = NULL; \
  data.delay_ms = 0; \
  data.expected_error_code = 0; \
  g_clear_error (&ipv4_error); \
  g_clear_error (&ipv6_error); \
  mock_resolver_set_ipv6_error (mock, NULL); \
  mock_resolver_set_ipv4_error (mock, NULL); \
  mock_resolver_set_ipv4_delay (mock, 0); \
  mock_resolver_set_ipv6_delay (mock, 0); \
} G_STMT_END

  /* init */
  CLEANUP ();
  for (i = 0; i < G_N_ELEMENTS (he_ipv4_addresses); ++i)
    {
      GInetAddress *ipv4_addr = g_inet_address_new_from_string (he_ipv4_addresses[i].input);
      GInetAddress *ipv6_addr = g_inet_address_new_from_string (he_ipv6_addresses[i].input);
      input_ipv4_results = g_list_append (input_ipv4_results, ipv4_addr);
      input_ipv6_results = g_list_append (input_ipv6_results, ipv6_addr);
      input_all_results = g_list_append (input_all_results, ipv4_addr);
      input_all_results = g_list_append (input_all_results, ipv6_addr);
    }
  data.loop = g_main_loop_new (NULL, FALSE);
  mock_resolver_set_ipv4_results (mock, input_ipv4_results);
  mock_resolver_set_ipv6_results (mock, input_ipv6_results);

  /* Sanity check first */
  g_test_message ("Sanity check");
  g_socket_address_enumerator_next_async (enumerator, NULL, got_addr, &data);
  g_main_loop_run (data.loop);
  assert_list_matches_expected (data.addrs, input_all_results);
  CLEANUP ();

  /* If ipv4 dns response is a bit slow we just don't get them */
  mock_resolver_set_ipv4_delay (mock, 25);
  g_test_message ("Testing slow ipv4");
  g_socket_address_enumerator_next_async (enumerator, NULL, got_addr, &data);
  g_main_loop_run (data.loop);
  assert_list_matches_expected (data.addrs, input_ipv6_results);
  CLEANUP ();

  /* If ipv6 is a bit slow it waits for them */
  mock_resolver_set_ipv6_delay (mock, 25);
  g_test_message ("Testing slow ipv6");
  g_socket_address_enumerator_next_async (enumerator, NULL, got_addr, &data);
  g_main_loop_run (data.loop);
  assert_list_matches_expected (data.addrs, input_all_results);
  CLEANUP ();

  /* If ipv6 is very slow we don't get them */
  mock_resolver_set_ipv6_delay (mock, 200);
  g_test_message ("Testing very slow ipv6");
  g_socket_address_enumerator_next_async (enumerator, NULL, got_addr, &data);
  g_main_loop_run (data.loop);
  assert_list_matches_expected (data.addrs, input_ipv4_results);
  CLEANUP ();

  /* Even if the dns response is slow we still get them if our connection attempts
   * take long enough. */
  data.delay_ms = 500;
  mock_resolver_set_ipv4_delay (mock, 200);
  g_test_message ("Testing slow ipv4 and connection");
  g_socket_address_enumerator_next_async (enumerator, NULL, got_addr, &data);
  g_main_loop_run (data.loop);
  assert_list_matches_expected (data.addrs, input_all_results);
  CLEANUP ();

  /* If ipv6 fails we still get ipv4. */
  ipv6_error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_TIMED_OUT, "IPv6 Broken");
  mock_resolver_set_ipv6_error (mock, ipv6_error);
  g_test_message ("Testing failing ipv6");
  g_socket_address_enumerator_next_async (enumerator, NULL, got_addr, &data);
  g_main_loop_run (data.loop);
  assert_list_matches_expected (data.addrs, input_ipv4_results);
  CLEANUP ();

  /* If ipv4 fails we still get ipv6. */
  ipv4_error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_TIMED_OUT, "IPv4 Broken");
  mock_resolver_set_ipv4_error (mock, ipv4_error);
  g_test_message ("Testing failing ipv4");
  g_socket_address_enumerator_next_async (enumerator, NULL, got_addr, &data);
  g_main_loop_run (data.loop);
  assert_list_matches_expected (data.addrs, input_ipv6_results);
  CLEANUP ();

  /* If both fail we get an error. */
  ipv4_error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_TIMED_OUT, "IPv4 Broken");
  mock_resolver_set_ipv4_error (mock, ipv4_error);
  ipv6_error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_TIMED_OUT, "IPv6 Broken");
  mock_resolver_set_ipv6_error (mock, ipv6_error);
  data.expected_error_code = G_IO_ERROR_TIMED_OUT;
  g_test_message ("Testing failing ipv6 and ipv4");
  g_socket_address_enumerator_next_async (enumerator, NULL, got_addr, &data);
  g_main_loop_run (data.loop);
  assert_list_matches_expected (data.addrs, NULL);
  CLEANUP ();

  /* The same with some different timings */
  ipv4_error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_TIMED_OUT, "IPv4 Broken");
  mock_resolver_set_ipv4_error (mock, ipv4_error);
  mock_resolver_set_ipv4_delay (mock, 25);
  ipv6_error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_TIMED_OUT, "IPv6 Broken");
  mock_resolver_set_ipv6_error (mock, ipv6_error);
  data.expected_error_code = G_IO_ERROR_TIMED_OUT;
  g_test_message ("Testing failing ipv6 and slow ipv4");
  g_socket_address_enumerator_next_async (enumerator, NULL, got_addr, &data);
  g_main_loop_run (data.loop);
  assert_list_matches_expected (data.addrs, NULL);
  CLEANUP ();

  ipv4_error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_TIMED_OUT, "IPv4 Broken");
  mock_resolver_set_ipv4_error (mock, ipv4_error);
  ipv6_error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_TIMED_OUT, "IPv6 Broken");
  mock_resolver_set_ipv6_error (mock, ipv6_error);
  mock_resolver_set_ipv6_delay (mock, 25);
  data.expected_error_code = G_IO_ERROR_TIMED_OUT;
  g_test_message ("Testing failing ipv6 and slow ipv4");
  g_socket_address_enumerator_next_async (enumerator, NULL, got_addr, &data);
  g_main_loop_run (data.loop);
  assert_list_matches_expected (data.addrs, NULL);
  CLEANUP ();

  ipv4_error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_TIMED_OUT, "IPv4 Broken");
  mock_resolver_set_ipv4_error (mock, ipv4_error);
  ipv6_error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_TIMED_OUT, "IPv6 Broken");
  mock_resolver_set_ipv6_error (mock, ipv6_error);
  mock_resolver_set_ipv6_delay (mock, 200);
  data.expected_error_code = G_IO_ERROR_TIMED_OUT;
  g_test_message ("Testing failing and slow ipv6 and ipv4");
  g_socket_address_enumerator_next_async (enumerator, NULL, got_addr, &data);
  g_main_loop_run (data.loop);
  assert_list_matches_expected (data.addrs, NULL);
  CLEANUP ();

  g_main_loop_unref (data.loop);
  g_list_free_full (input_all_results, g_object_unref);
  g_list_free (input_ipv4_results);
  g_list_free (input_ipv6_results);
  g_resolver_set_default (old_resolver);
  g_object_unref (old_resolver);
  g_object_unref (mock);
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

  for (i = 0; i < G_N_ELEMENTS (address_tests); i++)
    {
      path = g_strdup_printf ("/network-address/resolve-address/%d", i);
      g_test_add_data_func (path, &address_tests[i], test_resolve_address);
      g_free (path);
    }

  for (i = 0; i < G_N_ELEMENTS (address_tests); i++)
    {
      path = g_strdup_printf ("/gresolver/resolve-address/%d", i);
      g_test_add_data_func (path, &address_tests[i], test_resolve_address_gresolver);
      g_free (path);
    }

  g_test_add_func ("/network-address/scope-id", test_host_scope_id);
  g_test_add_func ("/network-address/uri-scope-id", test_uri_scope_id);
  g_test_add_func ("/network-address/loopback/basic", test_loopback_basic);
  g_test_add_func ("/network-address/loopback/sync", test_loopback_sync);
  g_test_add_func ("/network-address/loopback/async", test_loopback_async);
  g_test_add_func ("/network-address/to-string", test_to_string);

  g_test_add_func ("/network-address/happy-eyeballs", test_happy_eyeballs_async);

  return g_test_run ();
}
