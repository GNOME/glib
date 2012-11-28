/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2008 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include <glib.h>
#include "glibintl.h"

#include <stdio.h>
#include <string.h>

#include "gthreadedresolver.h"
#include "gnetworkingprivate.h"

#include "gcancellable.h"
#include "gtask.h"
#include "gsocketaddress.h"


G_DEFINE_TYPE (GThreadedResolver, g_threaded_resolver, G_TYPE_RESOLVER)

static void
g_threaded_resolver_init (GThreadedResolver *gtr)
{
}

static void
do_lookup_by_name (GTask         *task,
                   gpointer       source_object,
                   gpointer       task_data,
                   GCancellable  *cancellable)
{
  const char *hostname = task_data;
  struct addrinfo *res = NULL;
  GList *addresses;
  gint retval;
  GError *error = NULL;

  retval = getaddrinfo (hostname, NULL, &_g_resolver_addrinfo_hints, &res);
  addresses = _g_resolver_addresses_from_addrinfo (hostname, res, retval, &error);
  if (res)
    freeaddrinfo (res);

  if (addresses)
    {
      g_task_return_pointer (task, addresses,
                             (GDestroyNotify)g_resolver_free_addresses);
    }
  else
    g_task_return_error (task, error);
}

static GList *
lookup_by_name (GResolver     *resolver,
                const gchar   *hostname,
                GCancellable  *cancellable,
                GError       **error)
{
  GTask *task;
  GList *addresses;

  task = g_task_new (resolver, cancellable, NULL, NULL);
  g_task_set_task_data (task, g_strdup (hostname), g_free);
  g_task_set_return_on_cancel (task, TRUE);
  g_task_run_in_thread_sync (task, do_lookup_by_name);
  addresses = g_task_propagate_pointer (task, error);
  g_object_unref (task);

  return addresses;
}

static void
lookup_by_name_async (GResolver           *resolver,
                      const gchar         *hostname,
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data)
{
  GTask *task;

  task = g_task_new (resolver, cancellable, callback, user_data);
  g_task_set_task_data (task, g_strdup (hostname), g_free);
  g_task_set_return_on_cancel (task, TRUE);
  g_task_run_in_thread (task, do_lookup_by_name);
  g_object_unref (task);
}

static GList *
lookup_by_name_finish (GResolver     *resolver,
                       GAsyncResult  *result,
                       GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (result, resolver), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}


static void
do_lookup_by_address (GTask         *task,
                      gpointer       source_object,
                      gpointer       task_data,
                      GCancellable  *cancellable)
{
  GInetAddress *address = task_data;
  struct sockaddr_storage sockaddr;
  gsize sockaddr_size;
  gchar namebuf[NI_MAXHOST], *name;
  gint retval;
  GError *error = NULL;

  _g_resolver_address_to_sockaddr (address, &sockaddr, &sockaddr_size);
  retval = getnameinfo ((struct sockaddr *)&sockaddr, sockaddr_size,
                        namebuf, sizeof (namebuf), NULL, 0, NI_NAMEREQD);
  name = _g_resolver_name_from_nameinfo (address, namebuf, retval, &error);

  if (name)
    g_task_return_pointer (task, name, g_free);
  else
    g_task_return_error (task, error);
}

static gchar *
lookup_by_address (GResolver        *resolver,
                   GInetAddress     *address,
                   GCancellable     *cancellable,
                   GError          **error)
{
  GTask *task;
  gchar *name;

  task = g_task_new (resolver, cancellable, NULL, NULL);
  g_task_set_task_data (task, g_object_ref (address), g_object_unref);
  g_task_set_return_on_cancel (task, TRUE);
  g_task_run_in_thread_sync (task, do_lookup_by_address);
  name = g_task_propagate_pointer (task, error);
  g_object_unref (task);

  return name;
}

static void
lookup_by_address_async (GResolver           *resolver,
                         GInetAddress        *address,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  GTask *task;

  task = g_task_new (resolver, cancellable, callback, user_data);
  g_task_set_task_data (task, g_object_ref (address), g_object_unref);
  g_task_set_return_on_cancel (task, TRUE);
  g_task_run_in_thread (task, do_lookup_by_address);
  g_object_unref (task);
}

static gchar *
lookup_by_address_finish (GResolver     *resolver,
                          GAsyncResult  *result,
                          GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (result, resolver), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}


typedef struct {
  char *rrname;
  GResolverRecordType record_type;
} LookupRecordsData;

static void
free_lookup_records_data (LookupRecordsData *lrd)
{
  g_free (lrd->rrname);
  g_slice_free (LookupRecordsData, lrd);
}

static void
free_records (GList *records)
{
  g_list_free_full (records, (GDestroyNotify) g_variant_unref);
}

#if defined(G_OS_UNIX)
#ifdef __BIONIC__
#define C_IN 1
int res_query(const char *, int, int, u_char *, int);
#endif
#endif

static void
do_lookup_records (GTask         *task,
                   gpointer       source_object,
                   gpointer       task_data,
                   GCancellable  *cancellable)
{
  LookupRecordsData *lrd = task_data;
  GList *records;
  GError *error = NULL;
#if defined(G_OS_UNIX)
  gint len = 512;
  gint herr;
  GByteArray *answer;
  gint rrtype;

  rrtype = _g_resolver_record_type_to_rrtype (lrd->record_type);
  answer = g_byte_array_new ();
  for (;;)
    {
      g_byte_array_set_size (answer, len * 2);
      len = res_query (lrd->rrname, C_IN, rrtype, answer->data, answer->len);

      /* If answer fit in the buffer then we're done */
      if (len < 0 || len < (gint)answer->len)
        break;

      /*
       * On overflow some res_query's return the length needed, others
       * return the full length entered. This code works in either case.
       */
  }

  herr = h_errno;
  records = _g_resolver_records_from_res_query (lrd->rrname, rrtype, answer->data, len, herr, &error);
  g_byte_array_free (answer, TRUE);

#elif defined(G_OS_WIN32)
  DNS_STATUS status;
  DNS_RECORD *results = NULL;
  WORD dnstype;

  dnstype = _g_resolver_record_type_to_dnstype (lrd->record_type);
  status = DnsQuery_A (lrd->rrname, dnstype, DNS_QUERY_STANDARD, NULL, &results, NULL);
  records = _g_resolver_records_from_DnsQuery (lrd->rrname, dnstype, status, results, &error);
  if (results != NULL)
    DnsRecordListFree (results, DnsFreeRecordList);
#endif

  if (records)
    {
      g_task_return_pointer (task, records, (GDestroyNotify) free_records);
    }
  else
    g_task_return_error (task, error);
}

static GList *
lookup_records (GResolver              *resolver,
                const gchar            *rrname,
                GResolverRecordType     record_type,
                GCancellable           *cancellable,
                GError                **error)
{
  GTask *task;
  GList *records;
  LookupRecordsData *lrd;

  task = g_task_new (resolver, cancellable, NULL, NULL);

  lrd = g_slice_new (LookupRecordsData);
  lrd->rrname = g_strdup (rrname);
  lrd->record_type = record_type;
  g_task_set_task_data (task, lrd, (GDestroyNotify) free_lookup_records_data);

  g_task_set_return_on_cancel (task, TRUE);
  g_task_run_in_thread_sync (task, do_lookup_records);
  records = g_task_propagate_pointer (task, error);
  g_object_unref (task);

  return records;
}

static void
lookup_records_async (GResolver           *resolver,
                      const char          *rrname,
                      GResolverRecordType  record_type,
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data)
{
  GTask *task;
  LookupRecordsData *lrd;

  task = g_task_new (resolver, cancellable, callback, user_data);

  lrd = g_slice_new (LookupRecordsData);
  lrd->rrname = g_strdup (rrname);
  lrd->record_type = record_type;
  g_task_set_task_data (task, lrd, (GDestroyNotify) free_lookup_records_data);

  g_task_set_return_on_cancel (task, TRUE);
  g_task_run_in_thread (task, do_lookup_records);
  g_object_unref (task);
}

static GList *
lookup_records_finish (GResolver     *resolver,
                       GAsyncResult  *result,
                       GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (result, resolver), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}


static void
g_threaded_resolver_class_init (GThreadedResolverClass *threaded_class)
{
  GResolverClass *resolver_class = G_RESOLVER_CLASS (threaded_class);

  resolver_class->lookup_by_name           = lookup_by_name;
  resolver_class->lookup_by_name_async     = lookup_by_name_async;
  resolver_class->lookup_by_name_finish    = lookup_by_name_finish;
  resolver_class->lookup_by_address        = lookup_by_address;
  resolver_class->lookup_by_address_async  = lookup_by_address_async;
  resolver_class->lookup_by_address_finish = lookup_by_address_finish;
  resolver_class->lookup_records           = lookup_records;
  resolver_class->lookup_records_async     = lookup_records_async;
  resolver_class->lookup_records_finish    = lookup_records_finish;
}
