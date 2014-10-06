/*  GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2013 Samsung Electronics
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
 * Author: Michal Eljasiewicz   <m.eljasiewic@samsung.com>
 * Author: Lukasz Skalski       <l.skalski@samsung.com>
 */

#include "config.h"

#include "gkdbus.h"
#include "glib-unix.h"

#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <stdio.h>

#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif

#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#include <glib/gstdio.h>
#include <gio/gio.h>
#include <gio/gunixfdlist.h>

#include "glibintl.h"
#include "gunixfdmessage.h"
#include "gdbusprivate.h"
#include "kdbus.h"


/**
 * SECTION:gkdbus
 * @short_description: Low-level kdbus object
 * @include: gio/gio.h
 *
 * A #GKdbus is a lowlevel adapter for kdbus IPC solution. It is meant
 * to replace DBUS  as fundamental IPC solution for  Linux, however it
 * is  still experimental  work in  progress.  You  may  find detailed
 * description in kdbus.txt at https://github.com/gregkh/kdbus
 *
 */


/* Size of memory registered with kdbus for receiving messages */
#define KDBUS_POOL_SIZE (16 * 1024 * 1024)

#define ALIGN8(l) (((l) + 7) & ~7)
#define ALIGN8_PTR(p) ((void*) ALIGN8((gulong) p))

#define KDBUS_ITEM_HEADER_SIZE G_STRUCT_OFFSET(struct kdbus_item, data)
#define KDBUS_ITEM_SIZE(s) ALIGN8((s) + KDBUS_ITEM_HEADER_SIZE)

#define KDBUS_ITEM_NEXT(item) \
        (typeof(item))(((guint8 *)item) + ALIGN8((item)->size))

#define KDBUS_ITEM_FOREACH(item, head, first)                           \
        for (item = (head)->first;                                      \
             ((guint8 *)(item) < (guint8 *)(head) + (head)->size) &&    \
             ((guint8 *)(item) >= (guint8 *)(head));                    \
             item = KDBUS_ITEM_NEXT(item))

#define g_alloca0(x) memset(g_alloca(x), '\0', (x))

/**
 * use systemd-bus-drvierd (from systemd) which implements the all
 * org.freedesktop.DBus methods on kdbus
 */

#define SYSTEMD_BUS_DRIVERD


/* GBusNameOwnerReturnFlags */
typedef enum
{
  G_BUS_REQUEST_NAME_REPLY_PRIMARY_OWNER = 1, /* Caller is now the primary owner of the name, replacing any previous owner */
  G_BUS_REQUEST_NAME_REPLY_IN_QUEUE = 2,      /* The name already had an owner, the application will be placed in a queue */
  G_BUS_REQUEST_NAME_REPLY_EXISTS = 3,        /* The name already has an owner */
  G_BUS_REQUEST_NAME_REPLY_ALREADY_OWNER = 4  /* The application trying to request ownership of a name is already the owner of it */
} GBusNameOwnerReturnFlags;


/* GBusReleaseNameReturnFlags */
typedef enum
{
  G_BUS_RELEASE_NAME_REPLY_RELEASED = 1,     /* The caller has released his claim on the given name */
  G_BUS_RELEASE_NAME_REPLY_NON_EXISTENT = 2, /* The given name does not exist on this bus*/
  G_BUS_RELEASE_NAME_REPLY_NOT_OWNER = 3     /* The caller not waiting in the queue to own this name*/
} GBusReleaseNameReturnFlags;


/* GBusStartServiceReturnFlags */
typedef enum
{
  G_BUS_START_REPLY_SUCCESS = 1,             /* The service was successfully started */
  G_BUS_START_REPLY_ALREADY_RUNNING = 2,     /* A connection already owns the given name */
} GBusStartServiceReturnFlags;


/* GBusCredentialsFlags */
typedef enum
{
  G_BUS_CREDS_PID              = 1,
  G_BUS_CREDS_UID              = 2,
  G_BUS_CREDS_UNIQUE_NAME      = 3,
  G_BUS_CREDS_SELINUX_CONTEXT  = 4
} GBusCredentialsFlags;


static void     g_kdbus_initable_iface_init (GInitableIface  *iface);
static gboolean g_kdbus_initable_init       (GInitable       *initable,
                                             GCancellable    *cancellable,
                                             GError         **error);

#define g_kdbus_get_type _g_kdbus_get_type
G_DEFINE_TYPE_WITH_CODE (GKdbus, g_kdbus, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                g_kdbus_initable_iface_init));


struct _GKdbusPrivate
{
  gint               fd;
  gchar             *path;
  gchar             *kdbus_buffer;
  GSList            *kdbus_msg_items;
  guint64            unique_id;
  guint64            hello_flags;
  guint64            attach_flags;
  guint              closed : 1;
  guint              inited : 1;
  guint              timeout;
  guint              timed_out : 1;
  guchar             bus_id[16];
  struct kdbus_msg  *kmsg;
  GString           *msg_sender;
  GString           *msg_destination;

  gsize              bloom_size;
  guint              bloom_n_hash;

  gint              *fds;
  gint               num_fds;

  gint               memfd;
};


typedef struct {
  GSource        source;
  GPollFD        pollfd;
  GKdbus        *kdbus;
  GIOCondition   condition;
  GCancellable  *cancellable;
  GPollFD        cancel_pollfd;
  gint64         timeout_time;
} GKdbusSource;


typedef gboolean (*GKdbusSourceFunc) (GKdbus *kdbus,
                                      GIOCondition condition,
                                      gpointer user_data);


/* Hash keys for bloom filters*/
const guint8 hash_keys[8][16] =
{
  {0xb9,0x66,0x0b,0xf0,0x46,0x70,0x47,0xc1,0x88,0x75,0xc4,0x9c,0x54,0xb9,0xbd,0x15},
  {0xaa,0xa1,0x54,0xa2,0xe0,0x71,0x4b,0x39,0xbf,0xe1,0xdd,0x2e,0x9f,0xc5,0x4a,0x3b},
  {0x63,0xfd,0xae,0xbe,0xcd,0x82,0x48,0x12,0xa1,0x6e,0x41,0x26,0xcb,0xfa,0xa0,0xc8},
  {0x23,0xbe,0x45,0x29,0x32,0xd2,0x46,0x2d,0x82,0x03,0x52,0x28,0xfe,0x37,0x17,0xf5},
  {0x56,0x3b,0xbf,0xee,0x5a,0x4f,0x43,0x39,0xaf,0xaa,0x94,0x08,0xdf,0xf0,0xfc,0x10},
  {0x31,0x80,0xc8,0x73,0xc7,0xea,0x46,0xd3,0xaa,0x25,0x75,0x0f,0x9e,0x4c,0x09,0x29},
  {0x7d,0xf7,0x18,0x4b,0x7b,0xa4,0x44,0xd5,0x85,0x3c,0x06,0xe0,0x65,0x53,0x96,0x6d},
  {0xf2,0x77,0xe9,0x6f,0x93,0xb5,0x4e,0x71,0x9a,0x0c,0x34,0x88,0x39,0x25,0xbf,0x35}
};


/**
 * _g_kdbus_get_last_msg_sender
 *
 */
gchar *
_g_kdbus_get_last_msg_sender (GKdbus  *kdbus)
{
  return kdbus->priv->msg_sender->str;
}


/**
 * _g_kdbus_get_last_msg_destination
 *
 */
gchar *
_g_kdbus_get_last_msg_destination (GKdbus  *kdbus)
{
  return kdbus->priv->msg_destination->str;
}


/**
 * _g_kdbus_get_last_msg_items:
 *
 */
GSList *
_g_kdbus_get_last_msg_items (GKdbus  *kdbus)
{
  return kdbus->priv->kdbus_msg_items;
}


/**
 * g_kdbus_add_msg_part:
 *
 */
static void
g_kdbus_add_msg_part (GKdbus  *kdbus,
                      gchar   *data,
                      gsize    size)
{
  msg_part* part = g_new (msg_part, 1);
  part->data = data;
  part->size = size;
  kdbus->priv->kdbus_msg_items = g_slist_append(kdbus->priv->kdbus_msg_items, part);
}


/**
 * _g_kdbus_hexdump_all_items:
 *
 */
gchar *
_g_kdbus_hexdump_all_items (GSList  *kdbus_msg_items)
{

  GString *ret;
  gint item = 1;
  ret = g_string_new (NULL);

  while (kdbus_msg_items != NULL)
    {
      g_string_append_printf (ret, "\n  Item %d\n", item);
      g_string_append (ret, _g_dbus_hexdump (((msg_part*)kdbus_msg_items->data)->data, ((msg_part*)kdbus_msg_items->data)->size, 2));

      kdbus_msg_items = g_slist_next(kdbus_msg_items);
      item++;
    }

  return g_string_free (ret, FALSE);
}


/**
 * g_kdbus_finalize:
 *
 */
static void
g_kdbus_finalize (GObject  *object)
{
  GKdbus *kdbus = G_KDBUS (object);

  if (kdbus->priv->kdbus_buffer != NULL)
    munmap (kdbus->priv->kdbus_buffer, KDBUS_POOL_SIZE);

  kdbus->priv->kdbus_buffer = NULL;

  if (kdbus->priv->fd != -1 && !kdbus->priv->closed)
    _g_kdbus_close (kdbus, NULL);

  g_string_free (kdbus->priv->msg_sender, TRUE);
  g_string_free (kdbus->priv->msg_destination, TRUE);

  if (G_OBJECT_CLASS (g_kdbus_parent_class)->finalize)
    (*G_OBJECT_CLASS (g_kdbus_parent_class)->finalize) (object);
}


/**
 * g_kdbus_class_init:
 *
 */
static void
g_kdbus_class_init (GKdbusClass  *klass)
{
  GObjectClass *gobject_class G_GNUC_UNUSED = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GKdbusPrivate));
  gobject_class->finalize = g_kdbus_finalize;
}


/**
 * g_kdbus_initable_iface_init:
 *
 */
static void
g_kdbus_initable_iface_init (GInitableIface  *iface)
{
  iface->init = g_kdbus_initable_init;
}


/**
 * g_kdbus_init:
 *
 */
static void
g_kdbus_init (GKdbus  *kdbus)
{
  kdbus->priv = G_TYPE_INSTANCE_GET_PRIVATE (kdbus, G_TYPE_KDBUS, GKdbusPrivate);

  kdbus->priv->fd = -1;
  kdbus->priv->unique_id = -1;
  kdbus->priv->memfd = -1;

  kdbus->priv->path = NULL;
  kdbus->priv->kdbus_buffer = NULL;
  kdbus->priv->kdbus_msg_items = NULL;

  kdbus->priv->msg_sender = g_string_new (NULL);
  kdbus->priv->msg_destination = g_string_new (NULL);

  kdbus->priv->fds = NULL;
  kdbus->priv->num_fds = 0;

  kdbus->priv->hello_flags = KDBUS_HELLO_ACCEPT_FD;
  kdbus->priv->attach_flags = KDBUS_ATTACH_NAMES;
}


/**
 * g_kdbus_initable_init:
 *
 */
static gboolean
g_kdbus_initable_init (GInitable     *initable,
                       GCancellable  *cancellable,
                       GError       **error)
{
  GKdbus *kdbus;

  g_return_val_if_fail (G_IS_KDBUS (initable), FALSE);

  kdbus = G_KDBUS (initable);

  if (cancellable != NULL)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                           _("Cancellable initialization not supported"));
      return FALSE;
    }

  kdbus->priv->inited = TRUE;

  return TRUE;
}


/**
 * kdbus_source_prepare:
 *
 */
static gboolean
kdbus_source_prepare (GSource  *source,
                      gint     *timeout)
{
  GKdbusSource *kdbus_source = (GKdbusSource *)source;

  if (g_cancellable_is_cancelled (kdbus_source->cancellable))
    return TRUE;

  if (kdbus_source->timeout_time)
    {
      gint64 now;

      now = g_source_get_time (source);

      *timeout = (kdbus_source->timeout_time - now + 999) / 1000;
      if (*timeout < 0)
        {
          kdbus_source->kdbus->priv->timed_out = TRUE;
          *timeout = 0;
          return TRUE;
        }
    }
  else
    *timeout = -1;

  if ((kdbus_source->condition & kdbus_source->pollfd.revents) != 0)
    return TRUE;

  return FALSE;
}


/**
 * kdbus_source_check:
 *
 */
static gboolean
kdbus_source_check (GSource  *source)
{
  gint timeout;

  return kdbus_source_prepare (source, &timeout);
}


/**
 * kdbus_source_dispatch
 *
 */
static gboolean
kdbus_source_dispatch  (GSource      *source,
                        GSourceFunc   callback,
                        gpointer      user_data)
{
  GKdbusSourceFunc func = (GKdbusSourceFunc)callback;
  GKdbusSource *kdbus_source = (GKdbusSource *)source;
  GKdbus *kdbus = kdbus_source->kdbus;
  gboolean ret;

  if (kdbus_source->kdbus->priv->timed_out)
    kdbus_source->pollfd.revents |= kdbus_source->condition & (G_IO_IN | G_IO_OUT);

  ret = (*func) (kdbus,
                 kdbus_source->pollfd.revents & kdbus_source->condition,
                 user_data);

  if (kdbus->priv->timeout)
    kdbus_source->timeout_time = g_get_monotonic_time ()
                               + kdbus->priv->timeout * 1000000;

  else
    kdbus_source->timeout_time = 0;

  return ret;
}


/**
 * kdbus_source_finalize
 *
 */
static void
kdbus_source_finalize (GSource  *source)
{
  GKdbusSource *kdbus_source = (GKdbusSource *)source;
  GKdbus *kdbus;

  kdbus = kdbus_source->kdbus;

  g_object_unref (kdbus);

  if (kdbus_source->cancellable)
    {
      g_cancellable_release_fd (kdbus_source->cancellable);
      g_object_unref (kdbus_source->cancellable);
    }
}


/**
 * kdbus_source_closure_callback:
 *
 */
static gboolean
kdbus_source_closure_callback (GKdbus        *kdbus,
                               GIOCondition   condition,
                               gpointer       data)
{
  GClosure *closure = data;
  GValue params[2] = { G_VALUE_INIT, G_VALUE_INIT };
  GValue result_value = G_VALUE_INIT;
  gboolean result;

  g_value_init (&result_value, G_TYPE_BOOLEAN);

  g_value_init (&params[0], G_TYPE_KDBUS);
  g_value_set_object (&params[0], kdbus);
  g_value_init (&params[1], G_TYPE_IO_CONDITION);
  g_value_set_flags (&params[1], condition);

  g_closure_invoke (closure, &result_value, 2, params, NULL);

  result = g_value_get_boolean (&result_value);
  g_value_unset (&result_value);
  g_value_unset (&params[0]);
  g_value_unset (&params[1]);

  return result;
}


static GSourceFuncs kdbus_source_funcs =
{
  kdbus_source_prepare,
  kdbus_source_check,
  kdbus_source_dispatch,
  kdbus_source_finalize,
  (GSourceFunc)kdbus_source_closure_callback,
};


/**
 * kdbus_source_new:
 *
 */
static GSource *
kdbus_source_new (GKdbus        *kdbus,
                  GIOCondition   condition,
                  GCancellable  *cancellable)
{
  GSource *source;
  GKdbusSource *kdbus_source;

  source = g_source_new (&kdbus_source_funcs, sizeof (GKdbusSource));
  g_source_set_name (source, "GKdbus");
  kdbus_source = (GKdbusSource *)source;

  kdbus_source->kdbus = g_object_ref (kdbus);
  kdbus_source->condition = condition;

  if (g_cancellable_make_pollfd (cancellable,
                                 &kdbus_source->cancel_pollfd))
    {
      kdbus_source->cancellable = g_object_ref (cancellable);
      g_source_add_poll (source, &kdbus_source->cancel_pollfd);
    }

  kdbus_source->pollfd.fd = kdbus->priv->fd;
  kdbus_source->pollfd.events = condition;
  kdbus_source->pollfd.revents = 0;
  g_source_add_poll (source, &kdbus_source->pollfd);

  if (kdbus->priv->timeout)
    kdbus_source->timeout_time = g_get_monotonic_time ()
                               + kdbus->priv->timeout * 1000000;
  else
    kdbus_source->timeout_time = 0;

  return source;
}


/**
 * _g_kdbus_create_source:
 *
 */
GSource *
_g_kdbus_create_source (GKdbus        *kdbus,
                        GIOCondition   condition,
                        GCancellable  *cancellable)
{
  g_return_val_if_fail (G_IS_KDBUS (kdbus) && (cancellable == NULL || G_IS_CANCELLABLE (cancellable)), NULL);

  return kdbus_source_new (kdbus, condition, cancellable);
}


/**
 * _g_kdbus_open:
 * @kdbus: a #GKdbus.
 * @address: path to kdbus bus file.
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Opens file descriptor to kdbus bus control.
 * It is located in /dev/kdbus/uid-name/bus.
 *
 * Returns: TRUE on success.
 */
gboolean
_g_kdbus_open (GKdbus       *kdbus,
               const gchar  *address,
               GError      **error)
{
  g_return_val_if_fail (G_IS_KDBUS (kdbus), FALSE);

  kdbus->priv->fd = open(address, O_RDWR|O_NOCTTY|O_CLOEXEC);

  if (kdbus->priv->fd<0)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, _("Can't open kdbus endpoint"));
      return FALSE;
    }

  kdbus->priv->closed = FALSE;

  return TRUE;
}


/**
 * _g_kdbus_close:
 * @kdbus: a #GKdbus.
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Closes file descriptor to kdbus bus.
 * Disconnect a connection. If the connection's message list is empty,
 * the calls succeeds, closes file descriptor to kdbus bus. Otherwise
 * FALSE is returned without any further side-effects.
 *
 * Returns: TRUE on success.
 *
 */
gboolean
_g_kdbus_close (GKdbus  *kdbus,
                GError **error)
{
  gint res;

  if (kdbus->priv->closed)
    return TRUE; /* Multiple close not an error */

  g_return_val_if_fail (G_IS_KDBUS (kdbus), FALSE);

  if (ioctl(kdbus->priv->fd, KDBUS_CMD_BYEBYE) < 0)
    return FALSE;

  while (1)
    {
      res = close (kdbus->priv->fd);

      if (res == -1)
        {
          if (errno == EINTR)
            continue;

          g_set_error (error, G_IO_ERROR,
                       g_io_error_from_errno (errno),
                       _("Error closing kdbus fd: %s"),
                       g_strerror (errno));
          return FALSE;
        }
      break;
    }

  kdbus->priv->closed = TRUE;
  kdbus->priv->fd = -1;

  return TRUE;
}


/**
 * _g_kdbus_is_closed:
 * @kdbus: a #GKdbus.
 *
 * checks whether a kdbus is closed.
 *
 */
gboolean
_g_kdbus_is_closed (GKdbus  *kdbus)
{
  g_return_val_if_fail (G_IS_KDBUS (kdbus), FALSE);

  return kdbus->priv->closed;
}


/**
 * g_kdbus_generate_local_reply:
 *
 */
static GDBusMessage *
g_kdbus_generate_local_reply (GDBusMessage       *message,
                              GDBusMessageType    message_type,
                              GDBusMessageFlags   message_flags,
                              guint32             message_reply_serial,
                              GVariant           *message_body,
                              const gchar        *error_name)
{
  GDBusMessage *reply;

  reply = g_dbus_message_new ();

  g_dbus_message_set_sender (reply, "org.freedesktop.DBus");
  g_dbus_message_set_message_type (reply, message_type);
  g_dbus_message_set_flags (reply, message_flags);
  g_dbus_message_set_reply_serial (reply, message_reply_serial);

  g_dbus_message_set_body (reply, message_body);

  if (message != NULL)
    g_dbus_message_set_destination (reply, g_dbus_message_get_sender (message));

  if (message_type == G_DBUS_MESSAGE_TYPE_ERROR)
    g_dbus_message_set_error_name (reply, error_name);

  if (G_UNLIKELY (_g_dbus_debug_message ()))
    {
      gchar *s;
      _g_dbus_debug_print_lock ();
      g_print ("========================================================================\n"
               "GDBus-debug:Message:\n"
               "  <<<< RECEIVED LOCAL D-Bus message (N/A bytes)\n");

      s = g_dbus_message_print (reply, 2);
      g_print ("%s", s);
      g_free (s);
      _g_dbus_debug_print_unlock ();
    }

  return reply;
}


/**
 * g_kdbus_generate_local_error:
 *
 */
static void
g_kdbus_generate_local_error (GDBusWorker   *worker,
                              GDBusMessage  *dbus_msg,
                              GVariant      *message_body,
                              gint           error_code)
{
  GDBusMessage *reply;
  GError *error = NULL;
  gchar *dbus_error_name;

  error = g_error_new_literal (G_DBUS_ERROR, error_code, "");
  dbus_error_name = g_dbus_error_encode_gerror (error);

  reply = g_kdbus_generate_local_reply (dbus_msg,
                                        G_DBUS_MESSAGE_TYPE_ERROR,
                                        G_DBUS_MESSAGE_FLAGS_NO_REPLY_EXPECTED,
                                        g_dbus_message_get_serial (dbus_msg),
                                        message_body,
                                        dbus_error_name);
  _g_dbus_worker_queue_or_deliver_received_message (worker, reply);
}


/**
 * g_kdbus_check_signature:
 * Returns: TRUE on success.
 */
static gboolean
g_kdbus_check_signature (GDBusWorker         *worker,
                         GDBusMessage        *dbus_msg,
                         const gchar         *method_name,
                         GVariant            *body,
                         const GVariantType  *type)
{

  if (!g_variant_is_of_type (body, type))
    {
      GString *error_name = g_string_new (NULL);
      g_string_printf (error_name, "Call to %s has wrong args (expected %s)", method_name, g_variant_type_peek_string (type));
      g_kdbus_generate_local_error (worker,
                                    dbus_msg,
                                    g_variant_new ("(s)",error_name->str),
                                    G_DBUS_ERROR_INVALID_ARGS);
      g_string_free (error_name,TRUE);
      return FALSE;
    }
  else
    return TRUE;
}


/**
 * g_kdbus_check_name:
 * Returns: TRUE on success.
 */
static gboolean
g_kdbus_check_name (GDBusWorker   *worker,
                    GDBusMessage  *dbus_msg,
                    const gchar   *name)
{
  if (!g_dbus_is_name (name))
    {
      GString *error_name = g_string_new (NULL);
      g_string_printf (error_name, "Name \"%s\" is not valid", name);
      g_kdbus_generate_local_error (worker,
                                    dbus_msg,
                                    g_variant_new ("(s)",error_name->str),
                                    G_DBUS_ERROR_INVALID_ARGS);
      g_string_free (error_name,TRUE);
      return FALSE;
    }
  else
    return TRUE;
}


/**
 * g_kdbus_translate_request_name_flags:
 *
 */
static void
g_kdbus_translate_request_name_flags (GBusNameOwnerFlags   flags,
                                      guint64             *kdbus_flags)
{
  guint64 new_flags = 0;

  if (flags & G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT)
    new_flags |= KDBUS_NAME_ALLOW_REPLACEMENT;

  if (flags & G_BUS_NAME_OWNER_FLAGS_REPLACE)
    new_flags |= KDBUS_NAME_REPLACE_EXISTING;

  *kdbus_flags = new_flags;
}


/**
 * g_kdbus_NameHasOwner:
 * Returns: TRUE on success.
 */
static gboolean
g_kdbus_NameHasOwner (GKdbus *kdbus, const gchar *name)
{
  struct kdbus_cmd_conn_info *cmd;
  gssize size;
  gint ret;

  if (g_dbus_is_unique_name(name))
    {
       size = G_STRUCT_OFFSET (struct kdbus_cmd_conn_info, name);
       cmd = g_alloca0 (size);
       cmd->id = g_ascii_strtoull (name+3,NULL,10);
    }
  else
    {
       size = G_STRUCT_OFFSET (struct kdbus_cmd_conn_info, name) + strlen(name) + 1;
       cmd = g_alloca0 (size);
       strcpy(cmd->name, name);
    }

  cmd->flags = KDBUS_ATTACH_NAMES;
  cmd->size = size;

  ret = ioctl(kdbus->priv->fd, KDBUS_CMD_CONN_INFO, cmd);

  if (ret<0)
    return FALSE;
  else
    return TRUE;
}


/**
 * g_kdbus_take_fd:
 *
 */
static void
g_kdbus_take_fd (GKdbus  *kdbus)
{
  struct kdbus_cmd_hello *hello;
  struct kdbus_item *item;
  gchar *conn_name;
  size_t size, conn_name_size;

  conn_name = "gdbus-kdbus";
  conn_name_size = strlen (conn_name);

  size = ALIGN8(G_STRUCT_OFFSET(struct kdbus_cmd_hello, items)) +
         ALIGN8(G_STRUCT_OFFSET(struct kdbus_item, str) + conn_name_size + 1);

  hello = g_alloca0(size);
  hello->conn_flags = kdbus->priv->hello_flags;
  hello->attach_flags =  kdbus->priv->attach_flags;
  hello->size = size;
  hello->pool_size = KDBUS_POOL_SIZE;

  /* connection's human-readable name (only for debugging purposes)*/
  item = hello->items;
  item->size = G_STRUCT_OFFSET(struct kdbus_item, str) + conn_name_size + 1;
  item->type = KDBUS_ITEM_CONN_NAME;
  memcpy(item->str, conn_name, conn_name_size+1);
  item = KDBUS_ITEM_NEXT(item);

  if (ioctl(kdbus->priv->fd, KDBUS_CMD_HELLO, hello))
    g_error("[KDBUS] fd=%d failed to send hello: %m, %d", kdbus->priv->fd,errno);

  kdbus->priv->kdbus_buffer = mmap(NULL, KDBUS_POOL_SIZE, PROT_READ, MAP_SHARED, kdbus->priv->fd, 0);

  if (kdbus->priv->kdbus_buffer == MAP_FAILED)
    g_error("[KDBUS] error when mmap: %m, %d", errno);

  if (hello->bus_flags > 0xFFFFFFFFULL || hello->conn_flags > 0xFFFFFFFFULL)
    g_error("[KDBUS] incompatible flags");

  /* read bloom filters parameters */
  kdbus->priv->bloom_size = (gsize) hello->bloom.size;
  kdbus->priv->bloom_n_hash = (guint) hello->bloom.n_hash;

  /* validate bloom filters parameters here? */

  kdbus->priv->unique_id = hello->id;
  memcpy (kdbus->priv->bus_id, hello->id128, 16);
}


/**
 * g_kdbus_Hello_reply:
 * Returns: TRUE on success.
 */
static gboolean
g_kdbus_Hello_reply (GDBusWorker   *worker,
                     GKdbus        *kdbus,
                     GDBusMessage  *dbus_msg)
{
  GString *unique_name;
  GDBusMessage *reply;

  unique_name = g_string_new(NULL);
  g_string_printf (unique_name,":1.%" G_GUINT64_FORMAT, kdbus->priv->unique_id);

  reply = g_kdbus_generate_local_reply (dbus_msg,
                                        G_DBUS_MESSAGE_TYPE_METHOD_RETURN,
                                        G_DBUS_MESSAGE_FLAGS_NO_REPLY_EXPECTED,
                                        g_dbus_message_get_serial (dbus_msg),
                                        g_variant_new ("(s)",unique_name->str),
                                        NULL);
  _g_dbus_worker_queue_or_deliver_received_message (worker, reply);

  g_string_free (unique_name,TRUE);
  return TRUE;
}


/**
 * g_kdbus_RequestName_handler:
 * Returns: TRUE on success.
 */
static gboolean
g_kdbus_RequestName_handler (GDBusWorker   *worker,
                             GKdbus        *kdbus,
                             GDBusMessage  *dbus_msg)
{
  GDBusMessage *reply;
  GBusNameOwnerFlags flags;
  struct kdbus_cmd_name *kdbus_name;
  const gchar *name;
  guint64 kdbus_flags;
  guint64 size;
  gint ret;
  gint status = G_BUS_REQUEST_NAME_REPLY_PRIMARY_OWNER;

  /* read and validate message */
  GVariant *body = g_dbus_message_get_body (dbus_msg);

  if (!g_kdbus_check_signature (worker, dbus_msg, "RequestName", body, G_VARIANT_TYPE("(su)")))
    return TRUE;

  g_variant_get (body, "(&su)", &name, &flags);

  if (!g_kdbus_check_name (worker, dbus_msg, name))
    return TRUE;

  if (*name == ':')
    {
      GString *error_name = g_string_new (NULL);
      g_string_printf (error_name, "Cannot acquire a service starting with ':' such as \"%s\"", name);
      g_kdbus_generate_local_error (worker,
                                    dbus_msg,
                                    g_variant_new ("(s)",error_name->str),
                                    G_DBUS_ERROR_INVALID_ARGS);
      g_string_free (error_name,TRUE);
      return TRUE;
    }

  g_kdbus_translate_request_name_flags (flags, &kdbus_flags);

  /* calculate size */
  size = sizeof(*kdbus_name) + strlen(name) + 1;
  kdbus_name = g_alloca(size);

  /* set message header */
  memset(kdbus_name, 0, size);
  strcpy(kdbus_name->name, name);
  kdbus_name->size = size;
  kdbus_name->flags = kdbus_flags;

  /* send message */
  ret = ioctl(kdbus->priv->fd, KDBUS_CMD_NAME_ACQUIRE, kdbus_name);
  if (ret < 0)
    {
      if (errno == EEXIST)
        status = G_BUS_REQUEST_NAME_REPLY_EXISTS;
      else if (errno == EALREADY)
        status = G_BUS_REQUEST_NAME_REPLY_ALREADY_OWNER;
      else
        return FALSE;
    }

  if (kdbus_name->flags & KDBUS_NAME_IN_QUEUE)
    status = G_BUS_REQUEST_NAME_REPLY_IN_QUEUE;

  /* generate local reply */
  reply = g_kdbus_generate_local_reply (dbus_msg,
                                        G_DBUS_MESSAGE_TYPE_METHOD_RETURN,
                                        G_DBUS_MESSAGE_FLAGS_NO_REPLY_EXPECTED,
                                        g_dbus_message_get_serial (dbus_msg),
                                        g_variant_new ("(u)",status),
                                        NULL);
  _g_dbus_worker_queue_or_deliver_received_message (worker, reply);

  return TRUE;
}


/**
 * g_kdbus_ReleaseName_handler:
 * Returns: TRUE on success.
 */
static gboolean
g_kdbus_ReleaseName_handler (GDBusWorker   *worker,
                             GKdbus        *kdbus,
                             GDBusMessage  *dbus_msg)
{
  GDBusMessage *reply;
  struct kdbus_cmd_name *kdbus_name;
  const gchar *name;
  guint64 size;
  gint ret;
  gint status = G_BUS_RELEASE_NAME_REPLY_RELEASED;

  /* read and validate message */
  GVariant *body = g_dbus_message_get_body (dbus_msg);

  if (!g_kdbus_check_signature (worker, dbus_msg, "ReleaseName", body, G_VARIANT_TYPE("(s)")))
    return TRUE;

  g_variant_get (body, "(&s)", &name);

  if (!g_kdbus_check_name (worker, dbus_msg, name))
    return TRUE;

  if (*name == ':')
    {
      GString *error_name = g_string_new (NULL);
      g_string_printf (error_name, "Cannot release a service starting with ':' such as \"%s\"", name);
      g_kdbus_generate_local_error (worker,
                                    dbus_msg,
                                    g_variant_new ("(s)",error_name->str),
                                    G_DBUS_ERROR_INVALID_ARGS);
      g_string_free (error_name,TRUE);
      return TRUE;
    }

  /* calculate size */
  size = sizeof(*kdbus_name) + strlen(name) + 1;
  kdbus_name = g_alloca(size);

  /* set message header */
  memset(kdbus_name, 0, size);
  strcpy(kdbus_name->name, name);
  kdbus_name->size = size;

  /* send message */
  ret = ioctl(kdbus->priv->fd, KDBUS_CMD_NAME_RELEASE, kdbus_name);
  if (ret < 0)
    {
      if (errno == ESRCH)
        status = G_BUS_RELEASE_NAME_REPLY_NON_EXISTENT;
      else if (errno == EADDRINUSE)
        status = G_BUS_RELEASE_NAME_REPLY_NOT_OWNER;
      else
        return FALSE;
    }

  /* generate local reply */
  reply = g_kdbus_generate_local_reply (dbus_msg,
                                        G_DBUS_MESSAGE_TYPE_METHOD_RETURN,
                                        G_DBUS_MESSAGE_FLAGS_NO_REPLY_EXPECTED,
                                        g_dbus_message_get_serial (dbus_msg),
                                        g_variant_new ("(u)",status),
                                        NULL);
  _g_dbus_worker_queue_or_deliver_received_message (worker, reply);

  return TRUE;
}


/**
 * g_kdbus_ListNames_handler:
 * Returns: TRUE on success.
 */
static gboolean
g_kdbus_ListNames_handler (GDBusWorker   *worker,
                           GKdbus        *kdbus,
                           GDBusMessage  *dbus_msg,
                           guint64        flags)
{
  GDBusMessage *reply;
  GVariantBuilder *builder;
  struct kdbus_cmd_name_list cmd = {};
  struct kdbus_name_list *name_list;
  struct kdbus_cmd_name *name;
  guint64 prev_id = 0;
  gint ret;

  cmd.flags = flags;

  ret = ioctl(kdbus->priv->fd, KDBUS_CMD_NAME_LIST, &cmd);
  if (ret < 0)
    return FALSE;

  /* get name list */
  name_list = (struct kdbus_name_list *) ((guint8 *) kdbus->priv->kdbus_buffer + cmd.offset);

  builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
  KDBUS_ITEM_FOREACH(name, name_list, names)
    {
      if ((flags & KDBUS_NAME_LIST_UNIQUE) && name->owner_id != prev_id)
        {
          GString *unique_name = g_string_new (NULL);

          g_string_printf (unique_name, ":1.%llu", name->owner_id);

          g_variant_builder_add (builder, "s", unique_name->str);
          g_string_free (unique_name,TRUE);
          prev_id = name->owner_id;
        }

        if (g_dbus_is_name (name->name))
          g_variant_builder_add (builder, "s", name->name);
    }

  /* generate local reply */
  reply = g_kdbus_generate_local_reply (dbus_msg,
                                        G_DBUS_MESSAGE_TYPE_METHOD_RETURN,
                                        G_DBUS_MESSAGE_FLAGS_NO_REPLY_EXPECTED,
                                        g_dbus_message_get_serial (dbus_msg),
                                        g_variant_new ("(as)", builder),
                                        NULL);
  _g_dbus_worker_queue_or_deliver_received_message (worker, reply);

  g_variant_builder_unref (builder);
  ret = ioctl(kdbus->priv->fd, KDBUS_CMD_FREE, &cmd.offset);
  if (ret < 0)
    return FALSE;

  return TRUE;
}


/**
 * g_kdbus_ListQueuedOwners_handler:
 * Returns: TRUE on success.
 */
static gboolean
g_kdbus_ListQueuedOwners_handler (GDBusWorker   *worker,
                                  GKdbus        *kdbus,
                                  GDBusMessage  *dbus_msg)
{
  GDBusMessage *reply;
  GString *unique_name;
  GVariantBuilder *builder;
  struct kdbus_cmd_name_list cmd = {};
  struct kdbus_name_list *name_list;
  struct kdbus_cmd_name *name;
  const gchar *service;
  gint ret;

  /* read and validate message */
  GVariant *body = g_dbus_message_get_body (dbus_msg);

  if (!g_kdbus_check_signature (worker, dbus_msg, "ListQueuedOwners", body, G_VARIANT_TYPE("(s)")))
    return TRUE;

  g_variant_get (body, "(&s)", &service);

  if (!g_kdbus_check_name (worker, dbus_msg, service))
    return TRUE;

  if (!g_kdbus_NameHasOwner (kdbus, service))
    {
      GString *error_name = g_string_new (NULL);
      g_string_printf (error_name, "Could not get owners of name \'%s\': no such name", service);
      g_kdbus_generate_local_error (worker,
                                    dbus_msg,
                                    g_variant_new ("(s)",error_name->str),
                                    G_DBUS_ERROR_NAME_HAS_NO_OWNER);
      g_string_free (error_name,TRUE);
      return TRUE;
    }

  /* get queued name list */
  cmd.flags = KDBUS_NAME_LIST_QUEUED;

  ret = ioctl(kdbus->priv->fd, KDBUS_CMD_NAME_LIST, &cmd);
  if (ret < 0)
    return FALSE;

  name_list = (struct kdbus_name_list *) ((guint8 *) kdbus->priv->kdbus_buffer + cmd.offset);

  unique_name = g_string_new (NULL);
  builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
  KDBUS_ITEM_FOREACH(name, name_list, names)
    {
      if (name->size <= sizeof(*name))
        continue;

      if (strcmp(name->name, service))
        continue;

      g_string_printf (unique_name, ":1.%llu", name->owner_id);
      g_variant_builder_add (builder, "s", unique_name);

    }

  /* generate reply */
  reply = g_kdbus_generate_local_reply (dbus_msg,
                                        G_DBUS_MESSAGE_TYPE_METHOD_RETURN,
                                        G_DBUS_MESSAGE_FLAGS_NO_REPLY_EXPECTED,
                                        g_dbus_message_get_serial (dbus_msg),
                                        g_variant_new ("(as)", builder),
                                        NULL);
  _g_dbus_worker_queue_or_deliver_received_message (worker, reply);

  g_variant_builder_unref (builder);
  g_string_free (unique_name,TRUE);

  ret = ioctl(kdbus->priv->fd, KDBUS_CMD_FREE, &cmd.offset);
  if (ret < 0)
    return FALSE;

  return TRUE;
}


/**
 * g_kdbus_GetOwner_handler:
 * Returns: TRUE on success.
 */
static gboolean
g_kdbus_GetOwner_handler (GDBusWorker   *worker,
                          GKdbus        *kdbus,
                          GDBusMessage  *dbus_msg,
                          guint64        flag)
{
  GVariant *result = NULL;
  GDBusMessage *reply;
  struct kdbus_cmd_conn_info *cmd;
  struct kdbus_conn_info *conn_info;
  struct kdbus_item *item;
  const gchar *name;
  gssize size;
  gint ret;

  /* read and validate message */
  GVariant *body = g_dbus_message_get_body (dbus_msg);

  if (!g_kdbus_check_signature (worker, dbus_msg, "GetOwner", body, G_VARIANT_TYPE("(s)")))
    return TRUE;

  g_variant_get (body, "(&s)", &name);

  if (!g_kdbus_check_name (worker, dbus_msg, name))
    return TRUE;

  /* setup kmsg for ioctl */
  if (g_dbus_is_unique_name(name))
    {
       size = G_STRUCT_OFFSET (struct kdbus_cmd_conn_info, name);
       cmd = g_alloca0 (size);
       cmd->id = g_ascii_strtoull (name+3,NULL,10);
    }
  else
    {
       size = G_STRUCT_OFFSET (struct kdbus_cmd_conn_info, name) + strlen(name) + 1;
       cmd = g_alloca0 (size);
       strcpy(cmd->name, name);
    }

  cmd->flags = KDBUS_ATTACH_NAMES;
  cmd->size = size;

  /* get info about connection */
  ret = ioctl(kdbus->priv->fd, KDBUS_CMD_CONN_INFO, cmd);
  if (ret < 0)
    {
      GString *error_name = g_string_new (NULL);
      g_string_printf (error_name, "Could not get owners of name \'%s\': no such name", name);
      g_kdbus_generate_local_error (worker,
                                    dbus_msg,
                                    g_variant_new ("(s)",error_name->str),
                                    G_DBUS_ERROR_NAME_HAS_NO_OWNER);
      g_string_free (error_name, TRUE);
      return TRUE;
    }

  conn_info = (struct kdbus_conn_info *) ((guint8 *) kdbus->priv->kdbus_buffer + cmd->offset);

  if (conn_info->flags & KDBUS_HELLO_ACTIVATOR)
    return FALSE;

  if (flag == G_BUS_CREDS_UNIQUE_NAME)
    {
       GString *unique_name = g_string_new (NULL);

       g_string_printf (unique_name, ":1.%llu", (unsigned long long) conn_info->id);

       result = g_variant_new ("(s)", unique_name);
       g_string_free (unique_name,TRUE);
       goto send_reply;
    }

  /* read creds info */
  KDBUS_ITEM_FOREACH(item, conn_info, items)
   {

      switch (item->type)
        {

          case KDBUS_ITEM_CREDS:

            if (flag == G_BUS_CREDS_PID)
              {
                guint pid = item->creds.pid;
                result = g_variant_new ("(u)", pid);
                goto send_reply;
              }

            if (flag == G_BUS_CREDS_UID)
              {
                guint uid = item->creds.uid;
                result = g_variant_new ("(u)", uid);
                goto send_reply;
              }

          case KDBUS_ITEM_SECLABEL:
            if (flag == G_BUS_CREDS_SELINUX_CONTEXT)
              {
                gint counter;
                gchar *label;
                GVariantBuilder *builder = g_variant_builder_new (G_VARIANT_TYPE ("ay"));

                label = g_strdup (item->str);
                if (!label)
                  goto exit;

                for (counter = 0 ; counter < strlen (label) ; counter++)
                  {
                    g_variant_builder_add (builder, "y", label);
                    label++;
                  }

                result = g_variant_new ("(ay)", builder);
                g_variant_builder_unref (builder);
                g_free (label);
                goto send_reply;

              }
            break;

          case KDBUS_ITEM_PID_COMM:
          case KDBUS_ITEM_TID_COMM:
          case KDBUS_ITEM_EXE:
          case KDBUS_ITEM_CMDLINE:
          case KDBUS_ITEM_CGROUP:
          case KDBUS_ITEM_CAPS:
          case KDBUS_ITEM_NAME:
          case KDBUS_ITEM_AUDIT:
            break;

        }
   }

send_reply:
  if (result == NULL)
    goto exit;

  /* generate local reply */
  reply = g_kdbus_generate_local_reply (dbus_msg,
                                        G_DBUS_MESSAGE_TYPE_METHOD_RETURN,
                                        G_DBUS_MESSAGE_FLAGS_NO_REPLY_EXPECTED,
                                        g_dbus_message_get_serial (dbus_msg),
                                        result,
                                        NULL);
  _g_dbus_worker_queue_or_deliver_received_message (worker, reply);

exit:
  ioctl(kdbus->priv->fd, KDBUS_CMD_FREE, &cmd->offset);
  return TRUE;

}


/**
 * g_kdbus_NameHasOwner_handler:
 * Returns: TRUE on success.
 */
static gboolean
g_kdbus_NameHasOwner_handler (GDBusWorker   *worker,
                              GKdbus        *kdbus,
                              GDBusMessage  *dbus_msg)
{
  GDBusMessage *reply;
  GVariant *result = NULL;
  const gchar *name;

  /* read and validate message */
  GVariant *body = g_dbus_message_get_body (dbus_msg);

  if (!g_kdbus_check_signature (worker, dbus_msg, "NameHasOwner", body, G_VARIANT_TYPE("(s)")))
    return TRUE;

  g_variant_get (body, "(&s)", &name);

  if (!g_kdbus_check_name (worker, dbus_msg, name))
    return TRUE;

  /* check whether name has owner */
  if (!g_kdbus_NameHasOwner (kdbus, name))
    result = g_variant_new ("(b)", FALSE);
  else
    result = g_variant_new ("(b)", TRUE);

  /* generate local reply */
  reply = g_kdbus_generate_local_reply (dbus_msg,
                                        G_DBUS_MESSAGE_TYPE_METHOD_RETURN,
                                        G_DBUS_MESSAGE_FLAGS_NO_REPLY_EXPECTED,
                                        g_dbus_message_get_serial (dbus_msg),
                                        result,
                                        NULL);
  _g_dbus_worker_queue_or_deliver_received_message (worker, reply);
  return TRUE;
}


/**
 * g_kdbus_GetId_handler:
 * Returns: TRUE on success.
 */
static gboolean
g_kdbus_GetId_handler (GDBusWorker   *worker,
                       GKdbus        *kdbus,
                       GDBusMessage  *dbus_msg)
{
  GDBusMessage *reply;
  GString *result = g_string_new (NULL);
  gint i;

  for (i=0; i<16; i++)
    g_string_append_printf (result, "%02x", kdbus->priv->bus_id[i]);

  /* generate local reply */
  reply = g_kdbus_generate_local_reply (dbus_msg,
                                        G_DBUS_MESSAGE_TYPE_METHOD_RETURN,
                                        G_DBUS_MESSAGE_FLAGS_NO_REPLY_EXPECTED,
                                        g_dbus_message_get_serial (dbus_msg),
                                        g_variant_new ("(s)", result->str),
                                        NULL);
  _g_dbus_worker_queue_or_deliver_received_message (worker, reply);

  g_string_free (result,TRUE);
  return TRUE;
}


/**
 * g_kdbus_StartServiceByName_handler:
 * Returns: TRUE on success.
 *
 */
static gboolean
g_kdbus_StartServiceByName_handler (GDBusWorker   *worker,
                                    GKdbus        *kdbus,
                                    GDBusMessage  *dbus_msg)
{
  GDBusMessage *reply;
  GVariant *body;
  const gchar *name;
  guint64 flags;

  body = g_dbus_message_get_body (dbus_msg);

  if (!g_kdbus_check_signature (worker, dbus_msg, "StartServiceByName", body, G_VARIANT_TYPE("(su)")))
    return TRUE;

  g_variant_get (body, "(&su)", &name, &flags);

  if (!g_kdbus_check_name (worker, dbus_msg, name))
    return TRUE;

  if (g_kdbus_NameHasOwner (kdbus, name))
    {
      reply = g_kdbus_generate_local_reply (dbus_msg,
                                            G_DBUS_MESSAGE_TYPE_METHOD_RETURN,
                                            G_DBUS_MESSAGE_FLAGS_NO_REPLY_EXPECTED,
                                            g_dbus_message_get_serial (dbus_msg),
                                            g_variant_new ("(u)", G_BUS_START_REPLY_ALREADY_RUNNING),
                                            NULL);
      _g_dbus_worker_queue_or_deliver_received_message (worker, reply);
      return TRUE;
    }

  /* TODO */
  g_error ("[KDBUS] StartServiceByName method is not implemented yet");

  return TRUE;
}


/**
 * g_kdbus_AddMatch_handler:
 * Returns: TRUE on success.
 *
 */
static gboolean
g_kdbus_AddMatch_handler (GDBusWorker   *worker,
                          GKdbus        *kdbus,
                          GDBusMessage  *dbus_msg)
{
  GVariant *body;
  const gchar *rule;

  body = g_dbus_message_get_body (dbus_msg);

  if (!g_kdbus_check_signature (worker, dbus_msg, "AddMatch", body, G_VARIANT_TYPE("(s)")))
    return TRUE;

  g_variant_get (body, "(&s)", &rule);

  /* TODO */
  g_error ("[KDBUS] AddMatch method is not implemented yet");

  return TRUE;
}


/**
 * g_kdbus_RemoveMatch_handler:
 * Returns: TRUE on success.
 *
 */
static gboolean
g_kdbus_RemoveMatch_handler (GDBusWorker   *worker,
                             GKdbus        *kdbus,
                             GDBusMessage  *dbus_msg)
{
  GVariant *body;
  const gchar *rule;

  body = g_dbus_message_get_body (dbus_msg);

  if (!g_kdbus_check_signature (worker, dbus_msg, "RemoveMatch", body, G_VARIANT_TYPE("(s)")))
    return TRUE;

  g_variant_get (body, "(&s)", &rule);

  /* TODO */
  g_error ("[KDBUS] RemoveMatch method is not implemented yet");

  return TRUE;
}


/**
 * g_kdbus_UnsupportedMethod_handler:
 * Returns: TRUE on success.
 */
static gboolean
g_kdbus_UnsupportedMethod_handler (GDBusWorker   *worker,
                                   GKdbus        *kdbus,
                                   GDBusMessage  *dbus_msg,
                                   const gchar   *method_name)
{
  GString *error_name = g_string_new (NULL);
  g_string_printf (error_name, "Method \"%s\" is not supported", method_name);
  g_kdbus_generate_local_error (worker,
                                dbus_msg,
                                g_variant_new ("(s)",error_name->str),
                                G_DBUS_ERROR_UNKNOWN_METHOD);
  g_string_free (error_name,TRUE);
  return TRUE;
}


/**
 * g_kdbus_bus_driver:
 *
 */
static gboolean
g_kdbus_bus_driver (GDBusWorker   *worker,
                    GKdbus        *kdbus,
                    GDBusMessage  *dbus_msg)
{
  gboolean ret = FALSE;

  /* Hello */
  if (g_strcmp0(g_dbus_message_get_member(dbus_msg), "Hello") == 0)
    {
      g_kdbus_take_fd (kdbus);
      ret = g_kdbus_Hello_reply (worker, kdbus, dbus_msg);
    }

  /* RequestName and ReleaseName */
  else if (g_strcmp0(g_dbus_message_get_member(dbus_msg), "RequestName") == 0)
    ret = g_kdbus_RequestName_handler (worker, kdbus, dbus_msg);
  else if (g_strcmp0(g_dbus_message_get_member(dbus_msg), "ReleaseName") == 0)
    ret = g_kdbus_ReleaseName_handler (worker, kdbus, dbus_msg);

  /* All List* Methods */
  else if (g_strcmp0(g_dbus_message_get_member(dbus_msg), "ListNames") == 0)
    ret = g_kdbus_ListNames_handler (worker, kdbus, dbus_msg, KDBUS_NAME_LIST_UNIQUE | KDBUS_NAME_LIST_NAMES);
  else if (g_strcmp0(g_dbus_message_get_member(dbus_msg), "ListActivatableNames") == 0)
    ret = g_kdbus_ListNames_handler (worker, kdbus, dbus_msg, KDBUS_NAME_LIST_ACTIVATORS);
  else if (g_strcmp0(g_dbus_message_get_member(dbus_msg), "ListQueuedOwners") == 0)
    ret = g_kdbus_ListQueuedOwners_handler (worker, kdbus, dbus_msg);

  /* All Get* Methods */
  else if (g_strcmp0(g_dbus_message_get_member(dbus_msg), "GetNameOwner") == 0)
    ret = g_kdbus_GetOwner_handler (worker, kdbus, dbus_msg, G_BUS_CREDS_UNIQUE_NAME);
  else if (g_strcmp0(g_dbus_message_get_member(dbus_msg), "GetConnectionUnixProcessID") == 0)
    ret = g_kdbus_GetOwner_handler (worker, kdbus, dbus_msg, G_BUS_CREDS_PID);
  else if (g_strcmp0(g_dbus_message_get_member(dbus_msg), "GetConnectionUnixUser") == 0)
    ret = g_kdbus_GetOwner_handler (worker, kdbus, dbus_msg, G_BUS_CREDS_UID);
  else if (g_strcmp0(g_dbus_message_get_member(dbus_msg), "GetConnectionSELinuxSecurityContext") == 0)
    ret = g_kdbus_GetOwner_handler (worker, kdbus, dbus_msg, G_BUS_CREDS_SELINUX_CONTEXT);
  else if (g_strcmp0(g_dbus_message_get_member(dbus_msg), "GetId") == 0)
    ret = g_kdbus_GetId_handler (worker, kdbus, dbus_msg);

  /* NameHasOwner nad StartServiceByName methods */
  else if (g_strcmp0(g_dbus_message_get_member(dbus_msg), "NameHasOwner") == 0)
    ret = g_kdbus_NameHasOwner_handler (worker, kdbus, dbus_msg);
  else if (g_strcmp0(g_dbus_message_get_member(dbus_msg), "StartServiceByName") == 0)
    ret = g_kdbus_StartServiceByName_handler (worker, kdbus, dbus_msg);

  /* AddMatch and RemoveMatch */
  else if (g_strcmp0(g_dbus_message_get_member(dbus_msg), "AddMatch") == 0)
    ret = g_kdbus_AddMatch_handler (worker, kdbus, dbus_msg);
  else if (g_strcmp0(g_dbus_message_get_member(dbus_msg), "RemoveMatch") == 0)
    ret = g_kdbus_RemoveMatch_handler (worker, kdbus, dbus_msg);

  /* Unsupported Methods */
  else if (g_strcmp0(g_dbus_message_get_member(dbus_msg), "ReloadConfig") == 0)
    ret = g_kdbus_UnsupportedMethod_handler (worker, kdbus, dbus_msg, "ReloadConfig");
  else if (g_strcmp0(g_dbus_message_get_member(dbus_msg), "UpdateActivationEnvironment") == 0)
    ret = g_kdbus_UnsupportedMethod_handler (worker, kdbus, dbus_msg, "UpdateActivationEnvironment");

  else
    {
      GString *error_name;

      error_name = g_string_new (NULL);
      g_string_printf (error_name, "org.freedesktop.DBus does not understand message %s", g_dbus_message_get_member(dbus_msg));

      g_kdbus_generate_local_error (worker,
                                    dbus_msg,
                                    g_variant_new ("(s)",error_name->str),
                                    G_DBUS_ERROR_UNKNOWN_METHOD);
      g_string_free (error_name,TRUE);
    }

  return ret;
}


/**
 * g_kdbus_alloc_memfd:
 *
 */
static gboolean
g_kdbus_alloc_memfd (GKdbus  *kdbus)
{
  struct kdbus_cmd_memfd_make *memfd;
  struct kdbus_item *item;
  gssize size;
  gchar *name = "gdbus-memfd";

  size = ALIGN8(G_STRUCT_OFFSET(struct kdbus_cmd_memfd_make, items)) +
         ALIGN8(G_STRUCT_OFFSET(struct kdbus_item, str)) +
         strlen(name) + 1;

  memfd = g_alloca0 (size);
  memfd->size = size;

  item = memfd->items;
  item->size = ALIGN8(offsetof(struct kdbus_item, str)) + strlen(name) + 1;
  item->type = KDBUS_ITEM_MEMFD_NAME;
  memcpy(item->str, name, strlen(name) + 1);

  if (ioctl(kdbus->priv->fd, KDBUS_CMD_MEMFD_NEW, memfd) < 0)
    return FALSE;

  kdbus->priv->memfd = memfd->fd;

  return TRUE;
}


/**
 * _g_kdbus_release_msg:
 * Release memory occupied by kdbus_msg.
 * Use after DBUS message is extracted.
 */
void
_g_kdbus_release_kmsg (GKdbus  *kdbus)
{
  struct kdbus_item *item = NULL;
  GSList *iterator = NULL;
  guint64 offset;

  offset = (guint8 *)kdbus->priv->kmsg - (guint8 *)kdbus->priv->kdbus_buffer;
  ioctl(kdbus->priv->fd, KDBUS_CMD_FREE, &offset);

  for (iterator = kdbus->priv->kdbus_msg_items; iterator; iterator = iterator->next)
    g_free ((msg_part*)iterator->data);

  g_slist_free (kdbus->priv->kdbus_msg_items);
  kdbus->priv->kdbus_msg_items = NULL;

  KDBUS_ITEM_FOREACH (item, kdbus->priv->kmsg, items)
    {
      if (item->type == KDBUS_ITEM_PAYLOAD_MEMFD)
        close(item->memfd.fd);
      else if (item->type == KDBUS_ITEM_FDS)
        {
          gint i;
          gint num_fds = (item->size - G_STRUCT_OFFSET(struct kdbus_item, fds)) / sizeof(int);

          for (i = 0; i < num_fds; i++)
            close(item->fds[i]);
        }
    }
}


/**
 * g_kdbus_append_payload_vec:
 *
 */
static void
g_kdbus_append_payload_vec (struct kdbus_item **item,
                            const void         *data_ptr,
                            gssize              size)
{
  *item = ALIGN8_PTR(*item);
  (*item)->size = G_STRUCT_OFFSET (struct kdbus_item, vec) + sizeof(struct kdbus_vec);
  (*item)->type = KDBUS_ITEM_PAYLOAD_VEC;
  (*item)->vec.address = (guint64)((guintptr)data_ptr);
  (*item)->vec.size = size;
  *item = KDBUS_ITEM_NEXT(*item);
}


/**
 * g_kdbus_append_payload_memfd:
 *
 */
static void
g_kdbus_append_payload_memfd (struct kdbus_item **item,
                              int                 fd,
                              gssize              size)
{
  *item = ALIGN8_PTR(*item);
  (*item)->size = G_STRUCT_OFFSET (struct kdbus_item, memfd) + sizeof(struct kdbus_memfd);
  (*item)->type = KDBUS_ITEM_PAYLOAD_MEMFD;
  (*item)->memfd.fd = fd;
  (*item)->memfd.size = size;
  *item = KDBUS_ITEM_NEXT(*item);
}


/**
 * g_kdbus_append_payload_destiantion:
 *
 */
static void
g_kdbus_append_destination (struct kdbus_item **item,
                            const gchar        *destination,
                            gsize               size)
{
  *item = ALIGN8_PTR(*item);
  (*item)->size = G_STRUCT_OFFSET (struct kdbus_item, str) + size + 1;
  (*item)->type = KDBUS_ITEM_DST_NAME;
  memcpy ((*item)->str, destination, size+1);
  *item = KDBUS_ITEM_NEXT(*item);
}


/**
 * g_kdbus_append_payload_bloom:
 *
 */
static struct kdbus_bloom_filter *
g_kdbus_append_bloom (struct kdbus_item **item,
                      gsize               size)
{
  struct kdbus_item *bloom_item;

  bloom_item = ALIGN8_PTR(*item);
  bloom_item->size = G_STRUCT_OFFSET (struct kdbus_item, bloom_filter) +
                     G_STRUCT_OFFSET (struct kdbus_bloom_filter, data) +
                     size;

  bloom_item->type = KDBUS_ITEM_BLOOM_FILTER;

  *item = KDBUS_ITEM_NEXT(bloom_item);
  return &bloom_item->bloom_filter;
}


/**
 * g_kdbus_append_fds:
 *
 */
static void
g_kdbus_append_fds (struct kdbus_item **item,
                    GUnixFDList        *fd_list)
{
  *item = ALIGN8_PTR(*item);
  (*item)->size = G_STRUCT_OFFSET (struct kdbus_item, fds) + sizeof(int) * g_unix_fd_list_get_length(fd_list);
  (*item)->type = KDBUS_ITEM_FDS;
  memcpy ((*item)->fds, g_unix_fd_list_peek_fds(fd_list, NULL), sizeof(int) * g_unix_fd_list_get_length(fd_list));

  *item = KDBUS_ITEM_NEXT(*item);
}


/**
 * _g_kdbus_attach_fds_to_msg:
 *
 */
void
_g_kdbus_attach_fds_to_msg (GKdbus       *kdbus,
                            GUnixFDList **fd_list)
{
  if ((kdbus->priv->fds != NULL) && (kdbus->priv->num_fds > 0))
    {
      gint n;

      if (*fd_list == NULL)
        *fd_list = g_unix_fd_list_new();

      for (n = 0; n < kdbus->priv->num_fds; n++)
        {
          g_unix_fd_list_append (*fd_list, kdbus->priv->fds[n], NULL);
          (void) g_close (kdbus->priv->fds[n], NULL);
        }

      g_free (kdbus->priv->fds);
      kdbus->priv->fds = NULL;
      kdbus->priv->num_fds = 0;
    }
}


/**
 * g_kdbus_bloom_add_data:
 * Based on bus-bloom.c from systemd
 * http://cgit.freedesktop.org/systemd/systemd/tree/src/libsystemd/sd-bus/bus-bloom.c
 */
static void
g_kdbus_bloom_add_data (GKdbus      *kdbus,
                        guint64      bloom_data [],
                        const void  *data,
                        gsize        n)
{
  guint8 hash[8];
  guint64 bit_num;
  guint bytes_num = 0;
  guint cnt_1, cnt_2;

  guint c = 0;
  guint64 p = 0;

  bit_num = kdbus->priv->bloom_size * 8;

  if (bit_num > 1)
    bytes_num = ((__builtin_clzll(bit_num) ^ 63U) + 7) / 8;

  for (cnt_1 = 0; cnt_1 < (kdbus->priv->bloom_n_hash); cnt_1++)
    {
      for (cnt_2 = 0; cnt_2 < bytes_num; cnt_2++)
        {
          if (c <= 0)
            {
              g_siphash24(hash, data, n, hash_keys[cnt_1++]);
              c += 8;
            }

          p = (p << 8ULL) | (guint64) hash[8 - c];
          c--;
        }

      p &= bit_num - 1;
      bloom_data[p >> 6] |= 1ULL << (p & 63);
    }
}


/**
 * g_kdbus_bloom_add_pair:
 *
 */
static void
g_kdbus_bloom_add_pair (GKdbus       *kdbus,
                        guint64       bloom_data [],
                        const gchar  *parameter,
                        const gchar  *value)
{
  GString *data = g_string_new (NULL);

  g_string_printf (data,"%s:%s",parameter,value);
  g_kdbus_bloom_add_data(kdbus, bloom_data, data->str, data->len);
  g_string_free (data, TRUE);
}


/**
 * g_kdbus_bloom_add_prefixes:
 *
 */
static void
g_kdbus_bloom_add_prefixes (GKdbus       *kdbus,
                            guint64       bloom_data [],
                            const gchar  *parameter,
                            const gchar  *value,
                            gchar         separator)
{
  GString *data = g_string_new (NULL);

  g_string_printf (data,"%s:%s",parameter,value);

  for (;;)
    {
      gchar *last_sep;
      last_sep = strrchr(data->str, separator);
      if (!last_sep || last_sep == data->str)
        break;

      *last_sep = 0;
      g_kdbus_bloom_add_data(kdbus, bloom_data, data->str, last_sep-(data->str));
    }
  g_string_free (data, TRUE);
}


/**
 * g_kdbus_setup_bloom:
 * Based on bus-bloom.c from systemd
 * http://cgit.freedesktop.org/systemd/systemd/tree/src/libsystemd/sd-bus/bus-bloom.c
 */
static void
g_kdbus_setup_bloom (GKdbus                     *kdbus,
                     GDBusMessage               *dbus_msg,
                     struct kdbus_bloom_filter  *bloom_filter)
{
  GVariant *body;
  GVariantIter iter;
  GVariant *child;

  const gchar *message_type;
  const gchar *interface;
  const gchar *member;
  const gchar *path;

  void *bloom_data;
  gint cnt = 0;

  body = g_dbus_message_get_body (dbus_msg);
  message_type = _g_dbus_enum_to_string (G_TYPE_DBUS_MESSAGE_TYPE, g_dbus_message_get_message_type (dbus_msg));
  interface = g_dbus_message_get_interface (dbus_msg);
  member = g_dbus_message_get_member (dbus_msg);
  path = g_dbus_message_get_path (dbus_msg);

  bloom_data = bloom_filter->data;
  memset (bloom_data, 0, kdbus->priv->bloom_size);
  bloom_filter->generation = 0;

  g_kdbus_bloom_add_pair(kdbus, bloom_data, "message-type", message_type);

  if (interface)
    g_kdbus_bloom_add_pair(kdbus, bloom_data, "interface", interface);

  if (member)
    g_kdbus_bloom_add_pair(kdbus, bloom_data, "member", member);

  if (path)
    {
      g_kdbus_bloom_add_pair(kdbus, bloom_data, "path", path);
      g_kdbus_bloom_add_pair(kdbus, bloom_data, "path-slash-prefix", path);
      g_kdbus_bloom_add_prefixes(kdbus, bloom_data, "path-slash-prefix", path, '/');
    }

  if (body != NULL)
    {
      g_variant_iter_init (&iter, body);
      while ((child = g_variant_iter_next_value (&iter)))
        {
          gchar buf[sizeof("arg")-1 + 2 + sizeof("-slash-prefix")];
          gchar *child_string;
          gchar *e;

          /* Is it necessary? */
          //if (g_variant_is_container (child))
          //  iterate_container_recursive (child);

          if (!(g_variant_is_of_type (child, G_VARIANT_TYPE_STRING)) &&
              !(g_variant_is_of_type (child, G_VARIANT_TYPE_OBJECT_PATH)) &&
              !(g_variant_is_of_type (child, G_VARIANT_TYPE_SIGNATURE)))
            break;

          child_string = g_variant_dup_string (child, NULL);

          e = stpcpy(buf, "arg");
          if (cnt < 10)
            *(e++) = '0' + (char) cnt;
          else
            {
              *(e++) = '0' + (char) (cnt / 10);
              *(e++) = '0' + (char) (cnt % 10);
            }

          *e = 0;
          g_kdbus_bloom_add_pair(kdbus, bloom_data, buf, child_string);

          strcpy(e, "-dot-prefix");
          g_kdbus_bloom_add_prefixes(kdbus, bloom_data, buf, child_string, '.');

          strcpy(e, "-slash-prefix");
          g_kdbus_bloom_add_prefixes(kdbus, bloom_data, buf, child_string, '/');

          g_free (child_string);
          g_variant_unref (child);
          cnt++;
        }
    }
}


/**
 * g_kdbus_NameOwnerChanged_generate:
 * TODO: Not tesed yet
 */
static gssize
g_kdbus_NameOwnerChanged_generate (GKdbus             *kdbus,
                                   struct kdbus_item  *item)
{
  GVariant *result = NULL;
  GDBusMessage *reply;
  GError *error;
  guchar *blob;
  gssize reply_size = 0;

  gchar *owner;
  gchar *old_owner;
  gchar *new_owner;

  /* ID change */
  if (item->type == KDBUS_ITEM_ID_ADD || item->type == KDBUS_ITEM_ID_REMOVE)
    {
      owner = "";

      if (item->type == KDBUS_ITEM_ID_ADD)
        {
          old_owner = NULL;
          new_owner = owner;
        }
      else
        {
          old_owner = owner;
          new_owner = NULL;
        }
    }

  /* name change */
  if (item->type == KDBUS_ITEM_NAME_ADD ||
      item->type == KDBUS_ITEM_NAME_REMOVE ||
      item->type == KDBUS_ITEM_NAME_CHANGE )
    {
     g_error ("[KDBUS] 'NameChange' is not implemented yet");
    }

  result = g_variant_new ("(sss)", owner, old_owner, new_owner);
  reply = g_kdbus_generate_local_reply (NULL,
                                        G_DBUS_MESSAGE_TYPE_SIGNAL,
                                        G_DBUS_MESSAGE_FLAGS_NO_REPLY_EXPECTED,
                                        -1,
                                        result,
                                        NULL);

  //_g_dbus_message_set_protocol_ver (reply,2);
  blob =  g_dbus_message_to_blob (reply, (gsize*) &reply_size, 0, &error);
  if (blob == NULL)

    g_error ("[KDBUS] NameOwnerChanged: %s\n",error->message);

  ((guint32 *) blob)[2] = GUINT32_TO_LE (-1);
  g_kdbus_add_msg_part (kdbus, (gchar*)blob, reply_size);

  return reply_size;

}


/**
 * g_kdbus_KernelMethodError_generate:
 *
 */
static gssize
g_kdbus_KernelMethodError_generate (GKdbus             *kdbus,
                                    struct kdbus_item  *item)
{
  GVariant *error_name;
  GDBusMessage *reply;
  GError *error;
  guchar *blob;
  gssize reply_size = 0;

  if (item->type == KDBUS_ITEM_REPLY_TIMEOUT)
    error_name = g_variant_new ("(s)", "Method call timed out");
  else
    error_name = g_variant_new ("(s)", "Method call peer died");

  error = NULL;
  reply = g_kdbus_generate_local_reply (NULL,
                                        G_DBUS_MESSAGE_TYPE_ERROR,
                                        G_DBUS_MESSAGE_FLAGS_NO_REPLY_EXPECTED,
                                        -1,
                                        error_name,
                                        "org.freedesktop.DBus.Error.NoReply");

  //_g_dbus_message_set_protocol_ver (reply,2);
  blob =  g_dbus_message_to_blob (reply, (gsize*) &reply_size, 0, &error);

  if (blob == NULL)
    g_error ("[KDBUS] KernelMethodError: %s\n",error->message);

  ((guint32 *) blob)[2] = GUINT32_TO_LE (-1);
  g_kdbus_add_msg_part (kdbus, (gchar*)blob, reply_size);

  return reply_size;
}


/**
 * g_kdbus_decode_kernel_msg:
 *
 */
static gssize
g_kdbus_decode_kernel_msg (GKdbus  *kdbus)
{
  struct kdbus_item *item = NULL;
  gssize size = 0;

  KDBUS_ITEM_FOREACH(item, kdbus->priv->kmsg, items)
    {
      switch (item->type)
        {
          case KDBUS_ITEM_ID_ADD:
          case KDBUS_ITEM_ID_REMOVE:
          case KDBUS_ITEM_NAME_ADD:
          case KDBUS_ITEM_NAME_REMOVE:
          case KDBUS_ITEM_NAME_CHANGE:
            size = g_kdbus_NameOwnerChanged_generate (kdbus, item);
            break;

          case KDBUS_ITEM_REPLY_TIMEOUT:
          case KDBUS_ITEM_REPLY_DEAD:
            size = g_kdbus_KernelMethodError_generate (kdbus, item);
            break;

          default:
            g_error ("[KDBUS] KERNEL: Unknown filed - %lld", item->type);
        }
    }

  /* Override information from the user header with data from the kernel */
  g_string_printf (kdbus->priv->msg_sender, "org.freedesktop.DBus");

  /* for destination */
  if (kdbus->priv->kmsg->dst_id == KDBUS_DST_ID_BROADCAST)
    /* for broadcast messages we don't have to set destination */
    ;
  else if (kdbus->priv->kmsg->dst_id == KDBUS_DST_ID_NAME)
    g_string_printf (kdbus->priv->msg_destination, ":1.%" G_GUINT64_FORMAT, (guint64) kdbus->priv->unique_id);
  else
    g_string_printf (kdbus->priv->msg_destination, ":1.%" G_GUINT64_FORMAT, (guint64) kdbus->priv->kmsg->dst_id);


  return size;
}


/**
 * g_kdbus_decode_dbus_msg:
 *
 */
static gssize
g_kdbus_decode_dbus_msg (GKdbus  *kdbus)
{
  struct kdbus_item *item;
  gchar *msg_ptr;
  gssize ret_size = 0;
  gssize data_size = 0;
  const gchar *destination = NULL;

  KDBUS_ITEM_FOREACH(item, kdbus->priv->kmsg, items)
    {
      if (item->size <= KDBUS_ITEM_HEADER_SIZE)
        g_error("[KDBUS] %llu bytes - invalid data record\n", item->size);

      data_size = item->size - KDBUS_ITEM_HEADER_SIZE;

      switch (item->type)
        {

         /* KDBUS_ITEM_DST_NAME */
         case KDBUS_ITEM_DST_NAME:
           destination = item->str;
           break;

        /* KDBUS_ITEM_PALOAD_OFF */
        case KDBUS_ITEM_PAYLOAD_OFF:

          msg_ptr = (gchar*) kdbus->priv->kmsg + item->vec.offset;
          g_kdbus_add_msg_part (kdbus, msg_ptr, item->vec.size);
          ret_size += item->vec.size;

          break;

        /* KDBUS_ITEM_PAYLOAD_MEMFD */
        case KDBUS_ITEM_PAYLOAD_MEMFD:

          msg_ptr = mmap(NULL, item->memfd.size, PROT_READ, MAP_SHARED, item->memfd.fd, 0);

          if (msg_ptr == MAP_FAILED)
            {
              g_print ("mmap() fd=%i failed:%m", item->memfd.fd);
              break;
            }

          g_kdbus_add_msg_part (kdbus, msg_ptr, item->memfd.size);
          ret_size += item->memfd.size;

          break;

        /* KDBUS_ITEM_FDS */
        case KDBUS_ITEM_FDS:

          kdbus->priv->num_fds = data_size / sizeof(int);
          kdbus->priv->fds = g_malloc0 (sizeof(int) * kdbus->priv->num_fds);
          memcpy(kdbus->priv->fds, item->fds, sizeof(int) * kdbus->priv->num_fds);

          break;

        /* All of the following items, like CMDLINE,
           CGROUP, etc. need some GDBUS API extensions and
           should be implemented in the future */
        case KDBUS_ITEM_CREDS:
        case KDBUS_ITEM_TIMESTAMP:
        case KDBUS_ITEM_PID_COMM:
        case KDBUS_ITEM_TID_COMM:
        case KDBUS_ITEM_EXE:
        case KDBUS_ITEM_CMDLINE:
        case KDBUS_ITEM_CGROUP:
        case KDBUS_ITEM_AUDIT:
        case KDBUS_ITEM_CAPS:
        case KDBUS_ITEM_SECLABEL:
        case KDBUS_ITEM_CONN_NAME:
        case KDBUS_ITEM_NAME:
          break;

        default:
          g_error ("[KDBUS] DBUS_PAYLOAD: Unknown filed - %lld", item->type);
          break;
        }
    }

  /* Override information from the user header with data from the kernel */

  if (kdbus->priv->kmsg->src_id == KDBUS_SRC_ID_KERNEL)
    g_string_printf (kdbus->priv->msg_sender, "org.freedesktop.DBus");
  else
    g_string_printf (kdbus->priv->msg_sender, ":1.%" G_GUINT64_FORMAT, (guint64) kdbus->priv->kmsg->src_id);

  if (destination)
    g_string_printf (kdbus->priv->msg_destination, "%s", destination);
  else if (kdbus->priv->kmsg->dst_id == KDBUS_DST_ID_BROADCAST)
    /* for broadcast messages we don't have to set destination */
    ;
  else if (kdbus->priv->kmsg->dst_id == KDBUS_DST_ID_NAME)
    g_string_printf (kdbus->priv->msg_destination, ":1.%" G_GUINT64_FORMAT, (guint64) kdbus->priv->unique_id);
  else
    g_string_printf (kdbus->priv->msg_destination, ":1.%" G_GUINT64_FORMAT, (guint64) kdbus->priv->kmsg->dst_id);

  return ret_size;
}


/**
 * _g_kdbus_receive:
 *
 */
gssize
_g_kdbus_receive (GKdbus        *kdbus,
                  GCancellable  *cancellable,
                  GError       **error)
{
  struct kdbus_cmd_recv recv = {};
  gssize size = 0;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return -1;

  again:
    if (ioctl(kdbus->priv->fd, KDBUS_CMD_MSG_RECV, &recv) < 0)
      {
        if (errno == EINTR || errno == EAGAIN)
          goto again;

        g_set_error (error, G_IO_ERROR, g_io_error_from_errno(errno),_("Error receiving message - KDBUS_CMD_MSG_RECV error"));
        return -1;
      }

    kdbus->priv->kmsg = (struct kdbus_msg *)((guint8 *)kdbus->priv->kdbus_buffer + recv.offset);

    if (kdbus->priv->kmsg->payload_type == KDBUS_PAYLOAD_DBUS)
      size = g_kdbus_decode_dbus_msg (kdbus);
    else if (kdbus->priv->kmsg->payload_type == KDBUS_PAYLOAD_KERNEL)
      size = g_kdbus_decode_kernel_msg (kdbus);
    else
      g_error ("[KDBUS] Unknown payload type: %llu", kdbus->priv->kmsg->payload_type);

    return size;
}


/**
 * _g_kdbus_send:
 * Returns: size of data sent or -1 when error
 */
gsize
_g_kdbus_send (GDBusWorker   *worker,
               GKdbus        *kdbus,
               GDBusMessage  *dbus_msg,
               gchar         *blob,
               gsize          blob_size,
               GUnixFDList   *fd_list,
               GCancellable  *cancellable,
               GError       **error)
{
  struct kdbus_msg* kmsg;
  struct kdbus_item *item;
  guint64 kmsg_size = 0;
  const gchar *name;
  gboolean use_memfd = FALSE;
  guint64 dst_id = KDBUS_DST_ID_BROADCAST;

  g_return_val_if_fail (G_IS_KDBUS (kdbus), -1);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return -1;


  /*
   * If systemd-bus-driverd from systemd isn't available
   * try to process the bus driver messages locally
   */
#ifndef SYSTEMD_BUS_DRIVERD
  if (g_strcmp0(g_dbus_message_get_destination(dbus_msg), "org.freedesktop.DBus") == 0)
    {
      if (g_kdbus_bus_driver (worker, kdbus, dbus_msg))
        return blob_size;
      else
        return -1;
    }
#else
    if ((g_strcmp0(g_dbus_message_get_destination(dbus_msg), "org.freedesktop.DBus") == 0) &&
        (g_strcmp0(g_dbus_message_get_member(dbus_msg), "Hello") == 0))
      {
        g_kdbus_take_fd (kdbus);
      }
#endif


  /*
   * check destination
   */
  if ((name = g_dbus_message_get_destination(dbus_msg)))
    {
      dst_id = KDBUS_DST_ID_NAME;
      if ((name[0] == ':') && (name[1] == '1') && (name[2] == '.'))
        {
          dst_id = strtoull(&name[3], NULL, 10);
          name=NULL;
        }
    }


  /*
   * check whether we should use memfd transport (for messages > 512K)
   */
  if (name && (blob_size > 524288))
    use_memfd = TRUE;


  /*
   * check and set message size
   */
  kmsg_size = sizeof(struct kdbus_msg);
  if (use_memfd)
    {
      kmsg_size += KDBUS_ITEM_SIZE(sizeof(struct kdbus_vec));   /* header */
      kmsg_size += KDBUS_ITEM_SIZE(sizeof(struct kdbus_memfd)); /* body */
    }
  else
    kmsg_size += KDBUS_ITEM_SIZE(sizeof(struct kdbus_vec));   /* header + body */

  if (fd_list != NULL && g_unix_fd_list_get_length (fd_list) > 0)
    kmsg_size += ALIGN8(G_STRUCT_OFFSET(struct kdbus_item, fds) + sizeof(int) * g_unix_fd_list_get_length(fd_list));

  if (name)
    kmsg_size += KDBUS_ITEM_SIZE(strlen(name) + 1);
  else if (dst_id == KDBUS_DST_ID_BROADCAST)
    kmsg_size += ALIGN8(G_STRUCT_OFFSET(struct kdbus_item, bloom_filter) +
                        G_STRUCT_OFFSET(struct kdbus_bloom_filter, data) +
                        kdbus->priv->bloom_size);

  kmsg = malloc(kmsg_size);
  if (!kmsg)
    g_error ("[KDBUS] kmsg malloc error");


  /*
   * set message header
   */
  memset(kmsg, 0, kmsg_size);
  kmsg->size = kmsg_size;
  kmsg->payload_type = KDBUS_PAYLOAD_DBUS;
  kmsg->dst_id = name ? 0 : dst_id;
  kmsg->src_id = kdbus->priv->unique_id;
  kmsg->cookie = g_dbus_message_get_serial(dbus_msg);
  kmsg->priority = 0;


  /*
   * set message flags
   */
  kmsg->flags = ((g_dbus_message_get_flags (dbus_msg) & G_DBUS_MESSAGE_FLAGS_NO_REPLY_EXPECTED) ? 0 : KDBUS_MSG_FLAGS_EXPECT_REPLY) |
                ((g_dbus_message_get_flags (dbus_msg) & G_DBUS_MESSAGE_FLAGS_NO_AUTO_START) ? KDBUS_MSG_FLAGS_NO_AUTO_START : 0);

  if ((kmsg->flags) & KDBUS_MSG_FLAGS_EXPECT_REPLY)
    kmsg->timeout_ns = 2000000000;
  else
    kmsg->cookie_reply = g_dbus_message_get_reply_serial(dbus_msg);


  /*
   * append payload
   */
  item = kmsg->items;
  if (use_memfd)
    {
      gint32 body_size;

      if (!g_kdbus_alloc_memfd (kdbus))
        g_error ("Can't alloc memfd");

      /* split blob to header and body */
      memcpy (&body_size, blob+4, 4);
      body_size = GINT32_FROM_LE (body_size);

      /*
       * write blob and seal
       * We should build up whole messsage directly in memfd object without
       * making copy but memfd will be completly reworked soon [1],
       * so we're still waiting for this:
       *
       * [1] https://code.google.com/p/d-bus/source/browse/TODO
       */

      if (write(kdbus->priv->memfd, blob + (blob_size-body_size), body_size) <= 0)
        g_error ("Can't write data to memfd object");

      if (ioctl(kdbus->priv->memfd, KDBUS_CMD_MEMFD_SEAL_SET, 1) < 0)
        g_error ("Can't seal memfd object");

      /* message header in its entirety must be contained in a single PAYLOAD_VEC item */
      g_kdbus_append_payload_vec (&item, blob, blob_size - body_size);
      /* send body as as PAYLOAD_MEMFD item */
      g_kdbus_append_payload_memfd (&item, kdbus->priv->memfd, body_size);
    }
  else
    {
      /* if we don't use memfd, send whole message as a PAYLOAD_VEC item */
      g_kdbus_append_payload_vec (&item, blob, blob_size);
    }


  /*
   * append destination or bloom filters
   */
  if (name)
    g_kdbus_append_destination (&item, name, strlen(name));
  else if (dst_id == KDBUS_DST_ID_BROADCAST)
    {
      struct kdbus_bloom_filter *bloom_filter;

      bloom_filter = g_kdbus_append_bloom (&item, kdbus->priv->bloom_size);
      g_kdbus_setup_bloom (kdbus, dbus_msg, bloom_filter);
    }


  /*
   * append fds if any
   */
  if (fd_list != NULL && g_unix_fd_list_get_length (fd_list) > 0)
    g_kdbus_append_fds (&item, fd_list);


  /*
   * send message
   */
again:
  if (ioctl(kdbus->priv->fd, KDBUS_CMD_MSG_SEND, kmsg))
    {
      GString *error_name;
      error_name = g_string_new (NULL);

      if(errno == EINTR)
        {
          g_string_free (error_name,TRUE);
          goto again;
        }
      else if (errno == ENXIO)
        {
          g_string_printf (error_name, "Name %s does not exist", g_dbus_message_get_destination(dbus_msg));
          g_kdbus_generate_local_error (worker,
                                        dbus_msg,
                                        g_variant_new ("(s)",error_name->str),
                                        G_DBUS_ERROR_SERVICE_UNKNOWN);
          g_string_free (error_name,TRUE);
          return 0;
        }
      else if ((errno == ESRCH) || (errno == EADDRNOTAVAIL))
        {
          if (kmsg->flags & KDBUS_MSG_FLAGS_NO_AUTO_START)
            {
              g_string_printf (error_name, "Name %s does not exist", g_dbus_message_get_destination(dbus_msg));
              g_kdbus_generate_local_error (worker,
                                            dbus_msg,
                                            g_variant_new ("(s)",error_name->str),
                                            G_DBUS_ERROR_SERVICE_UNKNOWN);
              g_string_free (error_name,TRUE);
              return 0;
            }
          else
            {
              g_string_printf (error_name, "The name %s was not provided by any .service files", g_dbus_message_get_destination(dbus_msg));
              g_kdbus_generate_local_error (worker,
                                            dbus_msg,
                                            g_variant_new ("(s)",error_name->str),
                                            G_DBUS_ERROR_SERVICE_UNKNOWN);
              g_string_free (error_name,TRUE);
              return 0;
            }
        }

      g_print ("[KDBUS] ioctl error sending kdbus message:%d (%m)\n",errno);
      g_set_error (error, G_IO_ERROR, g_io_error_from_errno(errno), _("Error sending message - KDBUS_CMD_MSG_SEND error"));
      return -1;
    }

  if (kdbus->priv->memfd >= 0)
    close(kdbus->priv->memfd);

  free(kmsg);

  return blob_size;
}
