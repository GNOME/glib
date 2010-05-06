/* GDBus - GLib D-Bus Library
 *
 * Copyright (C) 2008-2009 Red Hat, Inc.
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
 *
 * Author: David Zeuthen <davidz@redhat.com>
 */

#if !defined (GIO_COMPILATION)
#error "gdbusprivate.h is a private header file."
#endif

#ifndef __G_DBUS_PRIVATE_H__
#define __G_DBUS_PRIVATE_H__

#include <gio/giotypes.h>

G_BEGIN_DECLS

/* ---------------------------------------------------------------------------------------------------- */

typedef struct GDBusWorker GDBusWorker;

typedef void (*GDBusWorkerMessageReceivedCallback) (GDBusWorker   *worker,
                                                    GDBusMessage  *message,
                                                    gpointer       user_data);

typedef void (*GDBusWorkerDisconnectedCallback)    (GDBusWorker   *worker,
                                                    gboolean       remote_peer_vanished,
                                                    GError        *error,
                                                    gpointer       user_data);

/* This function may be called from any thread - callbacks will be in the shared private message thread
 * and must not block.
 */
GDBusWorker *_g_dbus_worker_new          (GIOStream                          *stream,
                                          GDBusCapabilityFlags                capabilities,
                                          GDBusWorkerMessageReceivedCallback  message_received_callback,
                                          GDBusWorkerDisconnectedCallback     disconnected_callback,
                                          gpointer                            user_data);

/* can be called from any thread - steals blob */
void         _g_dbus_worker_send_message (GDBusWorker    *worker,
                                          GDBusMessage   *message,
                                          gchar          *blob,
                                          gsize           blob_len);

/* can be called from any thread */
void         _g_dbus_worker_stop         (GDBusWorker    *worker);

/* ---------------------------------------------------------------------------------------------------- */

void _g_dbus_initialize (void);
gboolean _g_dbus_debug_authentication (void);
gboolean _g_dbus_debug_message (void);

gboolean _g_dbus_address_parse_entry (const gchar  *address_entry,
                                      gchar       **out_transport_name,
                                      GHashTable  **out_key_value_pairs,
                                      GError      **error);

gchar * _g_dbus_compute_complete_signature (GDBusArgInfo **args,
                                            gboolean       include_parentheses);

/* ---------------------------------------------------------------------------------------------------- */

G_END_DECLS

#endif /* __G_DBUS_PRIVATE_H__ */
