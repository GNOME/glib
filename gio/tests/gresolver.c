/*
 * Copyright (c) 2021 Igalia S.L.
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
 * Authors: Patrick Griffis <pgriffis@igalia.com>
 */

#include "config.h"

#include <glib.h>
#include <gio/gnetworking.h>

#define GIO_COMPILATION
#include "gthreadedresolver.h"
#undef GIO_COMPILATION

#ifdef HAVE_DN_COMP
static void
dns_builder_add_uint8 (GByteArray *builder,
                       guint8      value)
{
  g_byte_array_append (builder, &value, 1);
}

static void
dns_builder_add_uint16 (GByteArray *builder,
                        guint16     value)
{
    dns_builder_add_uint8 (builder, (value >> 8)  & 0xFF);
    dns_builder_add_uint8 (builder, (value)       & 0xFF);
}

static void
dns_builder_add_uint32 (GByteArray *builder,
                        guint32     value)
{
    dns_builder_add_uint8 (builder, (value >> 24) & 0xFF);
    dns_builder_add_uint8 (builder, (value >> 16) & 0xFF);
    dns_builder_add_uint8 (builder, (value >> 8)  & 0xFF);
    dns_builder_add_uint8 (builder, (value)       & 0xFF);
}

static void
dns_builder_add_length_prefixed_string (GByteArray *builder,
                                        const char *string)
{
    guint8 length;

    g_assert (strlen (string) <= G_MAXUINT8);

    length = (guint8) strlen (string);
    dns_builder_add_uint8 (builder, length);

    /* Don't include trailing NUL */
    g_byte_array_append (builder, (const guchar *)string, length);
}

static void
dns_builder_add_domain (GByteArray *builder,
                        const char *string)
{
  int ret;
  guchar buffer[256];

  ret = dn_comp (string, buffer, sizeof (buffer), NULL, NULL);
  g_assert (ret != -1);

  g_byte_array_append (builder, buffer, ret);
}

static void
dns_builder_add_answer_data (GByteArray *builder,
                             GByteArray *answer)
{
  dns_builder_add_uint16 (builder, answer->len); /* rdlength */
  g_byte_array_append (builder, answer->data, answer->len);
}
#endif /* HAVE_DN_COMP */

typedef struct
{
  GByteArray *answer;
} TestData;

static void
dns_test_setup (TestData      *fixture,
                gconstpointer  user_data)
{
  fixture->answer = g_byte_array_sized_new (2046);

#ifdef HAVE_DN_COMP
  /* Start with a header, we ignore everything except ancount.
     https://datatracker.ietf.org/doc/html/rfc1035#section-4.1.1 */
  dns_builder_add_uint16 (fixture->answer, 0); /* ID */
  dns_builder_add_uint16 (fixture->answer, 0); /* |QR|   Opcode  |AA|TC|RD|RA|   Z    |   RCODE   | */
  dns_builder_add_uint16 (fixture->answer, 0); /* QDCOUNT */
  dns_builder_add_uint16 (fixture->answer, 1); /* ANCOUNT (1 answer) */
  dns_builder_add_uint16 (fixture->answer, 0); /* NSCOUNT */
  dns_builder_add_uint16 (fixture->answer, 0); /* ARCOUNT */

  /* Answer section */
  dns_builder_add_domain (fixture->answer, "example.org");
  dns_builder_add_uint16 (fixture->answer, 65); /* type=HTTPS */
  dns_builder_add_uint16 (fixture->answer, 1); /* qclass=C_IN */
  dns_builder_add_uint32 (fixture->answer, 0); /* ttl (ignored) */
  /* Next one will be rdlength which is test specific. */
#endif
}

static void
dns_test_teardown (TestData      *fixture,
                   gconstpointer  user_data)
{
  g_byte_array_free (fixture->answer, TRUE);
}

static void
test_https_alias (TestData      *fixture,
                  gconstpointer  user_data)
{
#ifndef HAVE_DN_COMP
  g_test_skip ("The dn_comp() function was not available.");
  return;
#else
  GList *records;
  GByteArray *https_answer = g_byte_array_sized_new (1024);
  guint16 priority;
  const char *alias;
  GError *error = NULL;

  dns_builder_add_uint16 (https_answer, 0); /* priority */
  dns_builder_add_length_prefixed_string (https_answer, "foo.example.org"); /* alias target */

  dns_builder_add_answer_data (fixture->answer, https_answer);
  records = g_resolver_records_from_res_query ("example.org", 65,
                                               (guchar*)fixture->answer->data,
                                               fixture->answer->len,
                                               0, &error);

  g_assert_no_error (error);
  g_assert_cmpuint (g_list_length (records), ==, 1);
  g_variant_get (records->data, "(q&sa{sv})", &priority, &alias, NULL);

  g_assert_cmpuint (priority, ==, 0);
  g_assert_cmpstr (alias, ==, "foo.example.org.");

  g_list_free_full (records, (GDestroyNotify)g_variant_unref);
  g_byte_array_free (https_answer, TRUE);
#endif /* HAVE_DN_COMP */
}

static void
test_https_service (TestData      *fixture,
                    gconstpointer  user_data)
{
#ifndef HAVE_DN_COMP
  g_test_skip ("The dn_comp() function was not available.");
  return;
#else
  GList *records;
  GByteArray *https_answer = g_byte_array_sized_new (1024);
  guint16 priority;
  const char *target;
  GVariant *params;
  guint16 port;
  GVariantDict *dict;
  GError *error = NULL;
  const char **alpn, **mandatory;
  GVariant *key123_variant;
  const char *key123;
  gsize key123_size;

  dns_builder_add_uint16 (https_answer, 1); /* priority */
  dns_builder_add_length_prefixed_string (https_answer, ""); /* target */

  dns_builder_add_uint16 (https_answer, 3); /* SVCB key "port" */
  dns_builder_add_uint16 (https_answer, 2); /* Value length */
  dns_builder_add_uint16 (https_answer, 4443); /* SVCB value */

  dns_builder_add_uint16 (https_answer, 0); /* SVCB key "mandatory" */
  dns_builder_add_uint16 (https_answer, 2); /* Value length */
  dns_builder_add_uint16 (https_answer, 3); /* SVCB value */

  dns_builder_add_uint16 (https_answer, 1); /* SVCB key "alpn" */
  dns_builder_add_uint16 (https_answer, 3); /* Value length */
  dns_builder_add_length_prefixed_string (https_answer, "h2");

  dns_builder_add_uint16 (https_answer, 123); /* SVCB key "key123" */
  dns_builder_add_uint16 (https_answer, 4); /* Value length */
  dns_builder_add_length_prefixed_string (https_answer, "idk");

  dns_builder_add_answer_data (fixture->answer, https_answer);
  records = g_resolver_records_from_res_query ("example.org", 65,
                                               (guchar *)fixture->answer->data,
                                               fixture->answer->len,
                                               0, &error);

  g_assert_no_error (error);
  g_assert_cmpuint (g_list_length (records), ==, 1);
  g_variant_get (records->data, "(q&s@a{sv})", &priority, &target, &params);

  g_assert_cmpuint (priority, ==, 1);
  g_assert_cmpstr (target, ==, ".");
  g_assert_true (g_variant_is_of_type (params, G_VARIANT_TYPE_VARDICT));
  dict = g_variant_dict_new (params);
  g_assert_true (g_variant_dict_lookup (dict, "port", "q", &port));
  g_assert_cmpuint (port, ==, 4443);
  g_assert_true (g_variant_dict_lookup (dict, "alpn", "^a&s", &alpn));
  g_assert_cmpstr (alpn[0], ==, "h2");
  g_assert_true (g_variant_dict_lookup (dict, "mandatory", "^a&s", &mandatory));
  g_assert_cmpstr (mandatory[0], ==, "port");
  g_assert_true (g_variant_dict_lookup (dict, "key123", "@ay", &key123_variant));
  key123 = g_variant_get_fixed_array (key123_variant, &key123_size, 1);
  g_assert_true (!strncmp (key123 + 1, "idk", key123_size - 1));

  g_variant_dict_unref (dict);
  g_variant_unref (params);
  g_list_free_full (records, (GDestroyNotify)g_variant_unref);
  g_byte_array_free (https_answer, TRUE);
#endif
}

static void
test_https_invalid_1 (TestData      *fixture,
                      gconstpointer  user_data)
{
#ifndef HAVE_DN_COMP
  g_test_skip ("The dn_comp() function was not available.");
  return;
#else
  GList *records;
  GByteArray *https_answer = g_byte_array_sized_new (1024);
  GError *error = NULL;

  dns_builder_add_uint16 (https_answer, 1); /* priority */
  dns_builder_add_length_prefixed_string (https_answer, ""); /* target */

  /* Invalid value length is too long and will be caught. */
  dns_builder_add_uint16 (https_answer, 3); /* SVCB key "port" */
  dns_builder_add_uint16 (https_answer, 100); /* Value length */
  dns_builder_add_uint16 (https_answer, 4443); /* SVCB value */

  dns_builder_add_answer_data (fixture->answer, https_answer);
  records = g_resolver_records_from_res_query ("example.org", 65,
                                               (guchar *)fixture->answer->data,
                                               fixture->answer->len,
                                               0, &error);

  g_assert_error (error, G_RESOLVER_ERROR, G_RESOLVER_ERROR_INTERNAL);
  g_assert_null (records);

  g_error_free (error);
  g_byte_array_free (https_answer, TRUE);
#endif
}

static void
test_https_invalid_2 (TestData      *fixture,
                      gconstpointer  user_data)
{
#ifndef HAVE_DN_COMP
  g_test_skip ("The dn_comp() function was not available.");
  return;
#else
  GList *records;
  GByteArray *https_answer = g_byte_array_sized_new (1024);
  GError *error = NULL;

  dns_builder_add_uint16 (https_answer, 1); /* priority */
  dns_builder_add_length_prefixed_string (https_answer, ""); /* target */

  /* Within a SVCB value, having invalid length will also be caught. */
  dns_builder_add_uint16 (https_answer, 5); /* SVCB key "ECH" */
  dns_builder_add_uint16 (https_answer, 2); /* Value length */
  dns_builder_add_uint16 (https_answer, 1000); /* SVCB value (prefixed string, invalid length) */

  dns_builder_add_answer_data (fixture->answer, https_answer);
  records = g_resolver_records_from_res_query ("example.org", 65,
                                               (guchar *)fixture->answer->data,
                                               fixture->answer->len,
                                               0, &error);

  g_assert_error (error, G_RESOLVER_ERROR, G_RESOLVER_ERROR_INTERNAL);
  g_assert_null (records);

  g_error_free (error);
  g_byte_array_free (https_answer, TRUE);
#endif
}

static void
test_https_invalid_3 (TestData      *fixture,
                      gconstpointer  user_data)
{
#ifndef HAVE_DN_COMP
  g_test_skip ("The dn_comp() function was not available.");
  return;
#else
  GList *records;
  GByteArray *https_answer = g_byte_array_sized_new (1024);
  GError *error = NULL;

  dns_builder_add_uint16 (https_answer, 0); /* priority */

  /* Creating an invalid target string will be caught. */
  dns_builder_add_uint8 (https_answer, 100);
  g_byte_array_append (https_answer, (const guchar *)"test", 4);

  dns_builder_add_answer_data (fixture->answer, https_answer);
  records = g_resolver_records_from_res_query ("example.org", 65,
                                               (guchar *)fixture->answer->data,
                                               fixture->answer->len,
                                               0, &error);

  g_assert_error (error, G_RESOLVER_ERROR, G_RESOLVER_ERROR_INTERNAL);
  g_assert_null (records);

  g_error_free (error);
  g_byte_array_free (https_answer, TRUE);
#endif
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add ("/gresolver/https/alias", TestData, NULL, dns_test_setup, test_https_alias, dns_test_teardown);
  g_test_add ("/gresolver/https/service", TestData, NULL, dns_test_setup, test_https_service, dns_test_teardown);
  g_test_add ("/gresolver/https/invalid/1", TestData, NULL, dns_test_setup, test_https_invalid_1, dns_test_teardown);
  g_test_add ("/gresolver/https/invalid/2", TestData, NULL, dns_test_setup, test_https_invalid_2, dns_test_teardown);
  g_test_add ("/gresolver/https/invalid/3", TestData, NULL, dns_test_setup, test_https_invalid_3, dns_test_teardown);

  return g_test_run ();
}
