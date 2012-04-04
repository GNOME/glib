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
#include "gsimpleasyncresult.h"
#include "gsocketaddress.h"


G_DEFINE_TYPE (GThreadedResolver, g_threaded_resolver, G_TYPE_RESOLVER)

static void threaded_resolver_thread (gpointer thread_data, gpointer pool_data);

static void
g_threaded_resolver_init (GThreadedResolver *gtr)
{
  gtr->thread_pool = g_thread_pool_new (threaded_resolver_thread, gtr,
                                        -1, FALSE, NULL);
}

static void
finalize (GObject *object)
{
  GThreadedResolver *gtr = G_THREADED_RESOLVER (object);

  g_thread_pool_free (gtr->thread_pool, FALSE, FALSE);

  G_OBJECT_CLASS (g_threaded_resolver_parent_class)->finalize (object);
}

/* A GThreadedResolverRequest represents a request in progress
 * (usually, but see case 1). It is refcounted, to make sure that it
 * doesn't get freed too soon. In particular, it can't be freed until
 * (a) the resolver thread has finished resolving, (b) the calling
 * thread has received an answer, and (c) no other thread could be in
 * the process of trying to cancel it.
 *
 * The possibilities:
 *
 * 1. Synchronous non-cancellable request: in this case, the request
 *    is simply done in the calling thread, without using
 *    GThreadedResolverRequest at all.
 *
 * 2. Synchronous cancellable request: A req is created with a GCond,
 *    and 3 refs (for the resolution thread, the calling thread, and
 *    the cancellation signal handler).
 *
 *      a. If the resolution completes successfully, the thread pool
 *         function (threaded_resolver_thread()) will call
 *         g_threaded_resolver_request_complete(), which will detach
 *         the "cancelled" signal handler (dropping one ref on req)
 *         and signal the GCond, and then unref the req. The calling
 *         thread receives the signal from the GCond, processes the
 *         response, and unrefs the req, causing it to be freed.
 *
 *      b. If the resolution is cancelled before completing,
 *         request_cancelled() will call
 *         g_threaded_resolver_request_complete(), which will detach
 *         the signal handler (as above, unreffing the req), set
 *         req->error to indicate that it was cancelled, and signal
 *         the GCond. The calling thread receives the signal from the
 *         GCond, processes the response, and unrefs the req.
 *         Eventually, the resolver thread finishes resolving (or
 *         times out in the resolver) and calls
 *         g_threaded_resolver_request_complete() again, but
 *         _request_complete() does nothing this time since the
 *         request is already complete. The thread pool func then
 *         unrefs the req, causing it to be freed.
 *
 * 3. Asynchronous request: A req is created with a GSimpleAsyncResult
 *    (and no GCond). The calling thread's ref on req is set up to be
 *    automatically dropped when the async_result is freed. Two
 *    sub-possibilities:
 *
 *      a. If the resolution completes, the thread pool function
 *         (threaded_resolver_thread()) will call
 *         g_threaded_resolver_request_complete(), which will detach
 *         the "cancelled" signal handler (if it was present)
 *         (unreffing the req), queue the async_result to complete in
 *         an idle handler, unref the async_result (which is still
 *         reffed by the idle handler though), and then unref the req.
 *         The main thread then invokes the async_result's callback
 *         and processes the response. When it finishes, the
 *         async_result drops the ref that was taken by
 *         g_simple_async_result_complete_in_idle(), which causes the
 *         async_result to be freed, which causes req to be unreffed
 *         and freed.
 *
 *      b. If the resolution is cancelled, request_cancelled() will
 *         call g_threaded_resolver_request_complete(), which will
 *         detach the signal handler (as above, unreffing the req) set
 *         req->error to indicate that it was cancelled, and queue and
 *         unref the async_result. The main thread completes the
 *         async_request and unrefs it and the req, as above.
 *         Eventually, the resolver thread finishes resolving (or
 *         times out in the resolver) and calls
 *         g_threaded_resolver_request_complete() again, but
 *         _request_complete() does nothing this time since the
 *         request is already complete. The thread pool func then
 *         unrefs the req, causing it to be freed.
 *
 * g_threaded_resolver_request_complete() ensures that if the request
 * completes and cancels "at the same time" that only one of the two
 * conditions gets processed.
 */

typedef struct _GThreadedResolverRequest GThreadedResolverRequest;
typedef void (*GThreadedResolverResolveFunc) (GThreadedResolverRequest *, GError **);
typedef void (*GThreadedResolverFreeFunc) (GThreadedResolverRequest *);

struct _GThreadedResolverRequest {
  GThreadedResolverResolveFunc resolve_func;
  GThreadedResolverFreeFunc free_func;

  union {
    struct {
      gchar *hostname;
      GList *addresses;
    } name;
    struct {
      GInetAddress *address;
      gchar *name;
    } address;
    struct {
      gchar *rrname;
      GResolverRecordType record_type;
      GList *results;
    } records;
  } u;

  GCancellable *cancellable;
  GError *error;

  GMutex mutex;
  guint ref_count;

  GCond cond;
  GSimpleAsyncResult *async_result;
  gboolean complete;

};

static void g_threaded_resolver_request_unref (GThreadedResolverRequest *req);
static void request_cancelled (GCancellable *cancellable, gpointer req);
static void request_cancelled_disconnect_notify (gpointer req, GClosure *closure);

static GThreadedResolverRequest *
g_threaded_resolver_request_new (GThreadedResolverResolveFunc  resolve_func,
                                 GThreadedResolverFreeFunc     free_func,
				 GCancellable                 *cancellable)
{
  GThreadedResolverRequest *req;

  req = g_slice_new0 (GThreadedResolverRequest);
  req->resolve_func = resolve_func;
  req->free_func = free_func;

  /* Initial refcount is 2; one for the caller and one for resolve_func */
  req->ref_count = 2;

  g_mutex_init (&req->mutex);
  g_cond_init (&req->cond);
  /* Initially locked; caller must unlock */
  g_mutex_lock (&req->mutex);

  if (cancellable)
    {
      req->ref_count++;
      req->cancellable = g_object_ref (cancellable);
      g_signal_connect_data (cancellable, "cancelled",
			     G_CALLBACK (request_cancelled), req,
			     request_cancelled_disconnect_notify, 0);
    }

  return req;
}

static void
g_threaded_resolver_request_unref (GThreadedResolverRequest *req)
{
  guint ref_count;

  g_mutex_lock (&req->mutex);
  ref_count = --req->ref_count;
  g_mutex_unlock (&req->mutex);
  if (ref_count > 0)
    return;

  g_mutex_clear (&req->mutex);
  g_cond_clear (&req->cond);

  if (req->error)
    g_error_free (req->error);

  if (req->free_func)
    req->free_func (req);

  /* We don't have to free req->cancellable or req->async_result,
   * since (if set), they must already have been freed by
   * request_complete() in order to get here.
   */

  g_slice_free (GThreadedResolverRequest, req);
}

static void
g_threaded_resolver_request_complete (GThreadedResolverRequest *req,
				      GError                   *error)
{
  g_mutex_lock (&req->mutex);
  if (req->complete)
    {
      /* The req was cancelled, and now it has finished resolving as
       * well. But we have nowhere to send the result, so just return.
       */
      g_mutex_unlock (&req->mutex);
      g_clear_error (&error);
      return;
    }

  req->complete = TRUE;
  g_mutex_unlock (&req->mutex);

  if (error)
    g_propagate_error (&req->error, error);

  if (req->cancellable)
    {
      /* Drop the signal handler's ref on @req */
      g_signal_handlers_disconnect_by_func (req->cancellable, request_cancelled, req);
      g_object_unref (req->cancellable);
      req->cancellable = NULL;
    }

  if (req->async_result)
    {
      if (req->error)
        g_simple_async_result_set_from_error (req->async_result, req->error);
      g_simple_async_result_complete_in_idle (req->async_result);

      /* Drop our ref on the async_result, which will eventually cause
       * it to drop its ref on req.
       */
      g_object_unref (req->async_result);
      req->async_result = NULL;
    }

  else
    g_cond_signal (&req->cond);
}

static void
request_cancelled (GCancellable *cancellable,
                   gpointer      user_data)
{
  GThreadedResolverRequest *req = user_data;
  GError *error = NULL;

  g_cancellable_set_error_if_cancelled (req->cancellable, &error);
  g_threaded_resolver_request_complete (req, error);

  /* We can't actually cancel the resolver thread; it will eventually
   * complete on its own and call request_complete() again, which will
   * do nothing the second time.
   */
}

static void
request_cancelled_disconnect_notify (gpointer  req,
                                     GClosure *closure)
{
  g_threaded_resolver_request_unref (req);
}

static void
threaded_resolver_thread (gpointer thread_data,
                          gpointer pool_data)
{
  GThreadedResolverRequest *req = thread_data;
  GError *error = NULL;

  req->resolve_func (req, &error);
  g_threaded_resolver_request_complete (req, error);
  g_threaded_resolver_request_unref (req);
}

static void
resolve_sync (GThreadedResolver         *gtr,
              GThreadedResolverRequest  *req,
              GError                   **error)
{
  if (!req->cancellable)
    {
      req->resolve_func (req, error);
      g_mutex_unlock (&req->mutex);

      g_threaded_resolver_request_complete (req, FALSE);
      g_threaded_resolver_request_unref (req);
      return;
    }

  g_thread_pool_push (gtr->thread_pool, req, &req->error);
  if (!req->error)
    g_cond_wait (&req->cond, &req->mutex);
  g_mutex_unlock (&req->mutex);

  if (req->error)
    {
      g_propagate_error (error, req->error);
      req->error = NULL;
    }
}

static void
resolve_async (GThreadedResolver        *gtr,
               GThreadedResolverRequest *req,
               GAsyncReadyCallback       callback,
               gpointer                  user_data,
               gpointer                  tag)
{
  req->async_result = g_simple_async_result_new (G_OBJECT (gtr),
                                                 callback, user_data, tag);
  g_simple_async_result_set_op_res_gpointer (req->async_result, req,
                                             (GDestroyNotify)g_threaded_resolver_request_unref);
  g_thread_pool_push (gtr->thread_pool, req, NULL);
  g_mutex_unlock (&req->mutex);
}

static GThreadedResolverRequest *
resolve_finish (GResolver     *resolver,
                GAsyncResult  *result,
		gpointer       tag,
                GError       **error)
{
  g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (resolver), tag), NULL);

  return g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (result));
}

static void
do_lookup_by_name (GThreadedResolverRequest  *req,
                   GError                   **error)
{
  struct addrinfo *res = NULL;
  gint retval;

  retval = getaddrinfo (req->u.name.hostname, NULL,
                        &_g_resolver_addrinfo_hints, &res);
  req->u.name.addresses =
    _g_resolver_addresses_from_addrinfo (req->u.name.hostname, res, retval, error);
  if (res)
    freeaddrinfo (res);
}

static GList *
lookup_by_name (GResolver     *resolver,
                const gchar   *hostname,
                GCancellable  *cancellable,
                GError       **error)
{
  GThreadedResolver *gtr = G_THREADED_RESOLVER (resolver);
  GThreadedResolverRequest *req;
  GList *addresses;

  req = g_threaded_resolver_request_new (do_lookup_by_name, NULL, cancellable);
  req->u.name.hostname = (gchar *)hostname;
  resolve_sync (gtr, req, error);

  addresses = req->u.name.addresses;
  g_threaded_resolver_request_unref (req);
  return addresses;
}

static void
free_lookup_by_name (GThreadedResolverRequest *req)
{
  g_free (req->u.name.hostname);
  if (req->u.name.addresses)
    g_resolver_free_addresses (req->u.name.addresses);
}

static void
lookup_by_name_async (GResolver           *resolver,
                      const gchar         *hostname,
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data)
{
  GThreadedResolver *gtr = G_THREADED_RESOLVER (resolver);
  GThreadedResolverRequest *req;

  req = g_threaded_resolver_request_new (do_lookup_by_name, free_lookup_by_name,
                                         cancellable);
  req->u.name.hostname = g_strdup (hostname);
  resolve_async (gtr, req, callback, user_data, lookup_by_name_async);
}

static GList *
lookup_by_name_finish (GResolver     *resolver,
                       GAsyncResult  *result,
                       GError       **error)
{
  GThreadedResolverRequest *req;
  GList *addresses;

  req = resolve_finish (resolver, result, lookup_by_name_async, error);
  addresses = req->u.name.addresses;
  req->u.name.addresses = NULL;
  return addresses;
}


static void
do_lookup_by_address (GThreadedResolverRequest  *req,
                      GError                   **error)
{
  struct sockaddr_storage sockaddr;
  gsize sockaddr_size;
  gchar name[NI_MAXHOST];
  gint retval;

  _g_resolver_address_to_sockaddr (req->u.address.address,
                                   &sockaddr, &sockaddr_size);

  retval = getnameinfo ((struct sockaddr *)&sockaddr, sockaddr_size,
                        name, sizeof (name), NULL, 0, NI_NAMEREQD);
  req->u.address.name = _g_resolver_name_from_nameinfo (req->u.address.address,
                                                        name, retval, error);
}

static gchar *
lookup_by_address (GResolver        *resolver,
                   GInetAddress     *address,
                   GCancellable     *cancellable,
                   GError          **error)
{
  GThreadedResolver *gtr = G_THREADED_RESOLVER (resolver);
  GThreadedResolverRequest *req;
  gchar *name;

  req = g_threaded_resolver_request_new (do_lookup_by_address, NULL, cancellable);
  req->u.address.address = address;
  resolve_sync (gtr, req, error);

  name = req->u.address.name;
  g_threaded_resolver_request_unref (req);
  return name;
}

static void
free_lookup_by_address (GThreadedResolverRequest *req)
{
  g_object_unref (req->u.address.address);
  if (req->u.address.name)
    g_free (req->u.address.name);
}

static void
lookup_by_address_async (GResolver           *resolver,
                         GInetAddress        *address,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  GThreadedResolver *gtr = G_THREADED_RESOLVER (resolver);
  GThreadedResolverRequest *req;

  req = g_threaded_resolver_request_new (do_lookup_by_address,
                                         free_lookup_by_address,
                                         cancellable);
  req->u.address.address = g_object_ref (address);
  resolve_async (gtr, req, callback, user_data, lookup_by_address_async);
}

static gchar *
lookup_by_address_finish (GResolver     *resolver,
                          GAsyncResult  *result,
                          GError       **error)
{
  GThreadedResolverRequest *req;
  gchar *name;

  req = resolve_finish (resolver, result, lookup_by_address_async, error);
  name = req->u.address.name;
  req->u.address.name = NULL;
  return name;
}


static void
do_lookup_records (GThreadedResolverRequest  *req,
                   GError                   **error)
{
#if defined(G_OS_UNIX)
  gint len = 512;
  gint herr;
  GByteArray *answer;
  gint rrtype;

  rrtype = _g_resolver_record_type_to_rrtype (req->u.records.record_type);
  answer = g_byte_array_new ();
  for (;;)
    {
      g_byte_array_set_size (answer, len * 2);
      len = res_query (req->u.records.rrname, C_IN, rrtype, answer->data, answer->len);

      /* If answer fit in the buffer then we're done */
      if (len < 0 || len < (gint)answer->len)
        break;

      /*
       * On overflow some res_query's return the length needed, others
       * return the full length entered. This code works in either case.
       */
  }

  herr = h_errno;
  req->u.records.results = _g_resolver_records_from_res_query (req->u.records.rrname, rrtype, answer->data, len, herr, error);
  g_byte_array_free (answer, TRUE);

#elif defined(G_OS_WIN32)
  DNS_STATUS status;
  DNS_RECORD *results = NULL;
  WORD dnstype;

  dnstype = _g_resolver_record_type_to_dnstype (req->u.records.record_type);
  status = DnsQuery_A (req->u.records.rrname, dnstype, DNS_QUERY_STANDARD, NULL, &results, NULL);
  req->u.records.results = _g_resolver_records_from_DnsQuery (req->u.records.rrname, dnstype, status, results, error);
  if (results != NULL)
    DnsRecordListFree (results, DnsFreeRecordList);
#endif
}

static GList *
lookup_records (GResolver              *resolver,
                const gchar            *rrname,
                GResolverRecordType    record_type,
                GCancellable          *cancellable,
                GError               **error)
{
  GThreadedResolver *gtr = G_THREADED_RESOLVER (resolver);
  GThreadedResolverRequest *req;
  GList *results;

  req = g_threaded_resolver_request_new (do_lookup_records, NULL, cancellable);
  req->u.records.rrname = (char *)rrname;
  req->u.records.record_type = record_type;
  resolve_sync (gtr, req, error);

  results = req->u.records.results;
  g_threaded_resolver_request_unref (req);
  return results;
}

static void
free_lookup_records (GThreadedResolverRequest *req)
{
  g_free (req->u.records.rrname);
  g_list_free_full (req->u.records.results, (GDestroyNotify)g_variant_unref);
}

static void
lookup_records_async (GResolver           *resolver,
                      const char          *rrname,
                      GResolverRecordType  record_type,
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data)
{
  GThreadedResolver *gtr = G_THREADED_RESOLVER (resolver);
  GThreadedResolverRequest *req;

  req = g_threaded_resolver_request_new (do_lookup_records,
                                         free_lookup_records,
                                         cancellable);
  req->u.records.rrname = g_strdup (rrname);
  req->u.records.record_type = record_type;
  resolve_async (gtr, req, callback, user_data, lookup_records_async);
}

static GList *
lookup_records_finish (GResolver     *resolver,
                       GAsyncResult  *result,
                       GError       **error)
{
  GThreadedResolverRequest *req;
  GList *records;

  req = resolve_finish (resolver, result, lookup_records_async, error);
  records = req->u.records.results;
  req->u.records.results = NULL;
  return records;
}


static void
g_threaded_resolver_class_init (GThreadedResolverClass *threaded_class)
{
  GResolverClass *resolver_class = G_RESOLVER_CLASS (threaded_class);
  GObjectClass *object_class = G_OBJECT_CLASS (threaded_class);

  resolver_class->lookup_by_name           = lookup_by_name;
  resolver_class->lookup_by_name_async     = lookup_by_name_async;
  resolver_class->lookup_by_name_finish    = lookup_by_name_finish;
  resolver_class->lookup_by_address        = lookup_by_address;
  resolver_class->lookup_by_address_async  = lookup_by_address_async;
  resolver_class->lookup_by_address_finish = lookup_by_address_finish;
  resolver_class->lookup_records           = lookup_records;
  resolver_class->lookup_records_async     = lookup_records_async;
  resolver_class->lookup_records_finish    = lookup_records_finish;

  object_class->finalize = finalize;
}
