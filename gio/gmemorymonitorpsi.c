/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2025 Red Hat, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 */

#include "config.h"

#include "gcancellable.h"
#include "gdbusnamewatching.h"
#include "gdbusproxy.h"
#include "ginitable.h"
#include "gioerror.h"
#include "giomodule-priv.h"
#include "glibintl.h"
#include "glib/glib-private.h"
#include "glib/gstdio.h"
#include "gmemorymonitor.h"
#include "gmemorymonitorbase.h"
#include "gmemorymonitorpsi.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

/**
 * GMemoryMonitorPsi:
 *
 * A Linux [iface@Gio.MemoryMonitor] which uses the kernel
 * [pressure stall information](https://www.kernel.org/doc/html/latest/accounting/psi.html) (PSI).
 *
 * When it receives a PSI event, it emits
 * [signal@Gio.MemoryMonitor::low-memory-warning] with an appropriate warning
 * level.
 *
 * Since: 2.86
 */

/* Unprivileged users can also create monitors, with
 * the only limitation that the window size must be a
 * `multiple of 2s`, in order to prevent excessive resource usage.
 * see: https://www.kernel.org/doc/html/latest/accounting/psi.html*/
#define PSI_WINDOW_SEC 2

typedef enum {
  PROP_PROC_PATH = 1,
} GMemoryMonitorPsiProperty;

typedef enum
{
  MEMORY_PRESSURE_MONITOR_TRIGGER_SOME,
  MEMORY_PRESSURE_MONITOR_TRIGGER_FULL,
  MEMORY_PRESSURE_MONITOR_TRIGGER_MFD
} MemoryPressureMonitorTriggerType;

/* Each trigger here results in an open fd for the lifetime
 * of the `GMemoryMonitor`, so don’t add too many */
static const struct
{
  MemoryPressureMonitorTriggerType trigger_type;
  int threshold_ms;
} triggers[G_MEMORY_MONITOR_LOW_MEMORY_LEVEL_COUNT] = {
  { MEMORY_PRESSURE_MONITOR_TRIGGER_SOME, 70 },  /* 70ms out of 2sec for partial stall */
  { MEMORY_PRESSURE_MONITOR_TRIGGER_SOME, 100 }, /* 100ms out of 2sec for partial stall */
  { MEMORY_PRESSURE_MONITOR_TRIGGER_FULL, 100 }, /* 100ms out of 2sec for complete stall */
};

typedef struct
{
  GSource source;
  GPollFD *pollfd;
  GMemoryMonitorLowMemoryLevel level_type;
  GWeakRef monitor_weak;
} MemoryMonitorSource;

typedef gboolean (*MemoryMonitorCallbackFunc) (GMemoryMonitorPsi            *monitor,
                                               GMemoryMonitorLowMemoryLevel  level_type,
                                               void                         *user_data);

#define G_MEMORY_MONITOR_PSI_GET_INITABLE_IFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE ((o), G_TYPE_INITABLE, GInitable))

static void g_memory_monitor_psi_iface_init (GMemoryMonitorInterface *iface);
static void g_memory_monitor_psi_initable_iface_init (GInitableIface *iface);

struct _GMemoryMonitorPsi
{
  GMemoryMonitorBase parent_instance;
  GMainContext *worker;  /* (unowned) */
  GSource *triggers[G_MEMORY_MONITOR_LOW_MEMORY_LEVEL_COUNT];  /* (owned) (nullable) */

  char *cg_path;
  char *proc_path;

  gboolean proc_override;
};

G_DEFINE_TYPE_WITH_CODE (GMemoryMonitorPsi, g_memory_monitor_psi, G_TYPE_MEMORY_MONITOR_BASE,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                g_memory_monitor_psi_initable_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_MEMORY_MONITOR,
                                                g_memory_monitor_psi_iface_init)
                         _g_io_modules_ensure_extension_points_registered ();
                         g_io_extension_point_implement (G_MEMORY_MONITOR_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "psi",
                                                         20))

static void
g_memory_monitor_psi_init (GMemoryMonitorPsi *psi)
{
}

static void
g_memory_monitor_psi_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GMemoryMonitorPsi *monitor = G_MEMORY_MONITOR_PSI (object);

  switch ((GMemoryMonitorPsiProperty) prop_id)
    {
    case PROP_PROC_PATH:
      /* Construct only */
      g_assert (monitor->proc_path == NULL);
      monitor->proc_path = g_value_dup_string (value);
      if (monitor->proc_path != NULL)
        monitor->proc_override = TRUE;
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_memory_monitor_psi_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GMemoryMonitorPsi *monitor = G_MEMORY_MONITOR_PSI (object);

  switch ((GMemoryMonitorPsiProperty) prop_id)
    {
    case PROP_PROC_PATH:
      g_value_set_string (value, monitor->proc_override ? monitor->proc_path : NULL);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
g_memory_monitor_low_trigger_cb (GMemoryMonitorPsi            *monitor,
                                 GMemoryMonitorLowMemoryLevel  level_type,
                                 void                         *user_data)
{
  gdouble mem_ratio;

  /* Should be executed in the worker context */
  g_assert (g_main_context_is_owner (monitor->worker));

  mem_ratio = g_memory_monitor_base_query_mem_ratio ();

  /* if the test is running, skip memory ratio test */
  if (!monitor->proc_override)
    {
      /* if mem free ratio > 0.5, don't signal */
      if (mem_ratio < 0.0)
        return G_SOURCE_REMOVE;
      if (mem_ratio > 0.5)
        return G_SOURCE_CONTINUE;
    }

  g_memory_monitor_base_send_event_to_user (G_MEMORY_MONITOR_BASE (monitor), level_type);

  return G_SOURCE_CONTINUE;
}

static gboolean
event_check (GSource *source)
{
  MemoryMonitorSource *ev_source = (MemoryMonitorSource *) source;

  if (ev_source->pollfd->revents)
    return G_SOURCE_CONTINUE;

  return G_SOURCE_REMOVE;
}

static gboolean
event_dispatch (GSource     *source,
                GSourceFunc  callback,
                gpointer     user_data)
{
  MemoryMonitorSource *ev_source = (MemoryMonitorSource *) source;
  GMemoryMonitorPsi *monitor = NULL;

  monitor = g_weak_ref_get (&ev_source->monitor_weak);
  if (monitor == NULL)
    return G_SOURCE_REMOVE;

  if (monitor->proc_override)
    {
      if (!(g_source_query_unix_fd (source, ev_source->pollfd) & G_IO_IN))
        {
          g_object_unref (monitor);
          return G_SOURCE_CONTINUE;
        }
    }
  else
    {
      if (!(g_source_query_unix_fd (source, ev_source->pollfd) & G_IO_PRI))
        {
          g_object_unref (monitor);
          return G_SOURCE_CONTINUE;
        }
    }

  if (callback)
    ((MemoryMonitorCallbackFunc) callback) (monitor, ev_source->level_type, user_data);

  g_object_unref (monitor);

  return G_SOURCE_CONTINUE;
}

static void
event_finalize (GSource *source)
{
  MemoryMonitorSource *ev_source = (MemoryMonitorSource *) source;

  g_weak_ref_clear (&ev_source->monitor_weak);
}

static GSourceFuncs memory_monitor_event_funcs = {
  .check = event_check,
  .dispatch = event_dispatch,
  .finalize = event_finalize,
};

static GSource *
g_memory_monitor_create_source (GMemoryMonitorPsi            *monitor,
                                int                           fd,
                                GMemoryMonitorLowMemoryLevel  level_type,
                                gboolean                      is_path_override)
{
  MemoryMonitorSource *source;

  source = (MemoryMonitorSource *) g_source_new (&memory_monitor_event_funcs, sizeof (MemoryMonitorSource));
  if (is_path_override)
    source->pollfd = g_source_add_unix_fd ((GSource *) source, fd, G_IO_IN | G_IO_ERR);
  else
    source->pollfd = g_source_add_unix_fd ((GSource *) source, fd, G_IO_PRI | G_IO_ERR);
  source->level_type = level_type;
  g_weak_ref_init (&source->monitor_weak, monitor);

  return (GSource *) source;
}

static gboolean
g_memory_monitor_psi_calculate_mem_pressure_path (GMemoryMonitorPsi  *monitor,
                                                  GError            **error)
{
  pid_t pid;
  gchar *path_read = NULL;
  gchar *replacement = NULL;

  if (!monitor->proc_override)
    {
      pid = getpid ();
      monitor->proc_path = g_strdup_printf ("/proc/%d/cgroup", pid);
    }

  if (!g_file_get_contents (monitor->proc_path, &path_read, NULL, error))
    {
      g_free (path_read);
      return FALSE;
    }

  /* cgroupv2 is only supported and the format is shown as follows:
   * ex: 0::/user.slice/user-0.slice/session-c3.scope */
  if (!g_str_has_prefix (path_read, "0::"))
    {
      g_debug ("Unsupported cgroup path information.");
      g_free (path_read);
      return FALSE;
    }

  /* drop "0::" */
  replacement = g_strdup (path_read + strlen ("0::"));
  g_free (path_read);
  if (replacement == NULL)
    {
      g_debug ("Unsupported cgroup path format.");
      return FALSE;
    }

  replacement = g_strstrip (replacement);

  if (monitor->proc_override)
    {
      monitor->cg_path = g_steal_pointer (&replacement);
      return TRUE;
    }

  monitor->cg_path = g_build_filename ("/sys/fs/cgroup", replacement, "memory.pressure", NULL);
  g_debug ("cgroup path is %s", monitor->cg_path);

  g_free (replacement);
  return g_file_test (monitor->cg_path, G_FILE_TEST_EXISTS);
}

static GSource *
g_memory_monitor_psi_setup_trigger (GMemoryMonitorPsi             *monitor,
                                    GMemoryMonitorLowMemoryLevel   level_type,
                                    int                            threshold_us,
                                    int                            window_us,
                                    GError                       **error)
{
  GSource *source;
  int fd;
  int ret;
  size_t wlen;
  gchar *trigger = NULL;

  fd = g_open (monitor->cg_path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0)
    {
      int errsv = errno;
      g_debug ("Error on opening %s: %s", monitor->cg_path, g_strerror (errsv));
      g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errsv),
                   "Error on opening ‘%s’: %s", monitor->cg_path, g_strerror (errsv));
      return NULL;
    }

  /* The kernel PSI [1] trigger format is:
   * <some|full> <stall amount in us> <time window in us>
   * The “some” indicates the share of time in which at least some tasks are stalled on a given resource.
   * The “full” indicates the share of time in which all non-idle tasks are stalled on a given resource simultaneously.
   * "stall amount in us": The total stall time in us.
   * "time window in us": The specific time window in us.
   * e.g. writing "some 150000 1000000" would add 150ms threshold for partial memory stall measured within 1sec time window
   *
   * [1] https://docs.kernel.org/accounting/psi.html
   */
  trigger = g_strdup_printf ("%s %d %d",
                             (triggers[level_type].trigger_type == MEMORY_PRESSURE_MONITOR_TRIGGER_SOME) ? "some" : "full",
                             threshold_us,
                             window_us);

  errno = 0;
  wlen = strlen (trigger) + 1;
  while (wlen > 0)
    {
      int errsv;
      g_debug ("Write trigger %s", trigger);
      ret = write (fd, trigger, wlen);
      errsv = errno;
      if (ret < 0)
        {
          if (errsv == EINTR)
            {
              /* interrupted by signal, retry */
              continue;
            }
          else
            {
              g_set_error (error,
                           G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Error on setting PSI configurations: %s",
                           g_strerror (errsv));
              g_free (trigger);
              close (fd);
              return NULL;
            }
        }
      wlen -= ret;
    }
  g_free (trigger);

  source = g_memory_monitor_create_source (monitor, fd, level_type, monitor->proc_override);
  g_source_set_callback (source, G_SOURCE_FUNC (g_memory_monitor_low_trigger_cb), NULL, NULL);

  return g_steal_pointer (&source);
}

static gboolean
g_memory_monitor_setup_psi (GMemoryMonitorPsi  *monitor,
                            GError            **error)
{
  if (!g_memory_monitor_psi_calculate_mem_pressure_path (monitor, error))
    return FALSE;

  for (size_t i = 0; i < G_N_ELEMENTS (triggers); i++)
    {
      /* the user defined PSI is estimated per second and the unit is in micro second(us). */
      monitor->triggers[i] = g_memory_monitor_psi_setup_trigger (monitor,
                                                                 i,
                                                                 triggers[i].threshold_ms * 1000,
                                                                 PSI_WINDOW_SEC * 1000 * 1000,
                                                                 error);
      if (monitor->triggers[i] == NULL)
        return FALSE;
    }

  return TRUE;
}

static gboolean
g_memory_monitor_psi_initable_init (GInitable     *initable,
                                    GCancellable  *cancellable,
                                    GError       **error)
{
  GMemoryMonitorPsi *monitor = G_MEMORY_MONITOR_PSI (initable);

  monitor->worker = GLIB_PRIVATE_CALL (g_get_worker_context) ();

  if (g_memory_monitor_setup_psi (monitor, error))
    {
      for (size_t i = 0; i < G_N_ELEMENTS (monitor->triggers); i++)
        {
          g_source_attach (monitor->triggers[i], monitor->worker);
        }
    }
  else
    {
      g_debug ("PSI is not supported.");
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "PSI is not supported.");
      return FALSE;
    }

  return TRUE;
}

static void
g_memory_monitor_psi_finalize (GObject *object)
{
  GMemoryMonitorPsi *monitor = G_MEMORY_MONITOR_PSI (object);

  g_free (monitor->cg_path);
  g_free (monitor->proc_path);

  for (size_t i = 0; i < G_N_ELEMENTS (monitor->triggers); i++)
    {
      g_source_destroy (monitor->triggers[i]);
      g_source_unref (monitor->triggers[i]);
    }

  G_OBJECT_CLASS (g_memory_monitor_psi_parent_class)->finalize (object);
}

static void
g_memory_monitor_psi_class_init (GMemoryMonitorPsiClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = g_memory_monitor_psi_set_property;
  object_class->get_property = g_memory_monitor_psi_get_property;
  object_class->finalize = g_memory_monitor_psi_finalize;

  /**
   * GMemoryMonitorPsi:proc-path: (nullable)
   *
   * Kernel PSI path to use, if not the default.
   *
   * This is typically only used for test purposes.
   *
   * Since: 2.86
   */
  g_object_class_install_property (object_class,
                                   PROP_PROC_PATH,
                                   g_param_spec_string ("proc-path", NULL, NULL,
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
}

static void
g_memory_monitor_psi_iface_init (GMemoryMonitorInterface *monitor_iface)
{
}

static void
g_memory_monitor_psi_initable_iface_init (GInitableIface *iface)
{
  iface->init = g_memory_monitor_psi_initable_init;
}
