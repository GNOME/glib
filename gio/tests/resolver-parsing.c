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

static GByteArray *
dns_header (void)
{
  GByteArray *answer = g_byte_array_sized_new (2046);

  /* Start with a header, we ignore everything except ancount.
     https://datatracker.ietf.org/doc/html/rfc1035#section-4.1.1 */
  dns_builder_add_uint16 (answer, 0); /* ID */
  dns_builder_add_uint16 (answer, 0); /* |QR|   Opcode  |AA|TC|RD|RA|   Z    |   RCODE   | */
  dns_builder_add_uint16 (answer, 0); /* QDCOUNT */
  dns_builder_add_uint16 (answer, 1); /* ANCOUNT (1 answer) */
  dns_builder_add_uint16 (answer, 0); /* NSCOUNT */
  dns_builder_add_uint16 (answer, 0); /* ARCOUNT */

  return g_steal_pointer (&answer);
}
#endif /* HAVE_DN_COMP */

static void
test_invalid_header (void)
{
  const struct
    {
      const guint8 *answer;
      gsize answer_len;
      GResolverError expected_error_code;
    }
  vectors[] =
    {
      /* No answer: */
      { (const guint8 *) "", 0, G_RESOLVER_ERROR_NOT_FOUND },
      /* Definitely too short to be a valid header: */
      { (const guint8 *) "\x20", 1, G_RESOLVER_ERROR_INTERNAL },
      /* One byte too short to be a valid header: */
      { (const guint8 *) "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 11, G_RESOLVER_ERROR_INTERNAL },
      /* Valid header indicating no answers: */
      { (const guint8 *) "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 12, G_RESOLVER_ERROR_NOT_FOUND },
    };
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (vectors); i++)
    {
      GList *records = NULL;
      GError *local_error = NULL;

      records = g_resolver_records_from_res_query ("example.org",
                                                   g_resolver_record_type_to_rrtype (G_RESOLVER_RECORD_NS),
                                                   vectors[i].answer,
                                                   vectors[i].answer_len,
                                                   0,
                                                   &local_error);

      g_assert_error (local_error, G_RESOLVER_ERROR, (gint) vectors[i].expected_error_code);
      g_assert_null (records);
      g_clear_error (&local_error);
    }
}

static void
test_unknown_record_type (void)
{
#ifndef HAVE_DN_COMP
  g_test_skip ("The dn_comp() function was not available.");
  return;
#else
  GByteArray *answer = NULL;
  GList *records = NULL;
  GError *local_error = NULL;
  const guint type_id = 20;  /* ISDN, not supported anywhere */

  /* An answer with an unsupported type chosen from
   * https://en.wikipedia.org/wiki/List_of_DNS_record_types#[1]_Obsolete_record_types */
  answer = dns_header ();
  dns_builder_add_domain (answer, "example.org");
  dns_builder_add_uint16 (answer, type_id);
  dns_builder_add_uint16 (answer, 1); /* qclass=C_IN */
  dns_builder_add_uint32 (answer, 0); /* ttl (ignored) */
  dns_builder_add_uint16 (answer, 0); /* rdlength */

  records = g_resolver_records_from_res_query ("example.org",
                                               type_id,
                                               answer->data,
                                               answer->len,
                                               0,
                                               &local_error);

  g_assert_error (local_error, G_RESOLVER_ERROR, G_RESOLVER_ERROR_NOT_FOUND);
  g_assert_null (records);
  g_clear_error (&local_error);

  g_byte_array_free (answer, TRUE);
#endif
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add_func ("/gresolver/invalid-header", test_invalid_header);
  g_test_add_func ("/gresolver/unknown-record-type", test_unknown_record_type);

  return g_test_run ();
}
