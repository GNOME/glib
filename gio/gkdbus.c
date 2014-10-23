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
#include "glibintl.h"
#include "kdbus.h"
#include "gkdbusconnection.h"

#include <gio/gio.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <stdio.h>

#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif

#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#define KDBUS_POOL_SIZE (16 * 1024LU * 1024LU)
#define KDBUS_ALIGN8(l) (((l) + 7) & ~7)
#define KDBUS_ALIGN8_PTR(p) ((void*) (uintptr_t)(p))

#define KDBUS_ITEM_HEADER_SIZE G_STRUCT_OFFSET(struct kdbus_item, data)
#define KDBUS_ITEM_SIZE(s) KDBUS_ALIGN8((s) + KDBUS_ITEM_HEADER_SIZE)

#define KDBUS_ITEM_NEXT(item) \
        (typeof(item))(((guint8 *)item) + KDBUS_ALIGN8((item)->size))
#define KDBUS_ITEM_FOREACH(item, head, first)                    \
        for (item = (head)->first;                               \
             (guint8 *)(item) < (guint8 *)(head) + (head)->size; \
             item = KDBUS_ITEM_NEXT(item))

#define g_alloca0(x) memset(g_alloca(x), '\0', (x))

static void     g_kdbus_initable_iface_init (GInitableIface  *iface);
static gboolean g_kdbus_initable_init       (GInitable       *initable,
                                             GCancellable    *cancellable,
                                             GError         **error);

#define g_kdbus_get_type _g_kdbus_get_type
G_DEFINE_TYPE_WITH_CODE (GKdbus, g_kdbus, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                g_kdbus_initable_iface_init));

/* GBusCredentialsFlags */
typedef enum
{
  G_BUS_CREDS_PID              = 1,
  G_BUS_CREDS_UID              = 2,
  G_BUS_CREDS_UNIQUE_NAME      = 3,
  G_BUS_CREDS_SELINUX_CONTEXT  = 4
} GBusCredentialsFlags;

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

/* GKdbusPrivate struct */
struct _GKdbusPrivate
{
  gint               fd;

  gchar             *kdbus_buffer;
  struct kdbus_msg  *kmsg;

  gchar             *unique_name;
  guint64            unique_id;

  guint64            hello_flags;
  guint64            attach_flags;

  guint              closed : 1;
  guint              inited : 1;
  guint              timeout;
  guint              timed_out : 1;

  guchar             bus_id[16];
};

/* GKdbusSource struct */
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
  kdbus->priv->unique_name = NULL;

  kdbus->priv->kdbus_buffer = NULL;

  kdbus->priv->hello_flags = 0; /* KDBUS_HELLO_ACCEPT_FD */
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
 *
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
 * g_kdbus_free_data:
 *
 */
static gboolean
g_kdbus_free_data (GKdbus      *kdbus,
                   guint64      offset)
{
  struct kdbus_cmd_free cmd;
  int ret;

  cmd.offset = offset;
  cmd.flags = 0;

  ret = ioctl (kdbus->priv->fd, KDBUS_CMD_FREE, &cmd);
  if (ret < 0)
      return FALSE;

  return TRUE;
}


/**
 * g_kdbus_translate_nameowner_flags:
 *
 */
static void
g_kdbus_translate_nameowner_flags (GBusNameOwnerFlags   flags,
                                   guint64             *kdbus_flags)
{
  guint64 new_flags;

  new_flags = 0;

  if (flags & G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT)
    new_flags |= KDBUS_NAME_ALLOW_REPLACEMENT;

  if (flags & G_BUS_NAME_OWNER_FLAGS_REPLACE)
    new_flags |= KDBUS_NAME_REPLACE_EXISTING;

  if (!(flags & G_BUS_NAME_OWNER_FLAGS_DO_NOT_QUEUE))
    new_flags |= KDBUS_NAME_QUEUE;

  *kdbus_flags = new_flags;
}


/**
 * _g_kdbus_close:
 *
 */
gboolean
_g_kdbus_close (GKdbus  *kdbus,
                GError **error)
{
  gint res;

  g_return_val_if_fail (G_IS_KDBUS (kdbus), FALSE);

  if (kdbus->priv->closed)
    return TRUE;

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
 *
 */
gboolean
_g_kdbus_is_closed (GKdbus  *kdbus)
{
  g_return_val_if_fail (G_IS_KDBUS (kdbus), FALSE);

  return kdbus->priv->closed;
}


/**
 * _g_kdbus_Hello:
 *
 */
GVariant *
_g_kdbus_Hello (GIOStream  *stream,
                GError    **error)
{
  GKdbus *kdbus;
  struct kdbus_cmd_hello *hello;
  struct kdbus_item *item;

  gchar *conn_name;
  size_t size, conn_name_size;

  kdbus = _g_kdbus_connection_get_kdbus (G_KDBUS_CONNECTION (stream));

  conn_name = "gdbus-kdbus";
  conn_name_size = strlen (conn_name);

  size = KDBUS_ALIGN8 (G_STRUCT_OFFSET (struct kdbus_cmd_hello, items)) +
         KDBUS_ALIGN8 (G_STRUCT_OFFSET (struct kdbus_item, str) + conn_name_size + 1);

  hello = g_alloca0 (size);
  hello->conn_flags = kdbus->priv->hello_flags;
  hello->attach_flags =  kdbus->priv->attach_flags;
  hello->size = size;
  hello->pool_size = KDBUS_POOL_SIZE;

  item = hello->items;
  item->size = G_STRUCT_OFFSET (struct kdbus_item, str) + conn_name_size + 1;
  item->type = KDBUS_ITEM_CONN_NAME;
  memcpy (item->str, conn_name, conn_name_size+1);
  item = KDBUS_ITEM_NEXT (item);

  if (ioctl(kdbus->priv->fd, KDBUS_CMD_HELLO, hello))
    {
      g_set_error (error, G_IO_ERROR,
                   g_io_error_from_errno (errno),
                   _("Failed to send HELLO: %s"),
                   g_strerror (errno));
      return NULL;
    }

  kdbus->priv->kdbus_buffer = mmap(NULL, KDBUS_POOL_SIZE, PROT_READ, MAP_SHARED, kdbus->priv->fd, 0);
  if (kdbus->priv->kdbus_buffer == MAP_FAILED)
    {
      g_set_error (error, G_IO_ERROR,
                   g_io_error_from_errno (errno),
                   _("mmap error: %s"),
                   g_strerror (errno));
      return NULL;
    }

  if (hello->bus_flags > 0xFFFFFFFFULL)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           _("Incompatible HELLO flags"));
      return NULL;
    }

  memcpy (kdbus->priv->bus_id, hello->id128, 16);

  kdbus->priv->unique_id = hello->id;
  asprintf(&kdbus->priv->unique_name, ":1.%llu", (unsigned long long) hello->id);

  return g_variant_new ("(s)", kdbus->priv->unique_name);
}


/*
 * _g_kdbus_RequestName:
 *
 */
GVariant *
_g_kdbus_RequestName (GDBusConnection     *connection,
                      const gchar         *name,
                      GBusNameOwnerFlags   flags,
                      GError             **error)
{
  GKdbus *kdbus;
  GVariant *result;
  struct kdbus_cmd_name *kdbus_name;
  guint64 kdbus_flags;
  gssize len, size;
  gint status, ret;

  status = G_BUS_REQUEST_NAME_REPLY_PRIMARY_OWNER;

  kdbus = _g_kdbus_connection_get_kdbus (G_KDBUS_CONNECTION (g_dbus_connection_get_stream (connection)));
  if (kdbus == NULL)
    {
      g_set_error_literal (error,
                           G_DBUS_ERROR,
                           G_DBUS_ERROR_IO_ERROR,
                           _("The connection is closed"));
      return NULL;
    }

  if (!g_dbus_is_name (name))
    {
      g_set_error (error,
                   G_DBUS_ERROR,
                   G_DBUS_ERROR_INVALID_ARGS,
                   "Given bus name \"%s\" is not valid", name);
      return NULL;
    }

  if (*name == ':')
    {
      g_set_error (error,
                   G_DBUS_ERROR,
                   G_DBUS_ERROR_INVALID_ARGS,
                   "Cannot acquire a service starting with ':' such as \"%s\"", name);
      return NULL;
    }

  g_kdbus_translate_nameowner_flags (flags, &kdbus_flags);

  len = strlen(name) + 1;
  size = G_STRUCT_OFFSET (struct kdbus_cmd_name, items) + KDBUS_ITEM_SIZE(len);
  kdbus_name = g_alloca0 (size);
  kdbus_name->size = size;
  kdbus_name->items[0].size = KDBUS_ITEM_HEADER_SIZE + len;
  kdbus_name->items[0].type = KDBUS_ITEM_NAME;
  kdbus_name->flags = kdbus_flags;
  memcpy (kdbus_name->items[0].str, name, len);

  ret = ioctl(kdbus->priv->fd, KDBUS_CMD_NAME_ACQUIRE, kdbus_name);
  if (ret < 0)
    {
      if (errno == EEXIST)
        status = G_BUS_REQUEST_NAME_REPLY_EXISTS;
      else if (errno == EALREADY)
        status = G_BUS_REQUEST_NAME_REPLY_ALREADY_OWNER;
      else
        {
          g_set_error (error, G_IO_ERROR,
                       g_io_error_from_errno (errno),
                       _("Error while acquiring name: %s"),
                       g_strerror (errno));
          return NULL;
        }
    }

  if (kdbus_name->flags & KDBUS_NAME_IN_QUEUE)
    status = G_BUS_REQUEST_NAME_REPLY_IN_QUEUE;

  result = g_variant_new ("(u)", status);

  return result;
}


/*
 * _g_kdbus_ReleaseName:
 *
 */
GVariant *
_g_kdbus_ReleaseName (GDBusConnection     *connection,
                      const gchar         *name,
                      GError             **error)
{
  GKdbus *kdbus;
  GVariant *result;
  struct kdbus_cmd_name *kdbus_name;
  gssize len, size;
  gint status, ret;

  status = G_BUS_RELEASE_NAME_REPLY_RELEASED;

  kdbus = _g_kdbus_connection_get_kdbus (G_KDBUS_CONNECTION (g_dbus_connection_get_stream (connection)));
  if (kdbus == NULL)
    {
      g_set_error_literal (error,
                           G_DBUS_ERROR,
                           G_DBUS_ERROR_IO_ERROR,
                           _("The connection is closed"));
      return NULL;
    }

  if (!g_dbus_is_name (name))
    {
      g_set_error (error,
                   G_DBUS_ERROR,
                   G_DBUS_ERROR_INVALID_ARGS,
                   "Given bus name \"%s\" is not valid", name);
      return NULL;
    }

  if (*name == ':')
    {
      g_set_error (error,
                   G_DBUS_ERROR,
                   G_DBUS_ERROR_INVALID_ARGS,
                   "Cannot release a service starting with ':' such as \"%s\"", name);
      return NULL;
    }

  len = strlen(name) + 1;
  size = G_STRUCT_OFFSET (struct kdbus_cmd_name, items) + KDBUS_ITEM_SIZE(len);
  kdbus_name = g_alloca0 (size);
  kdbus_name->size = size;
  kdbus_name->items[0].size = KDBUS_ITEM_HEADER_SIZE + len;
  kdbus_name->items[0].type = KDBUS_ITEM_NAME;
  memcpy (kdbus_name->items[0].str, name, len);

  ret = ioctl(kdbus->priv->fd, KDBUS_CMD_NAME_RELEASE, kdbus_name);
  if (ret < 0)
    {
      if (errno == ESRCH)
        status = G_BUS_RELEASE_NAME_REPLY_NON_EXISTENT;
      else if (errno == EADDRINUSE)
        status = G_BUS_RELEASE_NAME_REPLY_NOT_OWNER;
      else
        {
          g_set_error (error, G_IO_ERROR,
                       g_io_error_from_errno (errno),
                       _("Error while releasing name: %s"),
                       g_strerror (errno));
          return NULL;
        }
    }

  result = g_variant_new ("(u)", status);

  return result;
}


/**
 * _g_kdbus_GetBusId:
 *
 */
GVariant *
_g_kdbus_GetBusId (GDBusConnection  *connection,
                   GError          **error)
{
  GKdbus   *kdbus;
  GVariant *result;
  GString  *result_str;
  guint     cnt;

  result_str = g_string_new (NULL);
  kdbus = _g_kdbus_connection_get_kdbus (G_KDBUS_CONNECTION (g_dbus_connection_get_stream (connection)));
  if (kdbus == NULL)
    {
      g_set_error_literal (error,
                           G_DBUS_ERROR,
                           G_DBUS_ERROR_IO_ERROR,
                           _("The connection is closed"));
      g_string_free (result_str, TRUE);
      return NULL;
    }

  for (cnt=0; cnt<16; cnt++)
    g_string_append_printf (result_str, "%02x", kdbus->priv->bus_id[cnt]);

  result = g_variant_new ("(s)", result_str->str);
  g_string_free (result_str, TRUE);

  return result;
}


/**
 * _g_kdbus_GetListNames:
 *
 */
GVariant *
_g_kdbus_GetListNames (GDBusConnection  *connection,
                       guint             list_name_type,
                       GError          **error)
{
  GKdbus *kdbus;
  GVariant *result;
  GVariantBuilder *builder;

  struct kdbus_cmd_name_list cmd = {};
  struct kdbus_name_list *name_list;
  struct kdbus_cmd_name *name;

  guint64 prev_id;
  gint ret;

  prev_id = 0;
  kdbus = _g_kdbus_connection_get_kdbus (G_KDBUS_CONNECTION (g_dbus_connection_get_stream (connection)));
  if (kdbus == NULL)
    {
      g_set_error_literal (error,
                           G_DBUS_ERROR,
                           G_DBUS_ERROR_IO_ERROR,
                           _("The connection is closed"));
      return NULL;
    }

  if (list_name_type)
    cmd.flags = KDBUS_NAME_LIST_ACTIVATORS;                     /* ListActivatableNames */
  else
    cmd.flags = KDBUS_NAME_LIST_UNIQUE | KDBUS_NAME_LIST_NAMES; /* ListNames */

  ret = ioctl(kdbus->priv->fd, KDBUS_CMD_NAME_LIST, &cmd);
  if (ret < 0)
    {
      g_set_error (error,
                   G_DBUS_ERROR,
                   G_DBUS_ERROR_FAILED,
                   _("Error listing names"));
      return NULL;
    }

  name_list = (struct kdbus_name_list *) ((guint8 *) kdbus->priv->kdbus_buffer + cmd.offset);
  builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));

  KDBUS_ITEM_FOREACH(name, name_list, names)
    {
      struct kdbus_item *item;
      const gchar *item_name = "";

      if ((cmd.flags & KDBUS_NAME_LIST_UNIQUE) && name->owner_id != prev_id)
        {
          GString *unique_name;

          unique_name = g_string_new (NULL);
          g_string_printf (unique_name, ":1.%llu", name->owner_id);
          g_variant_builder_add (builder, "s", unique_name->str);
          g_string_free (unique_name,TRUE);
          prev_id = name->owner_id;
        }

       KDBUS_ITEM_FOREACH(item, name, items)
         if (item->type == KDBUS_ITEM_NAME)
           item_name = item->str;

        if (g_dbus_is_name (item_name))
          g_variant_builder_add (builder, "s", item_name);
    }

  result = g_variant_new ("(as)", builder);
  g_variant_builder_unref (builder);

  g_kdbus_free_data (kdbus, cmd.offset);
  return result;
}


/**
 * _g_kdbus_NameHasOwner_internal:
 *
 */
static gboolean
g_kdbus_NameHasOwner_internal (GKdbus       *kdbus,
                               const gchar  *name,
                               GError      **error)
{
  struct kdbus_cmd_conn_info *cmd;
  gssize size, len;
  gint ret;

  if (g_dbus_is_unique_name(name))
    {
       size = G_STRUCT_OFFSET (struct kdbus_cmd_conn_info, items);
       cmd = g_alloca0 (size);
       cmd->id = g_ascii_strtoull (name+3, NULL, 10);
    }
  else
    {
       len = strlen(name) + 1;
       size = G_STRUCT_OFFSET (struct kdbus_cmd_conn_info, items) + KDBUS_ITEM_SIZE(len);
       cmd = g_alloca0 (size);
       cmd->items[0].size = KDBUS_ITEM_HEADER_SIZE + len;
       cmd->items[0].type = KDBUS_ITEM_NAME;
       memcpy (cmd->items[0].str, name, len);
    }
  cmd->size = size;

  ret = ioctl(kdbus->priv->fd, KDBUS_CMD_CONN_INFO, cmd);
  g_kdbus_free_data (kdbus, cmd->offset);

  if (ret < 0)
    return FALSE;
  else
    return TRUE;
}


/**
 * _g_kdbus_GetListQueuedOwners:
 *
 */
GVariant *
_g_kdbus_GetListQueuedOwners (GDBusConnection  *connection,
                              const gchar      *name,
                              GError          **error)
{
  GKdbus *kdbus;
  GVariant *result;
  GVariantBuilder *builder;
  GString *unique_name;
  gint ret;

  struct kdbus_cmd_name_list cmd = {};
  struct kdbus_name_list *name_list;
  struct kdbus_cmd_name *kname;

  kdbus = _g_kdbus_connection_get_kdbus (G_KDBUS_CONNECTION (g_dbus_connection_get_stream (connection)));
  if (kdbus == NULL)
    {
      g_set_error_literal (error,
                           G_DBUS_ERROR,
                           G_DBUS_ERROR_IO_ERROR,
                           _("The connection is closed"));
      return NULL;
    }

  if (!g_dbus_is_name (name))
    {
      g_set_error (error,
                   G_DBUS_ERROR,
                   G_DBUS_ERROR_INVALID_ARGS,
                   "Given bus name \"%s\" is not valid", name);
      return NULL;
    }

  if (!g_kdbus_NameHasOwner_internal (kdbus, name, error))
    {
      g_set_error (error,
                   G_DBUS_ERROR,
                   G_DBUS_ERROR_NAME_HAS_NO_OWNER,
                   "Could not get owner of name '%s': no such name", name);
      return NULL;
    }

  cmd.flags = KDBUS_NAME_LIST_QUEUED;
  ret = ioctl(kdbus->priv->fd, KDBUS_CMD_NAME_LIST, &cmd);
  if (ret < 0)
    {
      g_set_error (error,
                   G_DBUS_ERROR,
                   G_DBUS_ERROR_FAILED,
                   _("Error listing names"));
      return NULL;
    }

  name_list = (struct kdbus_name_list *) ((guint8 *) kdbus->priv->kdbus_buffer + cmd.offset);

  unique_name = g_string_new (NULL);
  builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
  KDBUS_ITEM_FOREACH(kname, name_list, names)
    {
      struct kdbus_item *item;
      const char *item_name = "";

      KDBUS_ITEM_FOREACH(item, kname, items)
        if (item->type == KDBUS_ITEM_NAME)
          item_name = item->str;

      if (strcmp(item_name, name))
        continue;

      g_string_printf (unique_name, ":1.%llu", kname->owner_id);
      g_variant_builder_add (builder, "s", item_name);
    }

  result = g_variant_new ("(as)", builder);
  g_variant_builder_unref (builder);
  g_string_free (unique_name,TRUE);

  g_kdbus_free_data (kdbus, cmd.offset);
  return result;
}


/**
 * _g_kdbus_NameHasOwner:
 *
 */
GVariant *
_g_kdbus_NameHasOwner (GDBusConnection  *connection,
                       const gchar      *name,
                       GError          **error)
{
  GKdbus *kdbus;
  GVariant *result;

  kdbus = _g_kdbus_connection_get_kdbus (G_KDBUS_CONNECTION (g_dbus_connection_get_stream (connection)));
  if (kdbus == NULL)
    {
      g_set_error_literal (error,
                           G_DBUS_ERROR,
                           G_DBUS_ERROR_IO_ERROR,
                           _("The connection is closed"));
      return NULL;
    }

  if (!g_dbus_is_name (name))
    {
      g_set_error (error,
                   G_DBUS_ERROR,
                   G_DBUS_ERROR_INVALID_ARGS,
                   "Given bus name \"%s\" is not valid", name);
      return NULL;
    }

  if (!g_kdbus_NameHasOwner_internal (kdbus, name, error))
    result = g_variant_new ("(b)", FALSE);
  else
    result = g_variant_new ("(b)", TRUE);

  return result;
}


/**
 * g_kdbus_GetConnInfo_internal:
 *
 */
static GVariant *
g_kdbus_GetConnInfo_internal (GDBusConnection  *connection,
                              const gchar      *name,
                              guint64           flag,
                              GError          **error)
{
  GKdbus *kdbus;
  GVariant *result;

  struct kdbus_cmd_conn_info *cmd;
  struct kdbus_conn_info *conn_info;
  struct kdbus_item *item;
  gssize size, len;
  gint ret;

  result = NULL;
  kdbus = _g_kdbus_connection_get_kdbus (G_KDBUS_CONNECTION (g_dbus_connection_get_stream (connection)));
  if (kdbus == NULL)
    {
      g_set_error_literal (error,
                           G_DBUS_ERROR,
                           G_DBUS_ERROR_IO_ERROR,
                           _("The connection is closed"));
      return NULL;
    }

  if (!g_dbus_is_name (name))
    {
      g_set_error (error,
                   G_DBUS_ERROR,
                   G_DBUS_ERROR_INVALID_ARGS,
                   "Given bus name \"%s\" is not valid", name);
      return NULL;
    }

  if (!g_kdbus_NameHasOwner_internal (kdbus, name, error))
    {
      g_set_error (error,
                   G_DBUS_ERROR,
                   G_DBUS_ERROR_NAME_HAS_NO_OWNER,
                   "Could not get owner of name '%s': no such name", name);
      return NULL;
    }

  if (g_dbus_is_unique_name(name))
    {
       size = G_STRUCT_OFFSET (struct kdbus_cmd_conn_info, items);
       cmd = g_alloca0 (size);
       cmd->id = g_ascii_strtoull (name+3, NULL, 10);
    }
  else
    {
       len = strlen(name) + 1;
       size = G_STRUCT_OFFSET (struct kdbus_cmd_conn_info, items) + KDBUS_ITEM_SIZE(len);
       cmd = g_alloca0 (size);
       cmd->items[0].size = KDBUS_ITEM_HEADER_SIZE + len;
       cmd->items[0].type = KDBUS_ITEM_NAME;
       memcpy (cmd->items[0].str, name, len);
    }

  cmd->flags = KDBUS_ATTACH_NAMES;
  cmd->size = size;

  ret = ioctl(kdbus->priv->fd, KDBUS_CMD_CONN_INFO, cmd);
  if (ret < 0)
    {
      g_set_error (error,
                   G_DBUS_ERROR,
                   G_DBUS_ERROR_FAILED,
                   _("Could not get connection info"));
      return NULL;
    }

  conn_info = (struct kdbus_conn_info *) ((guint8 *) kdbus->priv->kdbus_buffer + cmd->offset);

  /*
  if (conn_info->flags & KDBUS_HELLO_ACTIVATOR)
    {}
  */

  if (flag == G_BUS_CREDS_UNIQUE_NAME)
    {
       GString *unique_name;

       unique_name = g_string_new (NULL);
       g_string_printf (unique_name, ":1.%llu", (unsigned long long) conn_info->id);
       result = g_variant_new ("(s)", unique_name->str);
       g_string_free (unique_name,TRUE);
       goto exit;
    }

  KDBUS_ITEM_FOREACH(item, conn_info, items)
   {
      switch (item->type)
        {
          case KDBUS_ITEM_CREDS:

            if (flag == G_BUS_CREDS_PID)
              {
                guint pid = item->creds.pid;
                result = g_variant_new ("(u)", pid);
                goto exit;
               }

            if (flag == G_BUS_CREDS_UID)
              {
                guint uid = item->creds.uid;
                result = g_variant_new ("(u)", uid);
                goto exit;
              }

          case KDBUS_ITEM_SECLABEL:
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

exit:
  g_kdbus_free_data (kdbus, cmd->offset);
  return result;
}


/**
 * _g_kdbus_GetNameOwner:
 *
 */
GVariant *
_g_kdbus_GetNameOwner (GDBusConnection  *connection,
                       const gchar      *name,
                       GError          **error)
{
  return g_kdbus_GetConnInfo_internal (connection,
                                       name,
                                       G_BUS_CREDS_UNIQUE_NAME,
                                       error);
}


/**
 * _g_kdbus_GetConnectionUnixProcessID:
 *
 */
GVariant *
_g_kdbus_GetConnectionUnixProcessID (GDBusConnection  *connection,
                                     const gchar      *name,
                                     GError          **error)
{
  return g_kdbus_GetConnInfo_internal (connection,
                                       name,
                                       G_BUS_CREDS_PID,
                                       error);
}


/**
 * _g_kdbus_GetConnectionUnixUser:
 *
 */
GVariant *
_g_kdbus_GetConnectionUnixUser (GDBusConnection  *connection,
                                const gchar      *name,
                                GError          **error)
{
  return g_kdbus_GetConnInfo_internal (connection,
                                       name,
                                       G_BUS_CREDS_UID,
                                       error);
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
     //size = g_kdbus_decode_dbus_msg (kdbus);
     g_print ("Standard message\n");
   else if (kdbus->priv->kmsg->payload_type == KDBUS_PAYLOAD_KERNEL)
     //size = g_kdbus_decode_kernel_msg (kdbus);
     g_print ("Message from kernel\n");
   else
     //g_set_error
     g_error ("Unknown payload type: %llu", kdbus->priv->kmsg->payload_type);

   return size;
}
