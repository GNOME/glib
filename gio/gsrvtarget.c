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

#include "gsrvtarget.h"

#include <stdlib.h>
#include <string.h>

#include "gioalias.h"

/**
 * SECTION:gsrvtarget
 * @short_description: DNS SRV record target
 * @include: gio/gio.h
 *
 * SRV (service) records are used by some network protocols to provide
 * service-specific aliasing and load-balancing. For example, XMPP
 * (Jabber) uses SRV records to locate the XMPP server for a domain;
 * rather than connecting directly to "example.com" or assuming a
 * specific server hostname like "xmpp.example.com", an XMPP client
 * would look up the "xmpp-client" SRV record for "example.com", and
 * then connect to whatever host was pointed to by that record.
 *
 * You can use g_resolver_lookup_service() or
 * g_resolver_lookup_service_async() to find the #GSrvTarget<!-- -->s
 * for a given service. However, if you are simply planning to connect
 * to the remote service, you can use #GNetworkService's
 * #GSocketConnectable interface and not need to worry about
 * #GSrvTarget at all.
 */

struct _GSrvTarget {
  gchar   *hostname;
  guint16  port;

  guint16  priority;
  guint16  weight;
};

/**
 * GSrvTarget:
 *
 * A single target host/port that a network service is running on.
 */

GType
g_srv_target_get_type (void)
{
  static volatile gsize type_volatile = 0;

  if (g_once_init_enter (&type_volatile))
    {
      GType type = g_boxed_type_register_static (
                        g_intern_static_string ("GSrvTarget"),
			(GBoxedCopyFunc) g_srv_target_copy,
			(GBoxedFreeFunc) g_srv_target_free);
      g_once_init_leave (&type_volatile, type);
    }
  return type_volatile;
}

/**
 * g_srv_target_new:
 * @hostname: the host that the service is running on
 * @port: the port that the service is running on
 * @priority: the target's priority
 * @weight: the target's weight
 *
 * Creates a new #GSrvTarget with the given parameters.
 *
 * You should not need to use this; normally #GSrvTarget<!-- -->s are
 * created by #GResolver.
 *
 * Return value: a new #GSrvTarget.
 *
 * Since: 2.22
 */
GSrvTarget *
g_srv_target_new (const gchar *hostname,
                  guint16      port,
                  guint16      priority,
                  guint16      weight)
{
  GSrvTarget *target = g_slice_new0 (GSrvTarget);

  target->hostname = g_strdup (hostname);
  target->port = port;
  target->priority = priority;
  target->weight = weight;

  return target;
}

/**
 * g_srv_target_copy:
 * @target: a #GSrvTarget
 *
 * Copies @target
 *
 * Return value: a copy of @target
 *
 * Since: 2.22
 */
GSrvTarget *
g_srv_target_copy (GSrvTarget *target)
{
  return g_srv_target_new (target->hostname, target->port,
                           target->priority, target->weight);
}

/**
 * g_srv_target_free:
 * @target: a #GSrvTarget
 *
 * Frees @target
 *
 * Since: 2.22
 */
void
g_srv_target_free (GSrvTarget *target)
{
  g_free (target->hostname);
  g_slice_free (GSrvTarget, target);
}

/**
 * g_srv_target_get_hostname:
 * @target: a #GSrvTarget
 *
 * Gets @target's hostname (in ASCII form; if you are going to present
 * this to the user, you should use g_hostname_is_ascii_encoded() to
 * check if it contains encoded Unicode segments, and use
 * g_hostname_to_unicode() to convert it if it does.)
 *
 * Return value: @target's hostname
 *
 * Since: 2.22
 */
const gchar *
g_srv_target_get_hostname (GSrvTarget *target)
{
  return target->hostname;
}

/**
 * g_srv_target_get_port:
 * @target: a #GSrvTarget
 *
 * Gets @target's port
 *
 * Return value: @target's port
 *
 * Since: 2.22
 */
guint16
g_srv_target_get_port (GSrvTarget *target)
{
  return target->port;
}

/**
 * g_srv_target_get_priority:
 * @target: a #GSrvTarget
 *
 * Gets @target's priority. You should not need to look at this;
 * #GResolver already sorts the targets according to the algorithm in
 * RFC 2782.
 *
 * Return value: @target's priority
 *
 * Since: 2.22
 */
guint16
g_srv_target_get_priority (GSrvTarget *target)
{
  return target->priority;
}

/**
 * g_srv_target_get_weight:
 * @target: a #GSrvTarget
 *
 * Gets @target's weight. You should not need to look at this;
 * #GResolver already sorts the targets according to the algorithm in
 * RFC 2782.
 *
 * Return value: @target's weight
 *
 * Since: 2.22
 */
guint16
g_srv_target_get_weight (GSrvTarget *target)
{
  return target->weight;
}

gint
compare_target (gconstpointer a, gconstpointer b)
{
  GSrvTarget *ta = (GSrvTarget *)a;
  GSrvTarget *tb = (GSrvTarget *)b;

  if (ta->priority == tb->priority)
    {
      /* Arrange targets of the same priority "in any order, except
       * that all those with weight 0 are placed at the beginning of
       * the list"
       */
      if (ta->weight == 0)
        return -1;
      else if (tb->weight == 0)
        return 1;
      else
        return g_random_int_range (-1, 1);
    }
  else
    return ta->priority - tb->priority;
}

/**
 * g_srv_target_list_sort:
 * @targets: a #GList of #GSrvTarget
 *
 * Sorts @targets in place according to the algorithm in RFC 2782.
 *
 * Return value: the head of the sorted list.
 *
 * Since: 2.22
 */
GList *
g_srv_target_list_sort (GList *targets)
{
  gint sum, val, priority, weight;
  GList *first, *last, *n;
  GSrvTarget *target;
  gpointer tmp;

  if (!targets)
    return NULL;

  if (!targets->next)
    {
      target = targets->data;
      if (!strcmp (target->hostname, "."))
        {
          /* 'A Target of "." means that the service is decidedly not
           * available at this domain.'
           */
          g_srv_target_free (target);
          g_list_free (targets);
          return NULL;
        }
    }

  /* Sort by priority, and partly by weight */
  targets = g_list_sort (targets, compare_target);

  /* For each group of targets with the same priority, rebalance them
   * according to weight.
   */
  for (first = targets; first; first = last->next)
    {
      /* Skip @first to a non-0-weight target. */
      while (first && ((GSrvTarget *)first->data)->weight == 0)
        first = first->next;
      if (!first)
        break;

      /* Skip @last to the last target of the same priority. */
      priority = ((GSrvTarget *)first->data)->priority;
      last = first;
      while (last->next &&
             ((GSrvTarget *)last->next->data)->priority == priority)
        last = last->next;

      /* If there's only one non-0 weight target at this priority,
       * we can move on to the next priority level.
       */
      if (last == first)
        continue;

      /* Randomly reorder the non-0 weight targets, giving precedence
       * to the ones with higher weight. RFC 2782 describes this in
       * terms of assigning a running sum to each target and building
       * a new list. We do things slightly differently, but should get
       * the same result.
       */
      for (n = first, sum = 0; n != last->next; n = n->next)
        sum += ((GSrvTarget *)n->data)->weight;
      while (first != last)
        {
          val = g_random_int_range (0, sum);
          for (n = first; n != last; n = n->next)
            {
              weight = ((GSrvTarget *)n->data)->weight;
              if (val < weight)
                break;
              val -= weight;
            }

          tmp = first->data;
          first->data = n->data;
          n->data = tmp;

          sum -= weight;
          first = first->next;
        }
    }

  return targets;
}

#define __G_SRV_TARGET_C__
#include "gioaliasdef.c"
